#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nodepanda {

struct UserAccount {
    std::string id;
    std::string username;
    std::string email;
    std::string salt;
    std::string passwordHash;
    std::string role;
    int64_t createdAt = 0;
};

struct AuthSession {
    std::string tokenHash;
    std::string userId;
    std::string username;
    std::string role;
    int64_t expiresAt = 0;
};

class AuthManager {
public:
    void init(const std::filesystem::path& dataDir);
    bool hasUsers() const;
    bool registerUser(const std::string& username, const std::string& email,
                      const std::string& password, std::string& errorOut,
                      const std::string& role = "admin");
    bool authenticate(const std::string& identifier, const std::string& password,
                      UserAccount& outUser, std::string& errorOut);
    std::string createSession(const UserAccount& user);
    bool validateToken(const std::string& rawToken, AuthSession& outSession) const;
    void revokeToken(const std::string& rawToken);
    bool loadSavedCliSession(AuthSession& outSession) const;
    void saveCliToken(const std::string& rawToken) const;
    void clearCliToken() const;
    bool interactiveTerminalLogin(UserAccount& outUser, std::string& outToken);
    static std::string readPasswordMasked(const std::string& prompt);
    const UserAccount* currentUser() const { return m_hasActiveUser ? &m_activeUser : nullptr; }
    void setActiveUser(const UserAccount& user) { m_activeUser = user; m_hasActiveUser = true; }
    const std::vector<UserAccount>& getUsers() const { return m_users; }
    std::string getCliTokenPath() const { return (m_authDir / "cli-token").string(); }

private:
    void loadStore();
    void saveStore() const;
    static int64_t nowUnix();

    std::filesystem::path m_authDir;
    std::vector<UserAccount> m_users;
    std::vector<AuthSession> m_sessions;
    UserAccount m_activeUser;
    bool m_hasActiveUser = false;
};

}
