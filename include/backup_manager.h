#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace nodepanda {

class NoteManager;

struct BackupInfo {
    std::string name;
    std::string source;
    std::string createdAt;
    std::string checksum;
    int fileCount = 0;
    size_t totalBytes = 0;
    std::filesystem::path path;
};

class BackupManager {
public:
    void init(const std::filesystem::path& dataDir);
    BackupInfo createBackup(const NoteManager& nm, const std::string& label = "", const std::string& source = "manual");
    std::vector<BackupInfo> listBackups() const;
    bool restoreBackup(const std::string& backupName, NoteManager& nm, std::string& errorOut);
    bool removeBackup(const std::string& backupName, std::string& errorOut);
    void pruneAutomatic(int keepCount = 12);

private:
    static bool isSafeBackupName(const std::string& name);
    static std::string sanitizeToken(const std::string& token);
    static std::string computeDirectoryChecksum(const std::filesystem::path& notesDir, int& fileCountOut, size_t& totalBytesOut);
    static void writeMetadata(const BackupInfo& info);
    static bool readMetadata(const std::filesystem::path& backupDir, BackupInfo& out);

    std::filesystem::path m_dataDir;
    std::filesystem::path m_backupsDir;
};

}
