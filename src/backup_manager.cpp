#include "backup_manager.h"
#include "crypto_engine.h"
#include "note_manager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>

namespace nodepanda {

namespace {

std::string formatNow(const char* fmt) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmInfo{};
#ifdef _WIN32
    localtime_s(&tmInfo, &t);
#else
    localtime_r(&t, &tmInfo);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), fmt, &tmInfo);
    return std::string(buf);
}

}

bool BackupManager::isSafeBackupName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return false;
    if (name.find("..") != std::string::npos) return false;
    return true;
}

std::string BackupManager::sanitizeToken(const std::string& token) {
    std::string out;
    out.reserve(token.size());
    for (unsigned char c : token) {
        if (std::isalnum(c) || c == '-' || c == '_') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('-');
        }
    }
    return out;
}

std::string BackupManager::computeDirectoryChecksum(const std::filesystem::path& notesDir, int& fileCountOut, size_t& totalBytesOut) {
    fileCountOut = 0;
    totalBytesOut = 0;
    if (!std::filesystem::exists(notesDir)) {
        return CryptoEngine::sha256("");
    }

    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(notesDir, ec)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    std::ostringstream acc;
    for (const auto& f : files) {
        auto rel = std::filesystem::relative(f, notesDir, ec);
        std::string relStr = ec ? f.filename().generic_string() : rel.generic_string();
        std::ifstream in(f, std::ios::binary);
        if (!in.is_open()) continue;
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ++fileCountOut;
        totalBytesOut += content.size();
        acc << relStr << '\0' << content << '\0';
    }
    return CryptoEngine::sha256(acc.str());
}

void BackupManager::writeMetadata(const BackupInfo& info) {
    std::ofstream out(info.path / "metadata.tsv", std::ios::trunc);
    if (!out.is_open()) return;
    out << "name\t" << info.name << '\n';
    out << "source\t" << info.source << '\n';
    out << "createdAt\t" << info.createdAt << '\n';
    out << "checksum\t" << info.checksum << '\n';
    out << "fileCount\t" << info.fileCount << '\n';
    out << "totalBytes\t" << info.totalBytes << '\n';
}

bool BackupManager::readMetadata(const std::filesystem::path& backupDir, BackupInfo& out) {
    out = BackupInfo{};
    out.path = backupDir;
    out.name = backupDir.filename().string();

    auto metaPath = backupDir / "metadata.tsv";
    if (!std::filesystem::exists(metaPath)) {
        out.source = "unknown";
        out.createdAt = "";
        out.checksum = computeDirectoryChecksum(backupDir / "notes", out.fileCount, out.totalBytes);
        return std::filesystem::exists(backupDir / "notes");
    }

    std::ifstream in(metaPath);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string key = line.substr(0, tab);
        std::string val = line.substr(tab + 1);
        if (key == "name") out.name = val;
        else if (key == "source") out.source = val;
        else if (key == "createdAt") out.createdAt = val;
        else if (key == "checksum") out.checksum = val;
        else if (key == "fileCount") {
            try { out.fileCount = std::stoi(val); } catch (...) { out.fileCount = 0; }
        } else if (key == "totalBytes") {
            try { out.totalBytes = static_cast<size_t>(std::stoull(val)); } catch (...) { out.totalBytes = 0; }
        }
    }
    return true;
}

void BackupManager::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    m_backupsDir = m_dataDir / "backups";
    if (!std::filesystem::exists(m_backupsDir)) {
        std::filesystem::create_directories(m_backupsDir);
    }
}

BackupInfo BackupManager::createBackup(const NoteManager& nm, const std::string& label, const std::string& source) {
    if (m_backupsDir.empty()) {
        m_backupsDir = std::filesystem::path("data") / "backups";
    }
    if (!std::filesystem::exists(m_backupsDir)) {
        std::filesystem::create_directories(m_backupsDir);
    }

    std::string cleanSource = sanitizeToken(source);
    if (cleanSource.empty()) cleanSource = "manual";
    std::string cleanLabel = sanitizeToken(label);

    std::string stamp = formatNow("%Y%m%d_%H%M%S");
    std::string baseName = stamp + "_" + cleanSource;
    if (!cleanLabel.empty()) {
        baseName += "_" + cleanLabel;
    }

    std::string finalName = baseName;
    int suffix = 1;
    while (std::filesystem::exists(m_backupsDir / finalName)) {
        finalName = baseName + "_" + std::to_string(suffix++);
    }

    BackupInfo info;
    info.name = finalName;
    info.source = cleanSource;
    info.createdAt = formatNow("%Y-%m-%dT%H:%M:%S");
    info.path = m_backupsDir / finalName;

    std::filesystem::path notesDest = info.path / "notes";
    std::filesystem::create_directories(notesDest);

    const auto& srcNotesDir = nm.getNotesDirectory();
    std::error_code ec;
    if (!srcNotesDir.empty() && std::filesystem::exists(srcNotesDir, ec)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(srcNotesDir, ec)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".md") continue;
            auto rel = std::filesystem::relative(entry.path(), srcNotesDir, ec);
            if (ec) continue;
            auto destFile = notesDest / rel;
            std::filesystem::create_directories(destFile.parent_path(), ec);
            std::filesystem::copy_file(entry.path(), destFile, std::filesystem::copy_options::overwrite_existing, ec);
        }
    }

    for (const auto& [id, note] : nm.getAllNotes()) {
        std::filesystem::path relPath = id + ".md";
        if (!note.filepath.empty() && !srcNotesDir.empty()) {
            std::error_code rec;
            auto r = std::filesystem::relative(note.filepath, srcNotesDir, rec);
            if (!rec && !r.empty()) {
                relPath = r;
            }
        }
        auto destFile = notesDest / relPath;
        if (note.dirty || !std::filesystem::exists(destFile)) {
            std::filesystem::create_directories(destFile.parent_path(), ec);
            std::ofstream out(destFile, std::ios::binary | std::ios::trunc);
            if (out.is_open()) {
                out << note.serializeFrontmatter() << note.content;
            }
        }
    }

    info.checksum = computeDirectoryChecksum(notesDest, info.fileCount, info.totalBytes);
    writeMetadata(info);
    return info;
}

std::vector<BackupInfo> BackupManager::listBackups() const {
    std::vector<BackupInfo> result;
    if (m_backupsDir.empty() || !std::filesystem::exists(m_backupsDir)) return result;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(m_backupsDir, ec)) {
        if (!entry.is_directory()) continue;
        BackupInfo info;
        if (readMetadata(entry.path(), info)) {
            result.push_back(std::move(info));
        }
    }

    std::sort(result.begin(), result.end(), [](const BackupInfo& a, const BackupInfo& b) {
        if (a.createdAt != b.createdAt) return a.createdAt > b.createdAt;
        return a.name > b.name;
    });
    return result;
}

bool BackupManager::restoreBackup(const std::string& backupName, NoteManager& nm, std::string& errorOut) {
    errorOut.clear();
    if (!isSafeBackupName(backupName)) {
        errorOut = "Invalid backup name";
        return false;
    }

    std::filesystem::path backupDir = m_backupsDir / backupName;
    std::filesystem::path backupNotesDir = backupDir / "notes";
    if (!std::filesystem::exists(backupNotesDir) || !std::filesystem::is_directory(backupNotesDir)) {
        errorOut = "Backup not found: " + backupName;
        return false;
    }

    BackupInfo meta;
    if (readMetadata(backupDir, meta) && !meta.checksum.empty()) {
        int fc = 0;
        size_t tb = 0;
        std::string actual = computeDirectoryChecksum(backupNotesDir, fc, tb);
        if (!CryptoEngine::secureEquals(meta.checksum, actual)) {
            errorOut = "Backup integrity check failed (checksum mismatch)";
            return false;
        }
    }

    const auto& destNotesDir = nm.getNotesDirectory();
    if (destNotesDir.empty()) {
        errorOut = "Notes directory is not configured";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(destNotesDir, ec)) {
        std::filesystem::create_directories(destNotesDir, ec);
    }

    std::vector<std::filesystem::path> existingMd;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(destNotesDir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            existingMd.push_back(entry.path());
        }
    }
    for (const auto& p : existingMd) {
        std::filesystem::remove(p, ec);
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(backupNotesDir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto rel = std::filesystem::relative(entry.path(), backupNotesDir, ec);
        if (ec) continue;
        auto dst = destNotesDir / rel;
        std::filesystem::create_directories(dst.parent_path(), ec);
        std::filesystem::copy_file(entry.path(), dst, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            errorOut = "Failed to restore file: " + ec.message();
            return false;
        }
    }

    nm.scanDirectory();
    nm.rebuildAllLinks();
    return true;
}

bool BackupManager::removeBackup(const std::string& backupName, std::string& errorOut) {
    errorOut.clear();
    if (!isSafeBackupName(backupName)) {
        errorOut = "Invalid backup name";
        return false;
    }
    std::filesystem::path target = m_backupsDir / backupName;
    if (!std::filesystem::exists(target)) {
        errorOut = "Backup does not exist";
        return false;
    }
    std::error_code ec;
    std::filesystem::remove_all(target, ec);
    if (ec) {
        errorOut = "Failed to delete backup: " + ec.message();
        return false;
    }
    return true;
}

void BackupManager::pruneAutomatic(int keepCount) {
    if (keepCount < 0) keepCount = 0;
    auto all = listBackups();
    int keptAuto = 0;
    for (const auto& b : all) {
        if (b.source == "manual") continue;
        if (keptAuto < keepCount) {
            ++keptAuto;
        } else {
            std::string err;
            removeBackup(b.name, err);
        }
    }
}

}
