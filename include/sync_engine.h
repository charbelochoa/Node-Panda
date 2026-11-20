#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nodepanda {

class NoteManager;

struct SyncTransaction {
    int64_t t = 0;
    std::string txId;
    std::string entityId;
    std::string op;
    std::string payloadHash;
    std::string timestamp;
};

struct SyncDiagnostics {
    int64_t localT = 0;
    int64_t remoteT = 0;
    std::string localChecksum;
    std::string remoteChecksum;
    bool inSync = true;
    bool e2eeEnabled = false;
    bool running = false;
    std::string httpBase;
    std::string wsUrl;
    int pendingBatchCount = 0;
};

class SyncEngine {
public:
    void init(const std::filesystem::path& dataDir);
    void configureTransport(const std::string& httpBase, const std::string& wsUrl);
    bool enableE2EE(const std::string& password, std::string& errorOut);
    bool verifyE2EEPassword(const std::string& password) const;
    void recordTransaction(const std::string& entityId, const std::string& op, const std::string& payload);
    std::string computeStateChecksum(const NoteManager& nm) const;
    bool exportSnapshot(const NoteManager& nm, const std::filesystem::path& outPath, bool encrypt, const std::string& password, std::string& errorOut) const;
    bool importSnapshot(NoteManager& nm, const std::filesystem::path& inPath, const std::string& password, std::string& errorOut);
    void startSync();
    void stopSync();
    SyncDiagnostics diagnostics(const NoteManager& nm) const;
    const std::vector<SyncTransaction>& transactions() const { return m_transactions; }

private:
    void loadConfig();
    void saveConfig() const;
    void loadTransactions();
    void appendTransactionToDisk(const SyncTransaction& tx) const;

    static std::string currentIsoTimestamp();
    static std::string sanitizeField(const std::string& s);

    std::filesystem::path m_dataDir;
    std::filesystem::path m_syncDir;
    std::filesystem::path m_configPath;
    std::filesystem::path m_txLogPath;

    int64_t m_localT = 0;
    int64_t m_remoteT = 0;
    std::string m_remoteChecksum;
    bool m_e2eeEnabled = false;
    std::string m_e2eeSalt;
    std::string m_e2eeVerifier;
    bool m_running = false;
    std::string m_httpBase = "http://127.0.0.1:8080";
    std::string m_wsUrl = "ws://127.0.0.1:8080/ws";
    int m_pendingBatchCount = 0;
    std::vector<SyncTransaction> m_transactions;
};

}
