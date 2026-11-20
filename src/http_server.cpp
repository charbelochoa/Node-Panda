#ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCK closesocket
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSE_SOCK ::close
    typedef int SOCKET;
    #define INVALID_SOCKET (-1)
#endif

#include "http_server.h"
#include "app.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace nodepanda {

static inline SOCKET toSock(uintptr_t s) { return (SOCKET)s; }
static inline uintptr_t fromSock(SOCKET s) { return (uintptr_t)s; }
static const uintptr_t kInvalidSock = (uintptr_t)INVALID_SOCKET;

HttpServer::HttpServer() : m_serverSocket(kInvalidSock) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start(App* app, int port) {
    if (m_running.load()) return;

    m_app = app;
    m_port = port;
    m_running.store(true);
    m_thread = std::thread(&HttpServer::serverLoop, this);
}

void HttpServer::stop() {
    if (!m_running.load()) return;

    m_running.store(false);
    if (m_serverSocket != kInvalidSock) {
        CLOSE_SOCK(toSock(m_serverSocket));
        m_serverSocket = kInvalidSock;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void HttpServer::serverLoop() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        m_running.store(false);
        return;
    }
#endif

    SOCKET sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        m_running.store(false);
        return;
    }
    m_serverSocket = fromSock(sock);

    int opt = 1;
#ifdef _WIN32
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons((u_short)m_port);

    if (::bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        CLOSE_SOCK(sock);
        m_serverSocket = kInvalidSock;
#ifdef _WIN32
        WSACleanup();
#endif
        m_running.store(false);
        return;
    }

    ::listen(sock, 5);

    while (m_running.load()) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

#ifdef _WIN32
        u_long iMode = 1;
        ioctlsocket(sock, FIONBIO, &iMode);
        SOCKET clientSocket = ::accept(sock, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            Sleep(100);
            continue;
        }
#else
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        int sel = ::select(sock + 1, &readfds, nullptr, nullptr, &timeout);
        if (sel <= 0) continue;

        SOCKET clientSocket = ::accept(sock, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) continue;
#endif

        char buffer[4096] = {0};
        int bytesRead = ::recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string request(buffer);

            size_t firstSpace = request.find(' ');
            size_t secondSpace = request.find(' ', firstSpace + 1);
            if (firstSpace != std::string::npos && secondSpace != std::string::npos) {
                std::string method = request.substr(0, firstSpace);
                std::string path = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);

                std::string reqBody;
                size_t bodyStart = request.find("\r\n\r\n");
                if (bodyStart != std::string::npos) {
                    reqBody = request.substr(bodyStart + 4);
                }

                std::string response = handleRequest(method, path, reqBody);
                ::send(clientSocket, response.c_str(), (int)response.length(), 0);
            }
        }

        CLOSE_SOCK(clientSocket);
    }

    CLOSE_SOCK(sock);
    m_serverSocket = kInvalidSock;

#ifdef _WIN32
    WSACleanup();
#endif
}

std::string HttpServer::handleRequest(const std::string& method, const std::string& path,
                                      const std::string& body) {
    if (method != "GET") {
        return httpResponse(405, "{\"error\": \"Method not allowed\"}");
    }

    size_t queryPos = path.find('?');
    std::string pathOnly = (queryPos != std::string::npos) ? path.substr(0, queryPos) : path;

    if (pathOnly == "/api/notas") {
        return httpResponse(200, buildJsonNoteList());
    } else if (pathOnly.substr(0, 11) == "/api/notas/") {
        std::string id = pathOnly.substr(11);
        return httpResponse(200, buildJsonNote(id));
    } else if (pathOnly == "/api/grafo") {
        return httpResponse(200, buildJsonGraph());
    } else if (pathOnly == "/api/memoria/buscar") {
        std::string query = extractParam(path, "q");
        std::string nStr = extractParam(path, "n");
        int topN = nStr.empty() ? 10 : std::stoi(nStr);
        return httpResponse(200, buildJsonSearch(query, topN));
    } else if (pathOnly == "/api/stats") {
        if (!m_app) {
            return httpResponse(500, "{\"error\": \"App not initialized\"}");
        }

        auto& notes = m_app->getNoteManager().getAllNotes();
        int totalNotes = notes.size();
        int totalLinks = 0;
        for (const auto& [id, note] : notes) {
            totalLinks += note.links.size();
        }

        std::ostringstream oss;
        oss << "{\"total_notas\": " << totalNotes << ", \"total_enlaces\": " << totalLinks << "}";
        return httpResponse(200, oss.str());
    } else {
        return httpResponse(404, "{\"error\": \"Not found\"}");
    }
}

std::string HttpServer::buildJsonNoteList() {
    if (!m_app) {
        return "[]";
    }

    auto& notes = m_app->getNoteManager().getAllNotes();
    std::ostringstream oss;
    oss << "[";

    bool first = true;
    for (const auto& [id, note] : notes) {
        if (!first) oss << ", ";
        first = false;

        std::string escapedTitle = note.title;
        size_t pos = 0;
        while ((pos = escapedTitle.find('"', pos)) != std::string::npos) {
            escapedTitle.replace(pos, 1, "\\\"");
            pos += 2;
        }

        oss << "{\"id\": \"" << id << "\", \"titulo\": \"" << escapedTitle
            << "\", \"palabras\": " << note.content.length()
            << ", \"enlaces_count\": " << note.links.size() << "}";
    }

    oss << "]";
    return oss.str();
}

std::string HttpServer::buildJsonNote(const std::string& id) {
    if (!m_app) {
        return "{}";
    }

    const Note* note = m_app->getNoteManager().getNoteById(id);
    if (!note) {
        return "{}";
    }

    std::ostringstream oss;

    std::string escapedTitle = note->title;
    size_t pos = 0;
    while ((pos = escapedTitle.find('"', pos)) != std::string::npos) {
        escapedTitle.replace(pos, 1, "\\\"");
        pos += 2;
    }

    std::string escapedContent = note->content;
    pos = 0;
    while ((pos = escapedContent.find('"', pos)) != std::string::npos) {
        escapedContent.replace(pos, 1, "\\\"");
        pos += 2;
    }
    pos = 0;
    while ((pos = escapedContent.find('\n', pos)) != std::string::npos) {
        escapedContent.replace(pos, 1, "\\n");
        pos += 2;
    }
    pos = 0;
    while ((pos = escapedContent.find('\r', pos)) != std::string::npos) {
        escapedContent.replace(pos, 1, "\\r");
        pos += 2;
    }
    pos = 0;
    while ((pos = escapedContent.find('\t', pos)) != std::string::npos) {
        escapedContent.replace(pos, 1, "\\t");
        pos += 2;
    }

    oss << "{\"id\": \"" << id << "\", \"titulo\": \"" << escapedTitle
        << "\", \"contenido\": \"" << escapedContent << "\", \"enlaces\": [";

    bool first = true;
    for (const auto& link : note->links) {
        if (!first) oss << ", ";
        first = false;
        oss << "\"" << link << "\"";
    }

    oss << "], \"frontmatter\": {";

    first = true;
    for (const auto& [key, value] : note->frontmatter) {
        if (!first) oss << ", ";
        first = false;

        std::string escapedKey = key;
        pos = 0;
        while ((pos = escapedKey.find('"', pos)) != std::string::npos) {
            escapedKey.replace(pos, 1, "\\\"");
            pos += 2;
        }

        std::string escapedValue = value;
        pos = 0;
        while ((pos = escapedValue.find('"', pos)) != std::string::npos) {
            escapedValue.replace(pos, 1, "\\\"");
            pos += 2;
        }

        oss << "\"" << escapedKey << "\": \"" << escapedValue << "\"";
    }

    oss << "}}";
    return oss.str();
}

std::string HttpServer::buildJsonGraph() {
    if (!m_app) {
        return "{\"nodos\": [], \"aristas\": []}";
    }

    const auto& graph = m_app->getGraph();
    const auto& nodes = graph.getNodes();
    const auto& edges = graph.getEdges();

    std::ostringstream oss;
    oss << "{\"nodos\": [";

    bool first = true;
    for (const auto& node : nodes) {
        if (!first) oss << ", ";
        first = false;
        oss << "{\"id\": \"" << node.noteId << "\", \"tipo\": \"" << node.nodeType
            << "\", \"conexiones\": " << node.connectionCount << "}";
    }

    oss << "], \"aristas\": [";

    first = true;
    for (const auto& edge : edges) {
        if (!first) oss << ", ";
        first = false;
        oss << "{\"desde\": \"" << edge.from << "\", \"hasta\": \"" << edge.to << "\"}";
    }

    oss << "]}";
    return oss.str();
}

std::string HttpServer::buildJsonSearch(const std::string& query, int topN) {
    if (!m_app) {
        return "[]";
    }

    const auto& results = m_app->getMemoryEngine().search(query, topN);

    std::ostringstream oss;
    oss << "[";

    bool first = true;
    for (const auto& result : results) {
        if (!first) oss << ", ";
        first = false;

        std::string escapedTitle = result.title;
        size_t pos = 0;
        while ((pos = escapedTitle.find('"', pos)) != std::string::npos) {
            escapedTitle.replace(pos, 1, "\\\"");
            pos += 2;
        }

        oss << "{\"id\": \"" << result.noteId << "\", \"titulo\": \"" << escapedTitle
            << "\", \"score\": " << std::fixed << std::setprecision(2) << result.score << "}";
    }

    oss << "]";
    return oss.str();
}

std::string HttpServer::httpResponse(int code, const std::string& body,
                                      const std::string& contentType) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " OK\r\n"
        << "Content-Type: " << contentType << "; charset=utf-8\r\n"
        << "Content-Length: " << body.length() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

std::string HttpServer::urlDecode(const std::string& str) {
    std::ostringstream oss;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            std::string hex = str.substr(i + 1, 2);
            int value = std::stoi(hex, nullptr, 16);
            oss << static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            oss << ' ';
        } else {
            oss << str[i];
        }
    }
    return oss.str();
}

std::string HttpServer::extractParam(const std::string& path, const std::string& param) {
    size_t queryStart = path.find('?');
    if (queryStart == std::string::npos) {
        return "";
    }

    std::string query = path.substr(queryStart + 1);
    std::string searchStr = param + "=";
    size_t paramStart = query.find(searchStr);

    if (paramStart == std::string::npos) {
        return "";
    }

    size_t valueStart = paramStart + searchStr.length();
    size_t valueEnd = query.find('&', valueStart);
    if (valueEnd == std::string::npos) {
        valueEnd = query.length();
    }

    std::string value = query.substr(valueStart, valueEnd - valueStart);
    return urlDecode(value);
}

}
