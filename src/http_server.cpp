#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSE_SOCK closesocket
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCK ::close
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#endif

#include "http_server.h"
#include "app.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace nodepanda {

static inline SOCKET toSock(uintptr_t s) { return static_cast<SOCKET>(s); }
static inline uintptr_t fromSock(SOCKET s) { return static_cast<uintptr_t>(s); }
static const uintptr_t kInvalidSock = static_cast<uintptr_t>(INVALID_SOCKET);

HttpServer::HttpServer() : m_serverSocket(kInvalidSock) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start(App* app, int port, const std::string& bindHost) {
    if (m_running.load()) return;
    m_app = app;
    m_port = port;
    m_bindHost = bindHost.empty() ? "127.0.0.1" : bindHost;
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
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = (m_bindHost == "0.0.0.0")
        ? INADDR_ANY
        : inet_addr(m_bindHost.c_str());
    serverAddr.sin_port = htons(static_cast<u_short>(m_port));

    if (::bind(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) != 0) {
        CLOSE_SOCK(sock);
        m_serverSocket = kInvalidSock;
#ifdef _WIN32
        WSACleanup();
#endif
        m_running.store(false);
        return;
    }

    ::listen(sock, 16);

    while (m_running.load()) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

#ifdef _WIN32
        u_long iMode = 1;
        ioctlsocket(sock, FIONBIO, &iMode);
        SOCKET clientSocket = ::accept(sock, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            Sleep(50);
            continue;
        }
        u_long blockMode = 0;
        ioctlsocket(clientSocket, FIONBIO, &blockMode);
        DWORD rcvTimeout = 3000;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rcvTimeout), sizeof(rcvTimeout));
#else
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 80000;
        int sel = ::select(sock + 1, &readfds, nullptr, nullptr, &timeout);
        if (sel <= 0) continue;
        SOCKET clientSocket = ::accept(sock, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) continue;
#endif

        std::string request;
        char buffer[8192];
        int bytesRead = ::recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            request.assign(buffer, static_cast<size_t>(bytesRead));

            size_t firstSpace = request.find(' ');
            size_t secondSpace = request.find(' ', firstSpace + 1);
            if (firstSpace != std::string::npos && secondSpace != std::string::npos) {
                std::string method = request.substr(0, firstSpace);
                std::string path = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                std::string reqHeaders;
                std::string reqBody;
                size_t bodyStart = request.find("\r\n\r\n");
                if (bodyStart != std::string::npos) {
                    reqHeaders = request.substr(0, bodyStart);
                    reqBody = request.substr(bodyStart + 4);
                }
                std::string response = handleRequest(method, path, reqHeaders, reqBody);
                ::send(clientSocket, response.c_str(), static_cast<int>(response.length()), 0);
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

std::string HttpServer::jsonEscape(const std::string& s) {
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

std::string HttpServer::extractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
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

std::string HttpServer::handleRequest(const std::string& method, const std::string& path,
                                      const std::string& headers, const std::string& body) {
    (void)headers;
    if (method == "OPTIONS") {
        return httpResponse(200, "{}");
    }

    size_t queryPos = path.find('?');
    std::string pathOnly = (queryPos != std::string::npos) ? path.substr(0, queryPos) : path;

    if (method == "GET") {
        if (pathOnly == "/api/health" || pathOnly == "/healthz") {
            return httpResponse(200, buildJsonHealth());
        }
        if (pathOnly == "/api/snapshot") {
            return httpResponse(200, buildJsonSnapshot());
        }
        if (pathOnly == "/api/notas") {
            return httpResponse(200, buildJsonNoteList());
        }
        if (pathOnly.rfind("/api/notas/", 0) == 0) {
            return httpResponse(200, buildJsonNote(urlDecode(pathOnly.substr(11))));
        }
        if (pathOnly == "/api/grafo") {
            return httpResponse(200, buildJsonGraph());
        }
        if (pathOnly == "/api/memoria/buscar") {
            std::string query = extractParam(path, "q");
            std::string nStr = extractParam(path, "n");
            int topN = 10;
            if (!nStr.empty()) { try { topN = std::stoi(nStr); } catch (...) {} }
            return httpResponse(200, buildJsonSearch(query, topN));
        }
        if (pathOnly == "/api/stats") {
            if (!m_app) return httpResponse(500, "{\"error\":\"App not initialized\"}");
            auto& notes = m_app->getNoteManager().getAllNotes();
            size_t totalLinks = 0;
            for (const auto& [id, note] : notes) totalLinks += note.links.size();
            std::ostringstream oss;
            oss << "{\"total_notas\":" << notes.size()
                << ",\"total_enlaces\":" << totalLinks
                << ",\"total_bloques\":" << m_app->getOutlinerEngine().getAllBlocks().size()
                << ",\"total_tareas\":" << m_app->getTaskEngine().getTasks().size()
                << ",\"total_datoms\":" << m_app->getDatalogEngine().datomCount()
                << "}";
            return httpResponse(200, oss.str());
        }
        if (pathOnly == "/api/v1/blocks") {
            return httpResponse(200, buildJsonBlocks(extractParam(path, "page")));
        }
        if (pathOnly == "/api/v1/tasks") {
            return httpResponse(200, buildJsonTasks());
        }
        if (pathOnly == "/api/runs") {
            return httpResponse(200, buildJsonRuns());
        }
        if (pathOnly == "/api/messages") {
            std::string room = extractParam(path, "room");
            return httpResponse(200, buildJsonMessages(room.empty() ? "general" : room));
        }
        if (pathOnly == "/api/skills") {
            return httpResponse(200, buildJsonSkills());
        }
        if (pathOnly == "/api/db/status") {
            return httpResponse(200, buildJsonDbStatus());
        }
        if (pathOnly == "/api/v1/sync") {
            return httpResponse(200, buildJsonSyncStatus());
        }
        return httpResponse(404, "{\"error\":\"Not found\"}");
    }

    if (method == "POST") {
        if (pathOnly == "/api/auth/login") return handleAuthLogin(body);
        if (pathOnly == "/api/auth/register") return handleAuthRegister(body);
        if (pathOnly == "/api/runs") return handleCreateRun(body);
        if (pathOnly == "/api/messages") return handlePostMessage(body);
        if (pathOnly == "/api/v1/datalog") return handleDatalogQuery(body);
        if (pathOnly == "/api/v1/blocks") return handleUpsertBlock(body);
        if (pathOnly == "/api/context/retrieve") return handleContextRetrieve(body);
        return httpResponse(404, "{\"error\":\"Not found\"}");
    }

    return httpResponse(405, "{\"error\":\"Method not allowed\"}");
}

std::string HttpServer::buildJsonHealth() {
    std::ostringstream oss;
    oss << "{\"status\":\"ok\",\"service\":\"nodepanda\",\"version\":\"2.0.0\",\"port\":" << m_port << "}";
    return oss.str();
}

std::string HttpServer::buildJsonSnapshot() {
    if (!m_app) return "{}";
    std::ostringstream oss;
    oss << "{\"notes\":" << m_app->getNoteManager().getAllNotes().size()
        << ",\"blocks\":" << m_app->getOutlinerEngine().getAllBlocks().size()
        << ",\"tasks\":" << m_app->getTaskEngine().getTasks().size()
        << ",\"runs\":" << m_app->getAgentEngine().listRuns().size()
        << ",\"skills\":" << m_app->getSkillManager().getSkills().size()
        << ",\"db\":" << buildJsonDbStatus()
        << ",\"sync\":" << buildJsonSyncStatus()
        << "}";
    return oss.str();
}

std::string HttpServer::buildJsonNoteList() {
    if (!m_app) return "[]";
    auto& notes = m_app->getNoteManager().getAllNotes();
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& [id, note] : notes) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << jsonEscape(id) << "\",\"titulo\":\"" << jsonEscape(note.title)
            << "\",\"palabras\":" << note.content.length()
            << ",\"enlaces_count\":" << note.links.size() << "}";
    }
    oss << "]";
    return oss.str();
}

std::string HttpServer::buildJsonNote(const std::string& id) {
    if (!m_app) return "{}";
    const Note* note = m_app->getNoteManager().getNoteById(id);
    if (!note) return "{}";
    std::ostringstream oss;
    oss << "{\"id\":\"" << jsonEscape(id) << "\",\"titulo\":\"" << jsonEscape(note->title)
        << "\",\"contenido\":\"" << jsonEscape(note->content) << "\",\"enlaces\":[";
    bool first = true;
    for (const auto& link : note->links) {
        if (!first) oss << ",";
        first = false;
        oss << "\"" << jsonEscape(link) << "\"";
    }
    oss << "],\"frontmatter\":{";
    first = true;
    for (const auto& [key, value] : note->frontmatter) {
        if (!first) oss << ",";
        first = false;
        oss << "\"" << jsonEscape(key) << "\":\"" << jsonEscape(value) << "\"";
    }
    oss << "}}";
    return oss.str();
}

std::string HttpServer::buildJsonGraph() {
    if (!m_app) return "{\"nodos\":[],\"aristas\":[]}";
    const auto& graph = m_app->getGraph();
    std::ostringstream oss;
    oss << "{\"nodos\":[";
    bool first = true;
    for (const auto& node : graph.getNodes()) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << jsonEscape(node.noteId) << "\",\"tipo\":\"" << jsonEscape(node.nodeType)
            << "\",\"conexiones\":" << node.connectionCount << "}";
    }
    oss << "],\"aristas\":[";
    first = true;
    for (const auto& edge : graph.getEdges()) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"desde\":\"" << jsonEscape(edge.from) << "\",\"hasta\":\"" << jsonEscape(edge.to) << "\"}";
    }
    oss << "]}";
    return oss.str();
}

std::string HttpServer::buildJsonSearch(const std::string& query, int topN) {
    if (!m_app) return "[]";
    const auto& results = m_app->getMemoryEngine().search(query, topN);
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& result : results) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << jsonEscape(result.noteId) << "\",\"titulo\":\"" << jsonEscape(result.title)
            << "\",\"score\":" << std::fixed << std::setprecision(3) << result.score << "}";
    }
    oss << "]";
    return oss.str();
}

std::string HttpServer::buildJsonBlocks(const std::string& pageId) {
    if (!m_app) return "[]";
    const auto& blocks = m_app->getOutlinerEngine().getAllBlocks();
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& [uuid, blk] : blocks) {
        if (!pageId.empty() && blk.pageId != pageId) continue;
        if (!first) oss << ",";
        first = false;
        oss << "{\"uuid\":\"" << jsonEscape(blk.uuid) << "\",\"page\":\"" << jsonEscape(blk.pageId)
            << "\",\"parent\":\"" << jsonEscape(blk.parentUuid) << "\",\"content\":\"" << jsonEscape(blk.content) << "\"}";
    }
    oss << "]";
    return oss.str();
}

std::string HttpServer::buildJsonTasks() {
    if (!m_app) return "[]";
    const auto& tasks = m_app->getTaskEngine().getTasks();
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& t : tasks) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"uuid\":\"" << jsonEscape(t.uuid) << "\",\"page\":\"" << jsonEscape(t.pageId)
            << "\",\"status\":\"" << TaskEngine::statusToString(t.status)
            << "\",\"priority\":\"" << TaskEngine::priorityToString(t.priority)
            << "\",\"title\":\"" << jsonEscape(t.title) << "\"}";
    }
    oss << "]";
    return oss.str();
}

std::string HttpServer::buildJsonRuns() {
    if (!m_app) return "[]";
    auto runs = m_app->getAgentEngine().listRuns();
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& r : runs) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << jsonEscape(r.id) << "\",\"title\":\"" << jsonEscape(r.title)
            << "\",\"model\":\"" << jsonEscape(r.model) << "\",\"status\":\"" << jsonEscape(r.status)
            << "\",\"cost\":\"" << AiCatalog::formatUsdFromNano(r.costNano) << "\"}";
    }
    oss << "]";
    return oss.str();
}

std::string HttpServer::buildJsonMessages(const std::string& room) {
    if (!m_app) return "[]";
    auto msgs = m_app->getAgentEngine().listMessages(room);
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& m : msgs) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << jsonEscape(m.id) << "\",\"room\":\"" << jsonEscape(m.room)
            << "\",\"from\":\"" << jsonEscape(m.from) << "\",\"to\":\"" << jsonEscape(m.to)
            << "\",\"text\":\"" << jsonEscape(m.text) << "\"}";
    }
    oss << "]";
    return oss.str();
}

std::string HttpServer::buildJsonSkills() {
    if (!m_app) return "[]";
    const auto& skills = m_app->getSkillManager().getSkills();
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& s : skills) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"id\":\"" << jsonEscape(s.id) << "\",\"name\":\"" << jsonEscape(s.name)
            << "\",\"enabled\":" << (s.enabled ? "true" : "false") << "}";
    }
    oss << "]";
    return oss.str();
}

std::string HttpServer::buildJsonDbStatus() {
    if (!m_app) return "{}";
    auto st = m_app->getDatabaseManager().getStatus();
    std::ostringstream oss;
    oss << "{\"engine\":\"" << jsonEscape(st.engineName)
        << "\",\"mode\":\"" << jsonEscape(st.modeName)
        << "\",\"connected\":" << (st.connected ? "true" : "false")
        << ",\"synced_notes\":" << st.syncedNotes << "}";
    return oss.str();
}

std::string HttpServer::buildJsonSyncStatus() {
    if (!m_app) return "{}";
    auto diag = m_app->getSyncEngine().diagnostics(m_app->getNoteManager());
    std::ostringstream oss;
    oss << "{\"local_t\":" << diag.localT << ",\"remote_t\":" << diag.remoteT
        << ",\"checksum\":\"" << jsonEscape(diag.localChecksum)
        << "\",\"e2ee\":" << (diag.e2eeEnabled ? "true" : "false")
        << ",\"running\":" << (diag.running ? "true" : "false") << "}";
    return oss.str();
}

std::string HttpServer::handleAuthLogin(const std::string& body) {
    if (!m_app) return httpResponse(500, "{\"error\":\"App not initialized\"}");
    std::string user = extractJsonString(body, "username");
    if (user.empty()) user = extractJsonString(body, "email");
    std::string pass = extractJsonString(body, "password");
    UserAccount acct;
    std::string err;
    if (!m_app->getAuthManager().authenticate(user, pass, acct, err)) {
        return httpResponse(401, "{\"error\":\"" + jsonEscape(err) + "\"}");
    }
    std::string token = m_app->getAuthManager().createSession(acct);
    return httpResponse(200, "{\"token\":\"" + jsonEscape(token) + "\",\"user\":\"" + jsonEscape(acct.username) + "\"}");
}

std::string HttpServer::handleAuthRegister(const std::string& body) {
    if (!m_app) return httpResponse(500, "{\"error\":\"App not initialized\"}");
    std::string user = extractJsonString(body, "username");
    std::string email = extractJsonString(body, "email");
    std::string pass = extractJsonString(body, "password");
    std::string err;
    if (!m_app->getAuthManager().registerUser(user, email, pass, err)) {
        return httpResponse(400, "{\"error\":\"" + jsonEscape(err) + "\"}");
    }
    return httpResponse(201, "{\"status\":\"created\",\"username\":\"" + jsonEscape(user) + "\"}");
}

std::string HttpServer::handleCreateRun(const std::string& body) {
    if (!m_app) return httpResponse(500, "{\"error\":\"App not initialized\"}");
    std::string prompt = extractJsonString(body, "prompt");
    std::string model = extractJsonString(body, "model");
    std::string room = extractJsonString(body, "room");
    if (prompt.empty()) return httpResponse(400, "{\"error\":\"Missing prompt\"}");
    auto run = m_app->getAgentEngine().createAndStartRun(
        "api", prompt, model.empty() ? "auto" : model, "auto", room.empty() ? "general" : room);
    return httpResponse(201, "{\"id\":\"" + jsonEscape(run.id) + "\",\"status\":\"" + jsonEscape(run.status) + "\"}");
}

std::string HttpServer::handlePostMessage(const std::string& body) {
    if (!m_app) return httpResponse(500, "{\"error\":\"App not initialized\"}");
    std::string room = extractJsonString(body, "room");
    std::string from = extractJsonString(body, "from");
    std::string to = extractJsonString(body, "to");
    std::string text = extractJsonString(body, "text");
    auto msg = m_app->getAgentEngine().postMessage(
        room.empty() ? "general" : room,
        from.empty() ? "usuario" : from,
        to.empty() ? "todos" : to,
        text);
    return httpResponse(201, "{\"id\":\"" + jsonEscape(msg.id) + "\"}");
}

std::string HttpServer::handleDatalogQuery(const std::string& body) {
    if (!m_app) return httpResponse(500, "{\"error\":\"App not initialized\"}");
    std::string q = extractJsonString(body, "query");
    std::string err;
    auto rows = m_app->getDatalogEngine().executeQuery(q, err);
    if (!err.empty()) return httpResponse(400, "{\"error\":\"" + jsonEscape(err) + "\"}");
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{";
        bool first = true;
        for (const auto& [k, v] : rows[i].bindings) {
            if (!first) oss << ",";
            first = false;
            oss << "\"" << jsonEscape(k) << "\":\"" << jsonEscape(v) << "\"";
        }
        oss << "}";
    }
    oss << "]";
    return httpResponse(200, oss.str());
}

std::string HttpServer::handleUpsertBlock(const std::string& body) {
    if (!m_app) return httpResponse(500, "{\"error\":\"App not initialized\"}");
    std::string page = extractJsonString(body, "page");
    std::string content = extractJsonString(body, "content");
    if (page.empty()) page = m_app->getJournalEngine().ensureTodayJournal(m_app->getNoteManager());
    std::string uuid = m_app->getOutlinerEngine().createBlock(m_app->getNoteManager(), page, content);
    m_app->rebuildKnowledgeIndices();
    return httpResponse(201, "{\"uuid\":\"" + jsonEscape(uuid) + "\",\"page\":\"" + jsonEscape(page) + "\"}");
}

std::string HttpServer::handleContextRetrieve(const std::string& body) {
    if (!m_app) return httpResponse(500, "{\"error\":\"App not initialized\"}");
    std::string q = extractJsonString(body, "query");
    auto res = m_app->getContextBrain().retrieve(m_app->getNoteManager(), q, 1800);
    std::ostringstream oss;
    oss << "{\"method\":\"" << jsonEscape(res.method)
        << "\",\"tokens\":" << res.estimatedTokens
        << ",\"hits\":" << res.hits.size()
        << ",\"text\":\"" << jsonEscape(res.formattedXml) << "\"}";
    return httpResponse(200, oss.str());
}

std::string HttpServer::httpResponse(int code, const std::string& body, const std::string& contentType) {
    const char* statusText = "OK";
    if (code == 201) statusText = "Created";
    else if (code == 400) statusText = "Bad Request";
    else if (code == 401) statusText = "Unauthorized";
    else if (code == 404) statusText = "Not Found";
    else if (code == 405) statusText = "Method Not Allowed";
    else if (code == 500) statusText = "Internal Server Error";

    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << statusText << "\r\n"
        << "Content-Type: " << contentType << "; charset=utf-8\r\n"
        << "Content-Length: " << body.length() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS\r\n"
        << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

std::string HttpServer::urlDecode(const std::string& str) {
    std::ostringstream oss;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            try {
                int value = std::stoi(str.substr(i + 1, 2), nullptr, 16);
                oss << static_cast<char>(value);
                i += 2;
            } catch (...) {
                oss << str[i];
            }
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
    if (queryStart == std::string::npos) return {};
    std::string query = path.substr(queryStart + 1);
    std::string searchStr = param + "=";
    size_t paramStart = query.find(searchStr);
    if (paramStart == std::string::npos) return {};
    size_t valueStart = paramStart + searchStr.length();
    size_t valueEnd = query.find('&', valueStart);
    if (valueEnd == std::string::npos) valueEnd = query.length();
    return urlDecode(query.substr(valueStart, valueEnd - valueStart));
}

}
