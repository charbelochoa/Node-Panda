#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#define NP_CLOSE_SOCK closesocket
#define NP_SHUT_RDWR SD_BOTH
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#define NP_CLOSE_SOCK ::close
#define NP_SHUT_RDWR SHUT_RDWR
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#endif

#include "websocket_rtc.h"
#include "crypto_engine.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <random>
#include <sstream>

namespace nodepanda {

namespace {

constexpr size_t kMaxEvents = 256;
constexpr size_t kMaxHeaderBytes = 16384;
constexpr uint64_t kMaxFramePayload = 16ull * 1024ull * 1024ull;
const char* kWsMagicGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

inline SOCKET toSock(uintptr_t s) { return static_cast<SOCKET>(s); }
inline uintptr_t fromSock(SOCKET s) { return static_cast<uintptr_t>(s); }
const uintptr_t kInvalidSock = static_cast<uintptr_t>(INVALID_SOCKET);

inline uint32_t rotl32(uint32_t v, uint32_t bits) {
    return (v << bits) | (v >> (32u - bits));
}

void sha1Compress(uint32_t h[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = h[0];
    uint32_t b = h[1];
    uint32_t c = h[2];
    uint32_t d = h[3];
    uint32_t e = h[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f = 0;
        uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t temp = rotl32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl32(b, 30);
        b = a;
        a = temp;
    }

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

std::array<uint8_t, 20> sha1Bytes(const uint8_t* data, size_t len) {
    uint32_t h[5] = {
        0x67452301u,
        0xEFCDAB89u,
        0x98BADCFEu,
        0x10325476u,
        0xC3D2E1F0u
    };

    size_t offset = 0;
    while (offset + 64 <= len) {
        sha1Compress(h, data + offset);
        offset += 64;
    }

    uint8_t tail[128];
    std::memset(tail, 0, sizeof(tail));
    size_t rem = len - offset;
    if (rem > 0) {
        std::memcpy(tail, data + offset, rem);
    }
    tail[rem] = 0x80u;

    size_t paddedLen = (rem < 56) ? 64 : 128;
    uint64_t bitLen = static_cast<uint64_t>(len) * 8ull;
    for (int i = 0; i < 8; ++i) {
        tail[paddedLen - 1 - i] = static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFFu);
    }

    sha1Compress(h, tail);
    if (paddedLen == 128) {
        sha1Compress(h, tail + 64);
    }

    std::array<uint8_t, 20> out{};
    for (int i = 0; i < 5; ++i) {
        out[i * 4] = static_cast<uint8_t>((h[i] >> 24) & 0xFFu);
        out[i * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xFFu);
        out[i * 4 + 2] = static_cast<uint8_t>((h[i] >> 8) & 0xFFu);
        out[i * 4 + 3] = static_cast<uint8_t>(h[i] & 0xFFu);
    }
    return out;
}

struct WsFrame {
    bool fin = true;
    uint8_t opcode = 0x1;
    bool masked = false;
    std::string payload;
};

std::vector<uint8_t> encodeWsFrame(uint8_t opcode, const std::string& payload, bool masked) {
    std::vector<uint8_t> out;
    out.reserve(payload.size() + 14);
    out.push_back(static_cast<uint8_t>(0x80u | (opcode & 0x0Fu)));

    uint8_t maskFlag = masked ? 0x80u : 0x00u;
    uint64_t len = static_cast<uint64_t>(payload.size());
    if (len < 126ull) {
        out.push_back(static_cast<uint8_t>(maskFlag | static_cast<uint8_t>(len)));
    } else if (len <= 65535ull) {
        out.push_back(static_cast<uint8_t>(maskFlag | 126u));
        out.push_back(static_cast<uint8_t>((len >> 8) & 0xFFu));
        out.push_back(static_cast<uint8_t>(len & 0xFFu));
    } else {
        out.push_back(static_cast<uint8_t>(maskFlag | 127u));
        for (int i = 7; i >= 0; --i) {
            out.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFFu));
        }
    }

    if (masked) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        uint8_t maskKey[4];
        for (int i = 0; i < 4; ++i) {
            maskKey[i] = static_cast<uint8_t>(dist(gen));
            out.push_back(maskKey[i]);
        }
        for (size_t i = 0; i < payload.size(); ++i) {
            out.push_back(static_cast<uint8_t>(static_cast<uint8_t>(payload[i]) ^ maskKey[i % 4]));
        }
    } else {
        out.insert(out.end(), payload.begin(), payload.end());
    }
    return out;
}

int tryDecodeWsFrame(std::vector<uint8_t>& buffer, WsFrame& outFrame) {
    if (buffer.size() < 2) return 0;

    bool fin = (buffer[0] & 0x80u) != 0;
    uint8_t opcode = static_cast<uint8_t>(buffer[0] & 0x0Fu);
    bool masked = (buffer[1] & 0x80u) != 0;
    uint64_t payloadLen = static_cast<uint64_t>(buffer[1] & 0x7Fu);
    size_t offset = 2;

    if (payloadLen == 126ull) {
        if (buffer.size() < offset + 2) return 0;
        payloadLen = (static_cast<uint64_t>(buffer[offset]) << 8) |
                     static_cast<uint64_t>(buffer[offset + 1]);
        offset += 2;
    } else if (payloadLen == 127ull) {
        if (buffer.size() < offset + 8) return 0;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | static_cast<uint64_t>(buffer[offset + i]);
        }
        offset += 8;
    }

    if (payloadLen > kMaxFramePayload) return -1;

    uint8_t maskKey[4] = {0, 0, 0, 0};
    if (masked) {
        if (buffer.size() < offset + 4) return 0;
        std::memcpy(maskKey, buffer.data() + offset, 4);
        offset += 4;
    }

    if (buffer.size() < offset + static_cast<size_t>(payloadLen)) return 0;

    outFrame.fin = fin;
    outFrame.opcode = opcode;
    outFrame.masked = masked;
    outFrame.payload.resize(static_cast<size_t>(payloadLen));
    for (size_t i = 0; i < static_cast<size_t>(payloadLen); ++i) {
        uint8_t b = buffer[offset + i];
        if (masked) b ^= maskKey[i % 4];
        outFrame.payload[i] = static_cast<char>(b);
    }

    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(offset + static_cast<size_t>(payloadLen)));
    return 1;
}

void setSocketTimeout(SOCKET sock, int timeoutMs) {
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(timeoutMs);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

bool sendAll(SOCKET sock, const uint8_t* data, size_t len) {
    size_t sent = 0;
    int flags = 0;
#ifdef MSG_NOSIGNAL
    flags = MSG_NOSIGNAL;
#endif
    while (sent < len) {
        int chunk = static_cast<int>(std::min<size_t>(len - sent, 65536));
        int n = ::send(sock, reinterpret_cast<const char*>(data + sent), chunk, flags);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::string trimAscii(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string toLowerAscii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string extractHttpHeader(const std::string& headers, const std::string& name) {
    std::string needle = toLowerAscii(name);
    std::istringstream iss(headers);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = toLowerAscii(trimAscii(line.substr(0, colon)));
        if (key == needle) {
            return trimAscii(line.substr(colon + 1));
        }
    }
    return {};
}

bool readHttpHeaderBlock(SOCKET sock, std::string& headersOut, std::vector<uint8_t>& leftoverOut) {
    std::vector<uint8_t> raw;
    raw.reserve(2048);
    char buf[2048];
    const char marker[4] = {'\r', '\n', '\r', '\n'};

    while (raw.size() < kMaxHeaderBytes) {
        int n = ::recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        raw.insert(raw.end(), reinterpret_cast<uint8_t*>(buf), reinterpret_cast<uint8_t*>(buf) + n);

        auto it = std::search(raw.begin(), raw.end(), std::begin(marker), std::end(marker));
        if (it != raw.end()) {
            size_t headerLen = static_cast<size_t>(std::distance(raw.begin(), it));
            headersOut.assign(reinterpret_cast<const char*>(raw.data()), headerLen);
            size_t bodyOffset = headerLen + 4;
            if (bodyOffset < raw.size()) {
                leftoverOut.assign(raw.begin() + static_cast<std::ptrdiff_t>(bodyOffset), raw.end());
            } else {
                leftoverOut.clear();
            }
            return true;
        }
    }
    return false;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) >= 0x20) out.push_back(c);
                break;
        }
    }
    return out;
}

std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size() || json[pos] != '"') return {};
    ++pos;
    std::string out;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '\\' && pos < json.size()) {
            char next = json[pos++];
            if (next == 'n') out.push_back('\n');
            else if (next == 'r') out.push_back('\r');
            else if (next == 't') out.push_back('\t');
            else out.push_back(next);
        } else if (c == '"') {
            break;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

int64_t extractJsonInt64(const std::string& json, const std::string& key, int64_t fallback = 0) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return fallback;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return fallback;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size()) return fallback;
    bool neg = false;
    if (json[pos] == '-') {
        neg = true;
        ++pos;
    }
    if (pos >= json.size() || !std::isdigit(static_cast<unsigned char>(json[pos]))) return fallback;
    int64_t val = 0;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        val = val * 10 + (json[pos] - '0');
        ++pos;
    }
    return neg ? -val : val;
}

int64_t nowUnix() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

std::string nowTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmVal{};
#ifdef _WIN32
    localtime_s(&tmVal, &tt);
#else
    localtime_r(&tt, &tmVal);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tmVal.tm_year + 1900, tmVal.tm_mon + 1, tmVal.tm_mday,
                  tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec);
    return std::string(buf);
}

std::string generateRandomWsKey() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 255);
    std::string raw(16, '\0');
    for (size_t i = 0; i < 16; ++i) {
        raw[i] = static_cast<char>(dist(gen));
    }
    return CryptoEngine::base64Encode(raw);
}

}

struct WebSocketRtcServer::PeerConn {
    RtcPeer info;
    bool isOutbound = false;
    std::atomic<bool> closed{false};
    std::mutex sendMutex;
};

WebSocketRtcServer::WebSocketRtcServer() : m_serverSocket(kInvalidSock) {
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        m_wsaReady = true;
    }
#else
    m_wsaReady = true;
#endif
}

WebSocketRtcServer::~WebSocketRtcServer() {
    m_destroying.store(true);
    stop();
#ifdef _WIN32
    if (m_wsaReady) {
        WSACleanup();
        m_wsaReady = false;
    }
#endif
}

std::string WebSocketRtcServer::computeWebSocketAccept(const std::string& clientKey) {
    std::string combined = trimAscii(clientKey) + kWsMagicGuid;
    auto digest = sha1Bytes(reinterpret_cast<const uint8_t*>(combined.data()), combined.size());
    std::string raw(reinterpret_cast<const char*>(digest.data()), digest.size());
    return CryptoEngine::base64Encode(raw);
}

void WebSocketRtcServer::start(int port, const std::string& bindHost) {
    if (m_running.load()) return;
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    int targetPort = (port > 0 && port <= 65535) ? port : 8043;
    std::string targetHost = bindHost.empty() ? "127.0.0.1" : bindHost;

    SOCKET sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        recordEvent("error", "server", "Failed to create WebSocket listening socket");
        return;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = (targetHost == "0.0.0.0")
        ? INADDR_ANY
        : inet_addr(targetHost.c_str());
    addr.sin_port = htons(static_cast<uint16_t>(targetPort));

    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        NP_CLOSE_SOCK(sock);
        recordEvent("error", "server", "Failed to bind WebSocket server on port " + std::to_string(targetPort));
        return;
    }

    if (::listen(sock, 32) != 0) {
        NP_CLOSE_SOCK(sock);
        recordEvent("error", "server", "Failed to listen on WebSocket server socket");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_port = targetPort;
        m_bindHost = targetHost;
        m_serverSocket = fromSock(sock);
    }

    m_running.store(true);
    recordEvent("server", "local", "WebSocket RTC listening on " + targetHost + ":" + std::to_string(targetPort));
    m_serverThread = std::thread(&WebSocketRtcServer::serverLoop, this);
}

void WebSocketRtcServer::stop() {
    bool wasRunning = m_running.exchange(false);

    uintptr_t serverSock = kInvalidSock;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        serverSock = m_serverSocket;
        m_serverSocket = kInvalidSock;
    }
    if (serverSock != kInvalidSock) {
        ::shutdown(toSock(serverSock), NP_SHUT_RDWR);
        NP_CLOSE_SOCK(toSock(serverSock));
    }

    closeAllPeers();

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    std::vector<WorkerEntry> toJoin;
    {
        std::lock_guard<std::mutex> tlock(m_threadMutex);
        toJoin.swap(m_workers);
    }
    for (auto& w : toJoin) {
        if (w.thread.joinable()) {
            w.thread.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_peers.clear();
    }

    if (wasRunning && !m_destroying.load()) {
        recordEvent("server", "local", "WebSocket RTC server stopped");
    }
}

bool WebSocketRtcServer::isRunning() const {
    return m_running.load();
}

std::string WebSocketRtcServer::getBindHost() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bindHost;
}

void WebSocketRtcServer::setLocalPresence(const std::string& username,
                                          const std::string& noteId,
                                          const std::string& blockUuid) {
    std::string effectiveUser;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!username.empty()) {
            m_localUsername = username;
        }
        m_localNoteId = noteId;
        m_localBlockUuid = blockUuid;
        effectiveUser = m_localUsername;
    }

    std::string status = noteId.empty() ? "online" : "editing";
    std::string payload = "{\"username\":\"" + jsonEscape(effectiveUser) +
                          "\",\"noteId\":\"" + jsonEscape(noteId) +
                          "\",\"blockUuid\":\"" + jsonEscape(blockUuid) +
                          "\",\"status\":\"" + status + "\"}";
    broadcastMessage("presence", payload);
}

void WebSocketRtcServer::broadcastDelta(const std::string& entityId,
                                        const std::string& op,
                                        const std::string& content) {
    int64_t rev = ++m_currentRev;
    std::string localUser;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        localUser = m_localUsername;
    }

    std::string payload = "{\"rev\":" + std::to_string(rev) +
                          ",\"entityId\":\"" + jsonEscape(entityId) +
                          "\",\"op\":\"" + jsonEscape(op) +
                          "\",\"content\":\"" + jsonEscape(content) +
                          "\",\"username\":\"" + jsonEscape(localUser) + "\"}";
    broadcastMessage("tx/batch", payload);
}

void WebSocketRtcServer::broadcastMessage(const std::string& type, const std::string& jsonPayload) {
    int64_t rev = m_currentRev.load();
    std::string trimmed = trimAscii(jsonPayload);
    std::string wire;

    if (trimmed.empty() || trimmed == "{}") {
        wire = "{\"type\":\"" + jsonEscape(type) + "\",\"rev\":" + std::to_string(rev) + "}";
    } else if (trimmed.front() == '{' && trimmed.back() == '}') {
        if (trimmed.find("\"type\"") != std::string::npos) {
            wire = trimmed;
        } else {
            wire = "{\"type\":\"" + jsonEscape(type) + "\",\"rev\":" + std::to_string(rev) + "," + trimmed.substr(1);
        }
    } else {
        wire = "{\"type\":\"" + jsonEscape(type) + "\",\"rev\":" + std::to_string(rev) +
               ",\"payload\":\"" + jsonEscape(trimmed) + "\"}";
    }

    std::string localUser;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        localUser = m_localUsername;
    }
    broadcastRawText(wire);
    recordEvent(type.empty() ? "message" : type, localUser, wire);
}

bool WebSocketRtcServer::connectRemotePeer(const std::string& host,
                                           int port,
                                           const std::string& username,
                                           std::string& errorOut) {
    errorOut.clear();
    if (host.empty() || port <= 0 || port > 65535) {
        errorOut = "Invalid remote host or port";
        return false;
    }

    reapFinishedWorkers();

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0 || !result) {
        errorOut = "Unable to resolve host: " + host;
        return false;
    }

    SOCKET sock = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        ::freeaddrinfo(result);
        errorOut = "Failed to create client socket";
        return false;
    }

    setSocketTimeout(sock, 3000);
    if (::connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) != 0) {
        ::freeaddrinfo(result);
        NP_CLOSE_SOCK(sock);
        errorOut = "Connection refused by " + host + ":" + portStr;
        return false;
    }
    ::freeaddrinfo(result);

    std::string wsKey = generateRandomWsKey();
    std::string req = "GET / HTTP/1.1\r\n"
                      "Host: " + host + ":" + portStr + "\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Key: " + wsKey + "\r\n"
                      "Sec-WebSocket-Version: 13\r\n\r\n";

    if (!sendAll(sock, reinterpret_cast<const uint8_t*>(req.data()), req.size())) {
        NP_CLOSE_SOCK(sock);
        errorOut = "Failed to send WebSocket upgrade request";
        return false;
    }

    std::string respHeaders;
    std::vector<uint8_t> leftover;
    if (!readHttpHeaderBlock(sock, respHeaders, leftover)) {
        NP_CLOSE_SOCK(sock);
        errorOut = "No WebSocket handshake response from peer";
        return false;
    }

    size_t firstLineEnd = respHeaders.find("\r\n");
    std::string statusLine = (firstLineEnd != std::string::npos) ? respHeaders.substr(0, firstLineEnd) : respHeaders;
    if (statusLine.find("101") == std::string::npos) {
        NP_CLOSE_SOCK(sock);
        errorOut = "Remote peer rejected WebSocket upgrade";
        return false;
    }

    std::string acceptHeader = extractHttpHeader(respHeaders, "Sec-WebSocket-Accept");
    std::string expectedAccept = computeWebSocketAccept(wsKey);
    if (acceptHeader != expectedAccept) {
        NP_CLOSE_SOCK(sock);
        errorOut = "Invalid Sec-WebSocket-Accept from remote peer";
        return false;
    }

    std::string peerId = "remote-" + std::to_string(++m_peerSeq);
    auto peer = std::make_shared<PeerConn>();
    peer->isOutbound = true;
    peer->info.peerId = peerId;
    peer->info.username = username.empty() ? (host + ":" + portStr) : username;
    peer->info.status = "online";
    peer->info.lastSeenUnix = nowUnix();
    peer->info.socketHandle = fromSock(sock);

    std::string localUser;
    std::string localNote;
    std::string localBlock;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_peers.push_back(peer);
        localUser = username.empty() ? m_localUsername : username;
        localNote = m_localNoteId;
        localBlock = m_localBlockUuid;
    }

    std::string helloJson = "{\"type\":\"hello\",\"peerId\":\"" + jsonEscape(peerId) +
                            "\",\"username\":\"" + jsonEscape(localUser) +
                            "\",\"noteId\":\"" + jsonEscape(localNote) +
                            "\",\"blockUuid\":\"" + jsonEscape(localBlock) +
                            "\",\"rev\":" + std::to_string(m_currentRev.load()) + "}";
    sendFrameToPeer(peer, 0x1, helloJson);
    recordEvent("connect", peer->info.username, "Connected to " + host + ":" + portStr);

    auto doneFlag = std::make_shared<std::atomic<bool>>(false);
    {
        std::lock_guard<std::mutex> tlock(m_threadMutex);
        WorkerEntry entry;
        entry.done = doneFlag;
        entry.thread = std::thread(&WebSocketRtcServer::peerLoop, this, peer, std::move(leftover), doneFlag);
        m_workers.push_back(std::move(entry));
    }

    return true;
}

std::vector<RtcPeer> WebSocketRtcServer::listPeers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RtcPeer> out;
    out.reserve(m_peers.size());
    for (const auto& p : m_peers) {
        if (!p->closed.load()) {
            out.push_back(p->info);
        }
    }
    return out;
}

std::vector<RtcEvent> WebSocketRtcServer::recentEvents(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_events.empty() || maxCount == 0) return {};
    if (m_events.size() <= maxCount) return m_events;
    return std::vector<RtcEvent>(m_events.end() - static_cast<std::ptrdiff_t>(maxCount), m_events.end());
}

RtcStats WebSocketRtcServer::stats() const {
    RtcStats s;
    std::lock_guard<std::mutex> lock(m_mutex);
    s.serverRunning = m_running.load();
    s.port = m_port;
    int active = 0;
    for (const auto& p : m_peers) {
        if (!p->closed.load()) ++active;
    }
    s.connectedPeers = active;
    s.messagesSent = m_messagesSent.load();
    s.messagesReceived = m_messagesReceived.load();
    s.currentRev = m_currentRev.load();
    return s;
}

void WebSocketRtcServer::serverLoop() {
    while (m_running.load()) {
        uintptr_t listenSock = kInvalidSock;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            listenSock = m_serverSocket;
        }
        if (listenSock == kInvalidSock) break;

        SOCKET sock = toSock(listenSock);
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 80000;

        int sel = ::select(static_cast<int>(sock) + 1, &readfds, nullptr, nullptr, &tv);
        if (sel <= 0) {
            reapFinishedWorkers();
            continue;
        }

        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        SOCKET clientSock = ::accept(sock, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientSock == INVALID_SOCKET) {
            continue;
        }

        setSocketTimeout(clientSock, 3000);
        std::vector<uint8_t> leftover;
        std::string userHint;
        if (!performServerHandshake(fromSock(clientSock), leftover, userHint)) {
            NP_CLOSE_SOCK(clientSock);
            continue;
        }

        std::string peerId = "peer-" + std::to_string(++m_peerSeq);
        auto peer = std::make_shared<PeerConn>();
        peer->isOutbound = false;
        peer->info.peerId = peerId;
        peer->info.username = userHint.empty() ? peerId : userHint;
        peer->info.status = "online";
        peer->info.lastSeenUnix = nowUnix();
        peer->info.socketHandle = fromSock(clientSock);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_peers.push_back(peer);
        }

        recordEvent("connect", peer->info.username, "Inbound WebSocket peer joined (" + peerId + ")");

        auto doneFlag = std::make_shared<std::atomic<bool>>(false);
        {
            std::lock_guard<std::mutex> tlock(m_threadMutex);
            WorkerEntry entry;
            entry.done = doneFlag;
            entry.thread = std::thread(&WebSocketRtcServer::peerLoop, this, peer, std::move(leftover), doneFlag);
            m_workers.push_back(std::move(entry));
        }

        reapFinishedWorkers();
    }
}

bool WebSocketRtcServer::performServerHandshake(uintptr_t sockHandle,
                                                std::vector<uint8_t>& leftover,
                                                std::string& peerUserHint) {
    SOCKET sock = toSock(sockHandle);
    std::string reqHeaders;
    if (!readHttpHeaderBlock(sock, reqHeaders, leftover)) {
        return false;
    }

    std::string wsKey = extractHttpHeader(reqHeaders, "Sec-WebSocket-Key");
    if (wsKey.empty()) {
        std::string bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        sendAll(sock, reinterpret_cast<const uint8_t*>(bad.data()), bad.size());
        return false;
    }

    peerUserHint = extractHttpHeader(reqHeaders, "X-NodePanda-User");
    std::string acceptKey = computeWebSocketAccept(wsKey);
    std::string resp = "HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
    return sendAll(sock, reinterpret_cast<const uint8_t*>(resp.data()), resp.size());
}

void WebSocketRtcServer::peerLoop(std::shared_ptr<PeerConn> peer,
                                  std::vector<uint8_t> initialBytes,
                                  std::shared_ptr<std::atomic<bool>> doneFlag) {
    SOCKET sock = toSock(peer->info.socketHandle);
    std::vector<uint8_t> buffer = std::move(initialBytes);
    std::string fragPayload;
    bool inFragment = false;
    uint8_t fragOpcode = 0x1;

    auto processFrames = [&]() -> bool {
        while (true) {
            WsFrame frame;
            int res = tryDecodeWsFrame(buffer, frame);
            if (res == 0) return true;
            if (res < 0) return false;

            if (frame.opcode == 0x8) {
                sendFrameToPeer(peer, 0x8, "");
                return false;
            }
            if (frame.opcode == 0x9) {
                sendFrameToPeer(peer, 0xA, frame.payload);
                std::lock_guard<std::mutex> lock(m_mutex);
                peer->info.lastSeenUnix = nowUnix();
                continue;
            }
            if (frame.opcode == 0xA) {
                std::lock_guard<std::mutex> lock(m_mutex);
                peer->info.lastSeenUnix = nowUnix();
                continue;
            }

            if (frame.opcode == 0x1 || frame.opcode == 0x2) {
                if (frame.fin) {
                    inFragment = false;
                    fragPayload.clear();
                    ++m_messagesReceived;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        peer->info.lastSeenUnix = nowUnix();
                    }
                    if (frame.opcode == 0x1) {
                        handleIncomingText(peer, frame.payload);
                    }
                } else {
                    inFragment = true;
                    fragOpcode = frame.opcode;
                    fragPayload = frame.payload;
                }
            } else if (frame.opcode == 0x0 && inFragment) {
                fragPayload += frame.payload;
                if (frame.fin) {
                    inFragment = false;
                    ++m_messagesReceived;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        peer->info.lastSeenUnix = nowUnix();
                    }
                    if (fragOpcode == 0x1) {
                        handleIncomingText(peer, fragPayload);
                    }
                    fragPayload.clear();
                }
            }
        }
    };

    bool ok = processFrames();
    char chunk[4096];

    while (ok && !peer->closed.load() && !m_destroying.load()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int sel = ::select(static_cast<int>(sock) + 1, &readfds, nullptr, nullptr, &tv);
        if (sel < 0) break;
        if (sel == 0) continue;

        int n = ::recv(sock, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(chunk), reinterpret_cast<uint8_t*>(chunk) + n);
        ok = processFrames();
    }

    if (!peer->closed.exchange(true)) {
        ::shutdown(sock, NP_SHUT_RDWR);
        NP_CLOSE_SOCK(sock);
    }

    std::string peerName;
    std::string peerId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        peerName = peer->info.username;
        peerId = peer->info.peerId;
    }
    removePeer(peerId);
    if (!m_destroying.load()) {
        recordEvent("disconnect", peerName, "Peer disconnected (" + peerId + ")");
    }
    doneFlag->store(true);
}

void WebSocketRtcServer::handleIncomingText(const std::shared_ptr<PeerConn>& peer,
                                            const std::string& text) {
    std::string msgType = extractJsonString(text, "type");
    if (msgType.empty()) msgType = "message";

    std::string peerName;
    std::string peerId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        peerName = peer->info.username;
        peerId = peer->info.peerId;
    }

    if (msgType == "hello") {
        std::string user = extractJsonString(text, "username");
        std::string noteId = extractJsonString(text, "noteId");
        std::string blockUuid = extractJsonString(text, "blockUuid");
        int64_t remoteRev = extractJsonInt64(text, "rev", 0);

        std::string localUser;
        std::string localNote;
        std::string localBlock;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!user.empty()) peer->info.username = user;
            if (!noteId.empty()) peer->info.editingNoteId = noteId;
            if (!blockUuid.empty()) peer->info.editingBlockUuid = blockUuid;
            peer->info.status = noteId.empty() ? "online" : "editing";
            peer->info.lastSeenUnix = nowUnix();
            peerName = peer->info.username;
            localUser = m_localUsername;
            localNote = m_localNoteId;
            localBlock = m_localBlockUuid;
        }
        int64_t curRev = m_currentRev.load();
        if (remoteRev > curRev) {
            m_currentRev.store(remoteRev);
        }
        recordEvent("hello", peerName, text);

        if (!peer->isOutbound) {
            std::string reply = "{\"type\":\"hello\",\"peerId\":\"local\",\"username\":\"" + jsonEscape(localUser) +
                                "\",\"noteId\":\"" + jsonEscape(localNote) +
                                "\",\"blockUuid\":\"" + jsonEscape(localBlock) +
                                "\",\"rev\":" + std::to_string(m_currentRev.load()) + "}";
            sendFrameToPeer(peer, 0x1, reply);
        }
        return;
    }

    if (msgType == "ping") {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            peer->info.lastSeenUnix = nowUnix();
        }
        recordEvent("ping", peerName, text);
        std::string pong = "{\"type\":\"pong\",\"rev\":" + std::to_string(m_currentRev.load()) +
                           ",\"ts\":" + std::to_string(nowUnix()) + "}";
        sendFrameToPeer(peer, 0x1, pong);
        return;
    }

    if (msgType == "pong") {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            peer->info.lastSeenUnix = nowUnix();
            peer->info.status = "online";
        }
        recordEvent("pong", peerName, text);
        return;
    }

    if (msgType == "presence") {
        std::string user = extractJsonString(text, "username");
        std::string noteId = extractJsonString(text, "noteId");
        std::string blockUuid = extractJsonString(text, "blockUuid");
        std::string status = extractJsonString(text, "status");
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!user.empty()) peer->info.username = user;
            peer->info.editingNoteId = noteId;
            peer->info.editingBlockUuid = blockUuid;
            peer->info.status = !status.empty() ? status : (noteId.empty() ? "online" : "editing");
            peer->info.lastSeenUnix = nowUnix();
            peerName = peer->info.username;
        }
        recordEvent("presence", peerName, text);
        if (!peer->isOutbound) {
            broadcastRawText(text, peerId);
        }
        return;
    }

    if (msgType == "tx/batch") {
        int64_t newRev = ++m_currentRev;
        std::string entityId = extractJsonString(text, "entityId");
        std::string op = extractJsonString(text, "op");
        std::string content = extractJsonString(text, "content");
        recordEvent("tx/batch", peerName, text);

        std::string changed = "{\"type\":\"changed\",\"rev\":" + std::to_string(newRev) +
                              ",\"entityId\":\"" + jsonEscape(entityId) +
                              "\",\"op\":\"" + jsonEscape(op) +
                              "\",\"content\":\"" + jsonEscape(content) +
                              "\",\"peer\":\"" + jsonEscape(peerName) + "\"}";
        if (!peer->isOutbound) {
            broadcastRawText(changed);
        }
        return;
    }

    if (msgType == "changed") {
        int64_t incomingRev = extractJsonInt64(text, "rev", 0);
        int64_t curRev = m_currentRev.load();
        if (incomingRev > curRev) {
            m_currentRev.store(incomingRev);
        } else {
            ++m_currentRev;
        }
        recordEvent("changed", peerName, text);
        if (!peer->isOutbound) {
            broadcastRawText(text, peerId);
        }
        return;
    }

    recordEvent(msgType, peerName, text);
}

bool WebSocketRtcServer::sendFrameToPeer(const std::shared_ptr<PeerConn>& peer,
                                         uint8_t opcode,
                                         const std::string& payload) {
    if (!peer || peer->closed.load()) return false;
    auto frameBytes = encodeWsFrame(opcode, payload, peer->isOutbound);
    std::lock_guard<std::mutex> slock(peer->sendMutex);
    if (peer->closed.load()) return false;
    bool ok = sendAll(toSock(peer->info.socketHandle), frameBytes.data(), frameBytes.size());
    if (ok && (opcode == 0x1 || opcode == 0x2)) {
        ++m_messagesSent;
    }
    return ok;
}

void WebSocketRtcServer::broadcastRawText(const std::string& text, const std::string& excludePeerId) {
    std::vector<std::shared_ptr<PeerConn>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snapshot = m_peers;
    }
    for (const auto& p : snapshot) {
        if (!p || p->closed.load()) continue;
        if (!excludePeerId.empty() && p->info.peerId == excludePeerId) continue;
        sendFrameToPeer(p, 0x1, text);
    }
}

void WebSocketRtcServer::removePeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_peers.erase(
        std::remove_if(m_peers.begin(), m_peers.end(),
                       [&](const std::shared_ptr<PeerConn>& p) {
                           return !p || p->info.peerId == peerId;
                       }),
        m_peers.end());
}

void WebSocketRtcServer::recordEvent(const std::string& type,
                                     const std::string& peer,
                                     const std::string& payload) {
    RtcEvent ev;
    ev.id = "evt-" + std::to_string(++m_eventSeq);
    ev.type = type;
    ev.peer = peer;
    ev.payload = payload;
    ev.timestamp = nowTimestamp();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.push_back(std::move(ev));
    if (m_events.size() > kMaxEvents) {
        m_events.erase(m_events.begin(), m_events.begin() + static_cast<std::ptrdiff_t>(m_events.size() - kMaxEvents));
    }
}

void WebSocketRtcServer::reapFinishedWorkers() {
    std::vector<WorkerEntry> finished;
    {
        std::lock_guard<std::mutex> tlock(m_threadMutex);
        auto it = m_workers.begin();
        while (it != m_workers.end()) {
            if (it->done && it->done->load()) {
                finished.push_back(std::move(*it));
                it = m_workers.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& w : finished) {
        if (w.thread.joinable()) {
            w.thread.join();
        }
    }
}

void WebSocketRtcServer::closeAllPeers() {
    std::vector<std::shared_ptr<PeerConn>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snapshot = m_peers;
    }
    for (const auto& p : snapshot) {
        if (!p) continue;
        if (!p->closed.exchange(true)) {
            SOCKET s = toSock(p->info.socketHandle);
            ::shutdown(s, NP_SHUT_RDWR);
            NP_CLOSE_SOCK(s);
        }
    }
}

}
