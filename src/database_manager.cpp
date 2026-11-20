#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "database_manager.h"
#include "note_manager.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace nodepanda {

namespace {

std::string trimSpaces(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string runProcessCapture(const std::string& cmd, int& exitCodeOut) {
    std::string output;
#ifdef _WIN32
    FILE* pipe = _popen((cmd + " 2>&1").c_str(), "r");
#else
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
#endif
    if (!pipe) {
        exitCodeOut = -1;
        return "No se pudo ejecutar el controlador externo.";
    }
    char buffer[1024];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
#ifdef _WIN32
    exitCodeOut = _pclose(pipe);
#else
    exitCodeOut = pclose(pipe);
#endif
    return output;
}

}

void DatabaseManager::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    m_configPath = m_dataDir / ".nodepanda" / "database.conf";
    m_walPath = m_dataDir / ".nodepanda" / "db_wal.sql";
    std::error_code ec;
    std::filesystem::create_directories(m_dataDir / ".nodepanda", ec);

    if (!loadConfig()) {
        const char* envEngine = std::getenv("NODEPANDA_DB_ENGINE");
        const char* envHost = std::getenv("NODEPANDA_DB_HOST");
        const char* envPort = std::getenv("NODEPANDA_DB_PORT");
        const char* envName = std::getenv("NODEPANDA_DB_NAME");
        const char* envUser = std::getenv("NODEPANDA_DB_USER");
        const char* envPass = std::getenv("NODEPANDA_DB_PASSWORD");
        const char* envMode = std::getenv("NODEPANDA_DB_MODE");

        if (envEngine) m_config.engine = stringToEngine(envEngine);
        if (envHost) m_config.host = envHost;
        if (envPort) {
            try { m_config.port = std::stoi(envPort); } catch (...) {}
        }
        if (envName) m_config.database = envName;
        if (envUser) m_config.username = envUser;
        if (envPass) m_config.password = envPass;
        if (envMode && std::string(envMode) == "server") {
            m_config.deployMode = DbDeployMode::Server;
        }
        saveConfig();
    }

    m_status.engineName = engineToString(m_config.engine);
    m_status.modeName = (m_config.deployMode == DbDeployMode::Server) ? "server" : "local";
    m_status.cliDriverAvailable = !locateDriverBinary(m_config.engine).empty();
}

void DatabaseManager::configure(const DbConfig& cfg) {
    m_config = cfg;
    m_status.engineName = engineToString(m_config.engine);
    m_status.modeName = (m_config.deployMode == DbDeployMode::Server) ? "server" : "local";
    m_status.cliDriverAvailable = !locateDriverBinary(m_config.engine).empty();
    saveConfig();
}

bool DatabaseManager::loadConfig() {
    std::ifstream f(m_configPath);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trimSpaces(line.substr(0, eq));
        std::string v = trimSpaces(line.substr(eq + 1));
        if (k == "engine") m_config.engine = stringToEngine(v);
        else if (k == "mode") m_config.deployMode = (v == "server") ? DbDeployMode::Server : DbDeployMode::Local;
        else if (k == "host") m_config.host = v;
        else if (k == "port") { try { m_config.port = std::stoi(v); } catch (...) {} }
        else if (k == "database") m_config.database = v;
        else if (k == "username") m_config.username = v;
        else if (k == "password") m_config.password = v;
        else if (k == "tls") m_config.useTls = (v == "1" || v == "true");
        else if (k == "autosync") m_config.autoSync = (v == "1" || v == "true");
    }
    return true;
}

bool DatabaseManager::saveConfig() const {
    std::ofstream f(m_configPath, std::ios::trunc);
    if (!f.is_open()) return false;
    f << "engine=" << engineToString(m_config.engine) << "\n";
    f << "mode=" << (m_config.deployMode == DbDeployMode::Server ? "server" : "local") << "\n";
    f << "host=" << m_config.host << "\n";
    f << "port=" << m_config.port << "\n";
    f << "database=" << m_config.database << "\n";
    f << "username=" << m_config.username << "\n";
    f << "password=" << m_config.password << "\n";
    f << "tls=" << (m_config.useTls ? "true" : "false") << "\n";
    f << "autosync=" << (m_config.autoSync ? "true" : "false") << "\n";
    return true;
}

std::string DatabaseManager::engineToString(DbEngineType engine) {
    switch (engine) {
        case DbEngineType::PostgreSQL: return "postgresql";
        case DbEngineType::MySQL: return "mysql";
        case DbEngineType::LocalOnly: return "local";
    }
    return "local";
}

DbEngineType DatabaseManager::stringToEngine(const std::string& s) {
    std::string low = s;
    std::transform(low.begin(), low.end(), low.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (low == "postgres" || low == "postgresql" || low == "pg") return DbEngineType::PostgreSQL;
    if (low == "mysql" || low == "mariadb") return DbEngineType::MySQL;
    return DbEngineType::LocalOnly;
}

std::string DatabaseManager::escapeSql(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        if (c == '\'') out += "''";
        else if (c == '\\') out += "\\\\";
        else if (c == '\0') continue;
        else out.push_back(c);
    }
    return out;
}

bool DatabaseManager::probeTcpPort(const std::string& host, int port, int timeoutMs, std::string& bannerOut) {
    bannerOut.clear();
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#endif

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);

    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    bool connected = false;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
#ifdef _WIN32
        SOCKET sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == INVALID_SOCKET) continue;
        DWORD tv = static_cast<DWORD>(timeoutMs);
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        if (connect(sock, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) {
            connected = true;
            char buf[256] = {};
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n > 0) bannerOut.assign(buf, static_cast<size_t>(n));
            closesocket(sock);
            break;
        }
        closesocket(sock);
#else
        int sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;
        timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            connected = true;
            char buf[256] = {};
            ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n > 0) bannerOut.assign(buf, static_cast<size_t>(n));
            close(sock);
            break;
        }
        close(sock);
#endif
    }

    freeaddrinfo(res);
#ifdef _WIN32
    WSACleanup();
#endif
    return connected;
}

bool DatabaseManager::probePostgresHandshake(const std::string& host, int port, std::string& infoOut) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#endif
    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) {
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    bool ok = false;
#ifdef _WIN32
    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock != INVALID_SOCKET) {
        DWORD tv = 1500;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        if (connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) == 0) {
            unsigned char sslReq[8] = {0, 0, 0, 8, 0x04, 0xd2, 0x16, 0x2f};
            send(sock, reinterpret_cast<const char*>(sslReq), 8, 0);
            char resp = 0;
            int n = recv(sock, &resp, 1, 0);
            if (n == 1 && (resp == 'S' || resp == 'N')) {
                ok = true;
                infoOut = (resp == 'S') ? "PostgreSQL v3 (TLS soportado)" : "PostgreSQL v3 (TCP plano)";
            } else {
                ok = true;
                infoOut = "PostgreSQL TCP activo";
            }
        }
        closesocket(sock);
    }
#else
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock >= 0) {
        timeval tv{1, 500000};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (connect(sock, res->ai_addr, res->ai_addrlen) == 0) {
            unsigned char sslReq[8] = {0, 0, 0, 8, 0x04, 0xd2, 0x16, 0x2f};
            send(sock, sslReq, 8, 0);
            char resp = 0;
            ssize_t n = recv(sock, &resp, 1, 0);
            if (n == 1 && (resp == 'S' || resp == 'N')) {
                ok = true;
                infoOut = (resp == 'S') ? "PostgreSQL v3 (TLS soportado)" : "PostgreSQL v3 (TCP plano)";
            } else {
                ok = true;
                infoOut = "PostgreSQL TCP activo";
            }
        }
        close(sock);
    }
#endif
    freeaddrinfo(res);
#ifdef _WIN32
    WSACleanup();
#endif
    return ok;
}

bool DatabaseManager::probeMysqlHandshake(const std::string& host, int port, std::string& bannerOut) {
    std::string raw;
    if (!probeTcpPort(host, port, 1500, raw)) return false;
    if (raw.size() > 6) {
        std::string ver;
        for (size_t i = 5; i < raw.size() && raw[i] != '\0'; ++i) {
            if (raw[i] >= 32 && raw[i] <= 126) ver.push_back(raw[i]);
        }
        bannerOut = ver.empty() ? "MySQL Protocol v10" : ("MySQL " + ver);
    } else {
        bannerOut = "MySQL TCP activo";
    }
    return true;
}

std::string DatabaseManager::locateDriverBinary(DbEngineType engine) const {
    if (engine == DbEngineType::LocalOnly) return {};
    std::vector<std::string> candidates;
    if (engine == DbEngineType::PostgreSQL) {
        candidates = {
            "psql",
            "C:\\Program Files\\PostgreSQL\\17\\bin\\psql.exe",
            "C:\\Program Files\\PostgreSQL\\16\\bin\\psql.exe",
            "C:\\Program Files\\PostgreSQL\\15\\bin\\psql.exe",
            "/usr/bin/psql"
        };
    } else if (engine == DbEngineType::MySQL) {
        candidates = {
            "mysql",
            "C:\\xampp\\mysql\\bin\\mysql.exe",
            "C:\\Program Files\\MySQL\\MySQL Server 8.0\\bin\\mysql.exe",
            "C:\\Program Files\\MySQL\\MySQL Server 8.4\\bin\\mysql.exe",
            "/usr/bin/mysql"
        };
    }

    for (const auto& c : candidates) {
        if (c.find('\\') != std::string::npos || c.find('/') != std::string::npos) {
            if (std::filesystem::exists(c)) return c;
        } else {
            int code = -1;
#ifdef _WIN32
            std::string out = runProcessCapture("where " + c, code);
#else
            std::string out = runProcessCapture("which " + c, code);
#endif
            if (code == 0 && !trimSpaces(out).empty()) {
                return c;
            }
        }
    }
    return {};
}

bool DatabaseManager::testConnection(std::string& detailsOut) {
    m_status.engineName = engineToString(m_config.engine);
    m_status.modeName = (m_config.deployMode == DbDeployMode::Server) ? "server" : "local";
    m_status.cliDriverAvailable = !locateDriverBinary(m_config.engine).empty();
    m_status.lastError.clear();

    if (m_config.engine == DbEngineType::LocalOnly) {
        m_status.connected = true;
        m_status.tcpReachable = true;
        m_status.serverVersion = "Almacenamiento local nativo";
        detailsOut = m_status.serverVersion;
        return true;
    }

    std::string protoInfo;
    bool reachable = false;
    if (m_config.engine == DbEngineType::PostgreSQL) {
        reachable = probePostgresHandshake(m_config.host, m_config.port, protoInfo);
    } else if (m_config.engine == DbEngineType::MySQL) {
        reachable = probeMysqlHandshake(m_config.host, m_config.port, protoInfo);
    }

    m_status.tcpReachable = reachable;
    if (!reachable) {
        m_status.connected = false;
        m_status.lastError = "No se pudo conectar a " + m_config.host + ":" + std::to_string(m_config.port);
        detailsOut = m_status.lastError;
        return false;
    }

    m_status.serverVersion = protoInfo;
    if (m_status.cliDriverAvailable) {
        auto res = executeSql("SELECT 1;");
        if (res.ok) {
            m_status.connected = true;
            detailsOut = protoInfo + " (consulta verificada)";
            return true;
        }
        m_status.connected = false;
        m_status.lastError = res.error;
        detailsOut = protoInfo + " | Error auth/DB: " + res.error;
        return false;
    }

    m_status.connected = true;
    detailsOut = protoInfo + " (socket verificado, WAL SQL activo)";
    return true;
}

std::string DatabaseManager::exportSchemaSql(DbEngineType engine) const {
    std::filesystem::path p = m_dataDir / "sql" /
        (engine == DbEngineType::MySQL ? "mysql_schema.sql" : "postgres_schema.sql");
    std::ifstream f(p);
    if (f.is_open()) {
        std::ostringstream oss;
        oss << f.rdbuf();
        return oss.str();
    }
    if (engine == DbEngineType::MySQL) {
        return "CREATE TABLE IF NOT EXISTS notes (id VARCHAR(255) PRIMARY KEY, title VARCHAR(512), content LONGTEXT, frontmatter_json LONGTEXT);\n";
    }
    return "CREATE TABLE IF NOT EXISTS notes (id VARCHAR(255) PRIMARY KEY, title VARCHAR(512), content TEXT, frontmatter_json TEXT);\n";
}

bool DatabaseManager::initializeSchema(std::string& errorOut) {
    if (m_config.engine == DbEngineType::LocalOnly) {
        return true;
    }
    std::string sql = exportSchemaSql(m_config.engine);
    auto res = executeSql(sql);
    if (!res.ok) {
        errorOut = res.error;
        return false;
    }
    return true;
}

void DatabaseManager::appendWalEntry(const std::string& sql) {
    std::ofstream f(m_walPath, std::ios::app);
    if (f.is_open()) {
        f << sql << "\n";
    }
}

DbQueryResult DatabaseManager::executeViaPostgresCli(const std::string& sql) {
    DbQueryResult res;
    std::string bin = locateDriverBinary(DbEngineType::PostgreSQL);
    if (bin.empty()) {
        res.error = "Controlador psql no encontrado en el sistema.";
        return res;
    }

    std::filesystem::path tmpSql = m_dataDir / ".nodepanda" / "tmp_pg.sql";
    {
        std::ofstream f(tmpSql, std::ios::trunc);
        f << sql << "\n";
    }

    auto t0 = std::chrono::steady_clock::now();
    std::ostringstream cmd;
#ifdef _WIN32
    if (!m_config.password.empty()) {
        cmd << "set PGPASSWORD=" << m_config.password << "&& ";
    }
    cmd << "\"" << bin << "\" -h " << m_config.host
        << " -p " << m_config.port
        << " -U " << m_config.username
        << " -d " << m_config.database
        << " -A -F \"|\" -v ON_ERROR_STOP=1 -f \"" << tmpSql.string() << "\"";
#else
    if (!m_config.password.empty()) {
        cmd << "PGPASSWORD=\"" << m_config.password << "\" ";
    }
    cmd << "\"" << bin << "\" -h " << m_config.host
        << " -p " << m_config.port
        << " -U " << m_config.username
        << " -d " << m_config.database
        << " -A -F \"|\" -v ON_ERROR_STOP=1 -f \"" << tmpSql.string() << "\"";
#endif

    int exitCode = -1;
    std::string raw = runProcessCapture(cmd.str(), exitCode);
    auto t1 = std::chrono::steady_clock::now();
    res.latencyMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::error_code ec;
    std::filesystem::remove(tmpSql, ec);

    if (exitCode != 0) {
        res.ok = false;
        res.error = trimSpaces(raw);
        return res;
    }

    res.ok = true;
    std::istringstream iss(raw);
    std::string line;
    bool first = true;
    while (std::getline(iss, line)) {
        line = trimSpaces(line);
        if (line.empty()) continue;
        if (line.rfind("(", 0) == 0 && line.find("row") != std::string::npos) continue;
        std::vector<std::string> cols;
        std::istringstream ls(line);
        std::string cell;
        while (std::getline(ls, cell, '|')) cols.push_back(cell);
        if (first) {
            res.columns = cols;
            first = false;
        } else {
            res.rows.push_back(cols);
        }
    }
    return res;
}

DbQueryResult DatabaseManager::executeViaMysqlCli(const std::string& sql) {
    DbQueryResult res;
    std::string bin = locateDriverBinary(DbEngineType::MySQL);
    if (bin.empty()) {
        res.error = "Controlador mysql no encontrado en el sistema.";
        return res;
    }

    std::filesystem::path tmpSql = m_dataDir / ".nodepanda" / "tmp_mysql.sql";
    {
        std::ofstream f(tmpSql, std::ios::trunc);
        f << sql << "\n";
    }

    auto t0 = std::chrono::steady_clock::now();
    std::ostringstream cmd;
#ifdef _WIN32
    if (!m_config.password.empty()) {
        cmd << "set MYSQL_PWD=" << m_config.password << "&& ";
    }
    cmd << "\"" << bin << "\" -h " << m_config.host
        << " -P " << m_config.port
        << " -u " << m_config.username
        << " " << m_config.database
        << " --batch < \"" << tmpSql.string() << "\"";
#else
    if (!m_config.password.empty()) {
        cmd << "MYSQL_PWD=\"" << m_config.password << "\" ";
    }
    cmd << "\"" << bin << "\" -h " << m_config.host
        << " -P " << m_config.port
        << " -u " << m_config.username
        << " " << m_config.database
        << " --batch < \"" << tmpSql.string() << "\"";
#endif

    int exitCode = -1;
    std::string raw = runProcessCapture(cmd.str(), exitCode);
    auto t1 = std::chrono::steady_clock::now();
    res.latencyMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::error_code ec;
    std::filesystem::remove(tmpSql, ec);

    if (exitCode != 0) {
        res.ok = false;
        res.error = trimSpaces(raw);
        return res;
    }

    res.ok = true;
    std::istringstream iss(raw);
    std::string line;
    bool first = true;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> cols;
        std::istringstream ls(line);
        std::string cell;
        while (std::getline(ls, cell, '\t')) cols.push_back(cell);
        if (first) {
            res.columns = cols;
            first = false;
        } else {
            res.rows.push_back(cols);
        }
    }
    return res;
}

DbQueryResult DatabaseManager::executeSql(const std::string& sql) {
    if (m_config.engine == DbEngineType::LocalOnly) {
        appendWalEntry(sql);
        DbQueryResult localRes;
        localRes.ok = true;
        localRes.affectedRows = 1;
        return localRes;
    }

    DbQueryResult res = (m_config.engine == DbEngineType::PostgreSQL)
        ? executeViaPostgresCli(sql)
        : executeViaMysqlCli(sql);

    if (!res.ok) {
        appendWalEntry(sql);
        m_status.lastError = res.error;
    } else {
        m_status.connected = true;
        m_status.lastError.clear();
    }
    return res;
}

bool DatabaseManager::upsertNoteRecord(const std::string& id, const std::string& title,
                                       const std::string& content, const std::string& frontmatterJson) {
    std::ostringstream sql;
    if (m_config.engine == DbEngineType::MySQL) {
        sql << "INSERT INTO notes (id, title, content, frontmatter_json) VALUES ('"
            << escapeSql(id) << "', '" << escapeSql(title) << "', '"
            << escapeSql(content) << "', '" << escapeSql(frontmatterJson)
            << "') ON DUPLICATE KEY UPDATE title=VALUES(title), content=VALUES(content), frontmatter_json=VALUES(frontmatter_json);";
    } else {
        sql << "INSERT INTO notes (id, title, content, frontmatter_json) VALUES ('"
            << escapeSql(id) << "', '" << escapeSql(title) << "', '"
            << escapeSql(content) << "', '" << escapeSql(frontmatterJson)
            << "') ON CONFLICT (id) DO UPDATE SET title=EXCLUDED.title, content=EXCLUDED.content, frontmatter_json=EXCLUDED.frontmatter_json, updated_at=CURRENT_TIMESTAMP;";
    }
    auto res = executeSql(sql.str());
    if (res.ok) m_status.syncedNotes++;
    return res.ok;
}

bool DatabaseManager::upsertBlockRecord(const std::string& uuid, const std::string& pageId,
                                        const std::string& parentUuid, int orderIndex,
                                        const std::string& content, const std::string& propsJson) {
    std::ostringstream sql;
    if (m_config.engine == DbEngineType::MySQL) {
        sql << "INSERT INTO blocks (uuid, page_id, parent_uuid, order_index, content, properties_json) VALUES ('"
            << escapeSql(uuid) << "', '" << escapeSql(pageId) << "', '" << escapeSql(parentUuid)
            << "', " << orderIndex << ", '" << escapeSql(content) << "', '" << escapeSql(propsJson)
            << "') ON DUPLICATE KEY UPDATE parent_uuid=VALUES(parent_uuid), order_index=VALUES(order_index), content=VALUES(content), properties_json=VALUES(properties_json);";
    } else {
        sql << "INSERT INTO blocks (uuid, page_id, parent_uuid, order_index, content, properties_json) VALUES ('"
            << escapeSql(uuid) << "', '" << escapeSql(pageId) << "', '" << escapeSql(parentUuid)
            << "', " << orderIndex << ", '" << escapeSql(content) << "', '" << escapeSql(propsJson)
            << "') ON CONFLICT (uuid) DO UPDATE SET parent_uuid=EXCLUDED.parent_uuid, order_index=EXCLUDED.order_index, content=EXCLUDED.content, properties_json=EXCLUDED.properties_json;";
    }
    auto res = executeSql(sql.str());
    if (res.ok) m_status.syncedBlocks++;
    return res.ok;
}

bool DatabaseManager::upsertTaskRecord(const std::string& uuid, const std::string& pageId,
                                       const std::string& status, const std::string& priority,
                                       const std::string& title, const std::string& scheduled,
                                       const std::string& deadline) {
    std::ostringstream sql;
    if (m_config.engine == DbEngineType::MySQL) {
        sql << "INSERT INTO tasks (uuid, page_id, status, priority, title, scheduled_date, deadline_date) VALUES ('"
            << escapeSql(uuid) << "', '" << escapeSql(pageId) << "', '" << escapeSql(status)
            << "', '" << escapeSql(priority) << "', '" << escapeSql(title) << "', '"
            << escapeSql(scheduled) << "', '" << escapeSql(deadline)
            << "') ON DUPLICATE KEY UPDATE status=VALUES(status), priority=VALUES(priority), title=VALUES(title), scheduled_date=VALUES(scheduled_date), deadline_date=VALUES(deadline_date);";
    } else {
        sql << "INSERT INTO tasks (uuid, page_id, status, priority, title, scheduled_date, deadline_date) VALUES ('"
            << escapeSql(uuid) << "', '" << escapeSql(pageId) << "', '" << escapeSql(status)
            << "', '" << escapeSql(priority) << "', '" << escapeSql(title) << "', '"
            << escapeSql(scheduled) << "', '" << escapeSql(deadline)
            << "') ON CONFLICT (uuid) DO UPDATE SET status=EXCLUDED.status, priority=EXCLUDED.priority, title=EXCLUDED.title, scheduled_date=EXCLUDED.scheduled_date, deadline_date=EXCLUDED.deadline_date;";
    }
    auto res = executeSql(sql.str());
    if (res.ok) m_status.syncedTasks++;
    return res.ok;
}

bool DatabaseManager::insertAgentRunRecord(const std::string& runId, const std::string& owner,
                                           const std::string& prompt, const std::string& model,
                                           const std::string& status, double costUsd) {
    std::ostringstream sql;
    if (m_config.engine == DbEngineType::MySQL) {
        sql << "INSERT INTO agent_runs (id, owner, prompt, model, status, cost_usd) VALUES ('"
            << escapeSql(runId) << "', '" << escapeSql(owner) << "', '" << escapeSql(prompt)
            << "', '" << escapeSql(model) << "', '" << escapeSql(status) << "', " << costUsd
            << ") ON DUPLICATE KEY UPDATE status=VALUES(status), cost_usd=VALUES(cost_usd);";
    } else {
        sql << "INSERT INTO agent_runs (id, owner, prompt, model, status, cost_usd) VALUES ('"
            << escapeSql(runId) << "', '" << escapeSql(owner) << "', '" << escapeSql(prompt)
            << "', '" << escapeSql(model) << "', '" << escapeSql(status) << "', " << costUsd
            << ") ON CONFLICT (id) DO UPDATE SET status=EXCLUDED.status, cost_usd=EXCLUDED.cost_usd;";
    }
    auto res = executeSql(sql.str());
    if (res.ok) m_status.syncedRuns++;
    return res.ok;
}

bool DatabaseManager::insertUsageLedgerRecord(const std::string& id, const std::string& runId,
                                              const std::string& provider, const std::string& model,
                                              int inputTokens, int outputTokens, double costUsd) {
    std::ostringstream sql;
    sql << "INSERT INTO usage_ledger (id, run_id, provider, model, input_tokens, output_tokens, cost_usd) VALUES ('"
        << escapeSql(id) << "', '" << escapeSql(runId) << "', '" << escapeSql(provider)
        << "', '" << escapeSql(model) << "', " << inputTokens << ", " << outputTokens
        << ", " << costUsd << ");";
    return executeSql(sql.str()).ok;
}

bool DatabaseManager::upsertUserRecord(const std::string& id, const std::string& username,
                                       const std::string& email, const std::string& salt,
                                       const std::string& hash, const std::string& role) {
    std::ostringstream sql;
    if (m_config.engine == DbEngineType::MySQL) {
        sql << "INSERT INTO users (id, username, email, salt, password_hash, role) VALUES ('"
            << escapeSql(id) << "', '" << escapeSql(username) << "', '" << escapeSql(email)
            << "', '" << escapeSql(salt) << "', '" << escapeSql(hash) << "', '" << escapeSql(role)
            << "') ON DUPLICATE KEY UPDATE email=VALUES(email), password_hash=VALUES(password_hash), role=VALUES(role);";
    } else {
        sql << "INSERT INTO users (id, username, email, salt, password_hash, role) VALUES ('"
            << escapeSql(id) << "', '" << escapeSql(username) << "', '" << escapeSql(email)
            << "', '" << escapeSql(salt) << "', '" << escapeSql(hash) << "', '" << escapeSql(role)
            << "') ON CONFLICT (id) DO UPDATE SET email=EXCLUDED.email, password_hash=EXCLUDED.password_hash, role=EXCLUDED.role;";
    }
    return executeSql(sql.str()).ok;
}

bool DatabaseManager::syncNotes(const NoteManager& nm, std::string& summaryOut) {
    int count = 0;
    int okCount = 0;
    for (const auto& [id, note] : nm.getAllNotes()) {
        std::ostringstream fm;
        fm << "{";
        bool first = true;
        for (const auto& [k, v] : note.frontmatter) {
            if (!first) fm << ",";
            first = false;
            fm << "\"" << k << "\":\"" << v << "\"";
        }
        fm << "}";
        count++;
        if (upsertNoteRecord(note.id, note.title, note.content, fm.str())) {
            okCount++;
        }
    }
    m_status.syncedNotes = okCount;
    summaryOut = "Sincronizadas " + std::to_string(okCount) + "/" + std::to_string(count) +
                 " notas (" + engineToString(m_config.engine) + ")";
    return okCount == count;
}

}
