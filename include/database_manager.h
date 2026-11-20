#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace nodepanda {

enum class DbEngineType {
    LocalOnly,
    PostgreSQL,
    MySQL
};

enum class DbDeployMode {
    Local,
    Server
};

struct DbConfig {
    DbEngineType engine = DbEngineType::PostgreSQL;
    DbDeployMode deployMode = DbDeployMode::Local;
    std::string host = "127.0.0.1";
    int port = 5432;
    std::string database = "nodepanda";
    std::string username = "nodepanda";
    std::string password;
    bool useTls = false;
    bool autoSync = true;
};

struct DbQueryResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    int affectedRows = 0;
    double latencyMs = 0.0;
};

struct DbStatusInfo {
    bool connected = false;
    bool tcpReachable = false;
    bool cliDriverAvailable = false;
    std::string engineName;
    std::string modeName;
    std::string serverVersion;
    std::string lastError;
    int syncedNotes = 0;
    int syncedBlocks = 0;
    int syncedTasks = 0;
    int syncedRuns = 0;
};

class NoteManager;

class DatabaseManager {
public:
    void init(const std::filesystem::path& dataDir);
    void configure(const DbConfig& cfg);
    const DbConfig& config() const { return m_config; }
    bool loadConfig();
    bool saveConfig() const;

    bool testConnection(std::string& detailsOut);
    bool initializeSchema(std::string& errorOut);
    DbQueryResult executeSql(const std::string& sql);

    bool syncNotes(const NoteManager& nm, std::string& summaryOut);
    bool upsertNoteRecord(const std::string& id, const std::string& title,
                          const std::string& content, const std::string& frontmatterJson);
    bool upsertBlockRecord(const std::string& uuid, const std::string& pageId,
                           const std::string& parentUuid, int orderIndex,
                           const std::string& content, const std::string& propsJson);
    bool upsertTaskRecord(const std::string& uuid, const std::string& pageId,
                          const std::string& status, const std::string& priority,
                          const std::string& title, const std::string& scheduled,
                          const std::string& deadline);
    bool insertAgentRunRecord(const std::string& runId, const std::string& owner,
                              const std::string& prompt, const std::string& model,
                              const std::string& status, double costUsd);
    bool insertUsageLedgerRecord(const std::string& id, const std::string& runId,
                                 const std::string& provider, const std::string& model,
                                 int inputTokens, int outputTokens, double costUsd);
    bool upsertUserRecord(const std::string& id, const std::string& username,
                          const std::string& email, const std::string& salt,
                          const std::string& hash, const std::string& role);

    DbStatusInfo getStatus() const { return m_status; }
    std::string exportSchemaSql(DbEngineType engine) const;
    static std::string escapeSql(const std::string& value);
    static std::string engineToString(DbEngineType engine);
    static DbEngineType stringToEngine(const std::string& s);

private:
    bool probeTcpPort(const std::string& host, int port, int timeoutMs, std::string& bannerOut);
    bool probePostgresHandshake(const std::string& host, int port, std::string& infoOut);
    bool probeMysqlHandshake(const std::string& host, int port, std::string& bannerOut);
    std::string locateDriverBinary(DbEngineType engine) const;
    DbQueryResult executeViaPostgresCli(const std::string& sql);
    DbQueryResult executeViaMysqlCli(const std::string& sql);
    void appendWalEntry(const std::string& sql);

    std::filesystem::path m_dataDir;
    std::filesystem::path m_configPath;
    std::filesystem::path m_walPath;
    DbConfig m_config;
    DbStatusInfo m_status;
};

}
