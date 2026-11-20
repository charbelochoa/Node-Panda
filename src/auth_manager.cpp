#include "auth_manager.h"
#include "crypto_engine.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace nodepanda {

namespace {

std::string trimLine(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string toLowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}

int64_t AuthManager::nowUnix() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

void AuthManager::init(const std::filesystem::path& dataDir) {
    m_authDir = dataDir / ".nodepanda";
    std::error_code ec;
    std::filesystem::create_directories(m_authDir, ec);
    loadStore();
}

bool AuthManager::hasUsers() const {
    return !m_users.empty();
}

bool AuthManager::registerUser(const std::string& username, const std::string& email,
                               const std::string& password, std::string& errorOut,
                               const std::string& role) {
    std::string cleanUser = trimLine(username);
    std::string cleanEmail = toLowerCopy(trimLine(email));
    if (cleanUser.size() < 2) {
        errorOut = "El nombre de usuario debe tener al menos 2 caracteres.";
        return false;
    }
    if (password.size() < 4) {
        errorOut = "La contrasena debe tener al menos 4 caracteres.";
        return false;
    }
    for (const auto& u : m_users) {
        if (toLowerCopy(u.username) == toLowerCopy(cleanUser) ||
            (!cleanEmail.empty() && toLowerCopy(u.email) == cleanEmail)) {
            errorOut = "Ya existe una cuenta registrada con ese usuario o correo.";
            return false;
        }
    }

    UserAccount acct;
    acct.id = CryptoEngine::generateUuid();
    acct.username = cleanUser;
    acct.email = cleanEmail.empty() ? (toLowerCopy(cleanUser) + "@nodepanda.local") : cleanEmail;
    acct.salt = CryptoEngine::randomHex(16);
    acct.passwordHash = CryptoEngine::deriveKey(password, acct.salt, 6000);
    acct.role = m_users.empty() ? "admin" : role;
    acct.createdAt = nowUnix();

    m_users.push_back(acct);
    saveStore();
    return true;
}

bool AuthManager::authenticate(const std::string& identifier, const std::string& password,
                               UserAccount& outUser, std::string& errorOut) {
    std::string needle = toLowerCopy(trimLine(identifier));
    const UserAccount* matched = nullptr;
    for (const auto& u : m_users) {
        if (toLowerCopy(u.username) == needle || toLowerCopy(u.email) == needle) {
            matched = &u;
            break;
        }
    }

    std::string salt = matched ? matched->salt : "nodepanda-invalid-salt";
    std::string computed = CryptoEngine::deriveKey(password, salt, 6000);
    if (!matched || !CryptoEngine::secureEquals(matched->passwordHash, computed)) {
        errorOut = "Usuario o contrasena incorrectos.";
        return false;
    }

    outUser = *matched;
    m_activeUser = *matched;
    m_hasActiveUser = true;
    return true;
}

std::string AuthManager::createSession(const UserAccount& user) {
    std::string rawToken = CryptoEngine::randomHex(32);
    int64_t now = nowUnix();
    m_sessions.erase(std::remove_if(m_sessions.begin(), m_sessions.end(),
                                    [now](const AuthSession& s) { return s.expiresAt <= now; }),
                     m_sessions.end());

    AuthSession session;
    session.tokenHash = CryptoEngine::sha256(rawToken);
    session.userId = user.id;
    session.username = user.username;
    session.role = user.role;
    session.expiresAt = now + 7 * 86400;
    m_sessions.push_back(session);
    saveStore();
    return rawToken;
}

bool AuthManager::validateToken(const std::string& rawToken, AuthSession& outSession) const {
    if (rawToken.empty()) return false;
    std::string hash = CryptoEngine::sha256(trimLine(rawToken));
    int64_t now = nowUnix();
    for (const auto& s : m_sessions) {
        if (s.expiresAt > now && CryptoEngine::secureEquals(s.tokenHash, hash)) {
            outSession = s;
            return true;
        }
    }
    return false;
}

void AuthManager::revokeToken(const std::string& rawToken) {
    std::string hash = CryptoEngine::sha256(trimLine(rawToken));
    m_sessions.erase(std::remove_if(m_sessions.begin(), m_sessions.end(),
                                    [&hash](const AuthSession& s) {
                                        return CryptoEngine::secureEquals(s.tokenHash, hash);
                                    }),
                     m_sessions.end());
    saveStore();
}

bool AuthManager::loadSavedCliSession(AuthSession& outSession) const {
    std::ifstream f(m_authDir / "cli-token");
    if (!f.is_open()) return false;
    std::string token;
    std::getline(f, token);
    return validateToken(token, outSession);
}

void AuthManager::saveCliToken(const std::string& rawToken) const {
    std::ofstream f(m_authDir / "cli-token", std::ios::trunc);
    if (f.is_open()) {
        f << rawToken << "\n";
    }
}

void AuthManager::clearCliToken() const {
    std::error_code ec;
    std::filesystem::remove(m_authDir / "cli-token", ec);
}

std::string AuthManager::readPasswordMasked(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string password;
#ifdef _WIN32
    while (true) {
        int ch = _getch();
        if (ch == '\r' || ch == '\n') {
            std::cout << "\n";
            break;
        }
        if (ch == 8 || ch == 127) {
            if (!password.empty()) {
                password.pop_back();
                std::cout << "\b \b" << std::flush;
            }
            continue;
        }
        if (ch == 3) {
            std::cout << "\n";
            return {};
        }
        if (ch >= 32 && ch <= 126) {
            password.push_back(static_cast<char>(ch));
            std::cout << '*' << std::flush;
        }
    }
#else
    termios oldt{};
    if (tcgetattr(STDIN_FILENO, &oldt) == 0) {
        termios newt = oldt;
        newt.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        std::getline(std::cin, password);
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        std::cout << "\n";
    } else {
        std::getline(std::cin, password);
    }
#endif
    return password;
}

bool AuthManager::interactiveTerminalLogin(UserAccount& outUser, std::string& outToken) {
    std::cout << "\n";
    std::cout << "  _   _           _        ____                  _\n";
    std::cout << " | \\ | | ___   __| | ___  |  _ \\ __ _ _ __   __| | __ _\n";
    std::cout << " |  \\| |/ _ \\ / _` |/ _ \\ | |_) / _` | '_ \\ / _` |/ _` |\n";
    std::cout << " | |\\  | (_) | (_| |  __/ |  __/ (_| | | | | (_| | (_| |\n";
    std::cout << " |_| \\_|\\___/ \\__,_|\\___| |_|   \\__,_|_| |_|\\__,_|\\__,_|\n\n";

    if (!hasUsers()) {
        std::cout << "Configuracion inicial de cuenta administradora\n";
        std::string username;
        std::string email;
        std::cout << "Usuario: " << std::flush;
        if (!std::getline(std::cin, username)) return false;
        std::cout << "Correo (opcional): " << std::flush;
        std::getline(std::cin, email);
        std::string p1 = readPasswordMasked("Contrasena: ");
        std::string p2 = readPasswordMasked("Confirmar contrasena: ");
        if (p1 != p2) {
            std::cerr << "Error: las contrasenas no coinciden.\n";
            return false;
        }
        std::string err;
        if (!registerUser(username, email, p1, err, "admin")) {
            std::cerr << "Error: " << err << "\n";
            return false;
        }
        std::cout << "Cuenta creada correctamente.\n\n";
    }

    for (int attempt = 1; attempt <= 3; ++attempt) {
        std::string identifier;
        std::cout << "Usuario o correo: " << std::flush;
        if (!std::getline(std::cin, identifier)) return false;
        std::string password = readPasswordMasked("Contrasena: ");
        std::string err;
        if (authenticate(identifier, password, outUser, err)) {
            outToken = createSession(outUser);
            saveCliToken(outToken);
            std::cout << "Sesion iniciada como " << outUser.username << " (" << outUser.role << ").\n";
            return true;
        }
        std::cerr << err << " (Intento " << attempt << "/3)\n";
    }
    return false;
}

void AuthManager::loadStore() {
    m_users.clear();
    m_sessions.clear();
    std::ifstream uf(m_authDir / "users.db");
    std::string line;
    while (std::getline(uf, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        UserAccount u;
        std::string createdStr;
        if (std::getline(iss, u.id, '|') &&
            std::getline(iss, u.username, '|') &&
            std::getline(iss, u.email, '|') &&
            std::getline(iss, u.salt, '|') &&
            std::getline(iss, u.passwordHash, '|') &&
            std::getline(iss, u.role, '|') &&
            std::getline(iss, createdStr, '|')) {
            try { u.createdAt = std::stoll(createdStr); } catch (...) { u.createdAt = 0; }
            m_users.push_back(u);
        }
    }

    std::ifstream sf(m_authDir / "sessions.db");
    while (std::getline(sf, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        AuthSession s;
        std::string expStr;
        if (std::getline(iss, s.tokenHash, '|') &&
            std::getline(iss, s.userId, '|') &&
            std::getline(iss, s.username, '|') &&
            std::getline(iss, s.role, '|') &&
            std::getline(iss, expStr, '|')) {
            try { s.expiresAt = std::stoll(expStr); } catch (...) { s.expiresAt = 0; }
            m_sessions.push_back(s);
        }
    }
}

void AuthManager::saveStore() const {
    std::ofstream uf(m_authDir / "users.db", std::ios::trunc);
    for (const auto& u : m_users) {
        uf << u.id << "|" << u.username << "|" << u.email << "|"
           << u.salt << "|" << u.passwordHash << "|" << u.role << "|"
           << u.createdAt << "\n";
    }
    std::ofstream sf(m_authDir / "sessions.db", std::ios::trunc);
    for (const auto& s : m_sessions) {
        sf << s.tokenHash << "|" << s.userId << "|" << s.username << "|"
           << s.role << "|" << s.expiresAt << "\n";
    }
}

}
