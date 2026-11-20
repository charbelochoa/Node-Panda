#include "asset_manager.h"
#include "crypto_engine.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>

namespace nodepanda {

std::string AssetManager::currentIsoTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmInfo{};
#ifdef _WIN32
    localtime_s(&tmInfo, &t);
#else
    localtime_r(&t, &tmInfo);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmInfo);
    return std::string(buf);
}

std::string AssetManager::sanitizeFilename(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == '\t' || c == '\n' || c == '\r') {
            out.push_back('_');
        } else {
            out.push_back(c);
        }
    }
    return out.empty() ? "asset" : out;
}

void AssetManager::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    m_assetsDir = m_dataDir / "assets";
    m_manifestPath = m_assetsDir / "manifest.tsv";
    if (!std::filesystem::exists(m_assetsDir)) {
        std::filesystem::create_directories(m_assetsDir);
    }
    loadManifest();
    scanAssets();
}

std::filesystem::path AssetManager::resolveAssetFilePath(const AssetRecord& rec) const {
    auto prefixed = m_assetsDir / (rec.uuid + "_" + rec.filename);
    if (std::filesystem::exists(prefixed)) {
        return prefixed;
    }
    auto direct = m_assetsDir / rec.filename;
    if (std::filesystem::exists(direct)) {
        return direct;
    }
    auto byUuidExt = m_assetsDir / (rec.uuid + rec.extension);
    if (std::filesystem::exists(byUuidExt)) {
        return byUuidExt;
    }
    return prefixed;
}

bool AssetManager::computeFileHash(const std::filesystem::path& filePath, std::string& hashOut, size_t& sizeOut) const {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    sizeOut = data.size();
    auto digest = CryptoEngine::digestBytes(data.empty() ? nullptr : data.data(), data.size());
    hashOut = CryptoEngine::toHex(digest.data(), digest.size());
    return true;
}

void AssetManager::loadManifest() {
    m_assets.clear();
    if (m_manifestPath.empty() || !std::filesystem::exists(m_manifestPath)) return;
    std::ifstream in(m_manifestPath);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string uuid, filename, ext, sha, sizeStr, linked, created, verStr;
        if (!std::getline(ss, uuid, '\t')) continue;
        if (!std::getline(ss, filename, '\t')) continue;
        if (!std::getline(ss, ext, '\t')) continue;
        if (!std::getline(ss, sha, '\t')) continue;
        if (!std::getline(ss, sizeStr, '\t')) continue;
        std::getline(ss, linked, '\t');
        std::getline(ss, created, '\t');
        std::getline(ss, verStr, '\t');
        if (uuid.empty() || filename.empty()) continue;

        AssetRecord rec;
        rec.uuid = uuid;
        rec.filename = filename;
        rec.extension = ext;
        rec.sha256 = sha;
        try {
            rec.sizeBytes = static_cast<size_t>(std::stoull(sizeStr));
        } catch (...) {
            rec.sizeBytes = 0;
        }
        rec.linkedNoteId = linked;
        rec.createdAt = created;
        rec.verified = (verStr != "0");
        m_assets.push_back(std::move(rec));
    }
}

void AssetManager::saveManifest() const {
    if (m_manifestPath.empty()) return;
    if (!std::filesystem::exists(m_assetsDir)) {
        std::filesystem::create_directories(m_assetsDir);
    }
    std::ofstream out(m_manifestPath, std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& rec : m_assets) {
        out << rec.uuid << '\t'
            << rec.filename << '\t'
            << rec.extension << '\t'
            << rec.sha256 << '\t'
            << rec.sizeBytes << '\t'
            << rec.linkedNoteId << '\t'
            << rec.createdAt << '\t'
            << (rec.verified ? "1" : "0") << '\n';
    }
}

void AssetManager::scanAssets() {
    if (m_assetsDir.empty() || !std::filesystem::exists(m_assetsDir)) return;

    std::set<std::string> trackedPaths;
    for (auto& rec : m_assets) {
        auto p = resolveAssetFilePath(rec);
        if (std::filesystem::exists(p) && std::filesystem::is_regular_file(p)) {
            std::error_code ec;
            auto canon = std::filesystem::weakly_canonical(p, ec);
            trackedPaths.insert((ec ? p : canon).generic_string());
            std::string currentHash;
            size_t currentSize = 0;
            if (computeFileHash(p, currentHash, currentSize)) {
                rec.sizeBytes = currentSize;
                rec.verified = CryptoEngine::secureEquals(rec.sha256, currentHash);
            } else {
                rec.verified = false;
            }
        } else {
            rec.verified = false;
        }
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(m_assetsDir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().filename() == "manifest.tsv") continue;

        auto canon = std::filesystem::weakly_canonical(entry.path(), ec);
        std::string key = (ec ? entry.path() : canon).generic_string();
        if (trackedPaths.count(key) > 0) continue;

        std::string hash;
        size_t sz = 0;
        if (!computeFileHash(entry.path(), hash, sz)) continue;

        AssetRecord rec;
        rec.uuid = CryptoEngine::generateUuid();
        rec.filename = entry.path().filename().string();
        rec.extension = entry.path().extension().string();
        rec.sha256 = hash;
        rec.sizeBytes = sz;
        rec.linkedNoteId = "";
        rec.createdAt = currentIsoTimestamp();
        rec.verified = true;
        m_assets.push_back(std::move(rec));
    }

    saveManifest();
}

AssetRecord AssetManager::importAsset(const std::filesystem::path& sourcePath, const std::string& linkedNoteId, std::string& errorOut) {
    errorOut.clear();
    AssetRecord empty;
    if (sourcePath.empty() || !std::filesystem::exists(sourcePath)) {
        errorOut = "Source file does not exist";
        return empty;
    }
    if (!std::filesystem::is_regular_file(sourcePath)) {
        errorOut = "Source path is not a regular file";
        return empty;
    }
    if (m_assetsDir.empty()) {
        errorOut = "AssetManager is not initialized";
        return empty;
    }
    if (!std::filesystem::exists(m_assetsDir)) {
        std::filesystem::create_directories(m_assetsDir);
    }

    std::string hash;
    size_t sz = 0;
    if (!computeFileHash(sourcePath, hash, sz)) {
        errorOut = "Failed to read source file";
        return empty;
    }

    AssetRecord rec;
    rec.uuid = CryptoEngine::generateUuid();
    rec.filename = sanitizeFilename(sourcePath.filename().string());
    rec.extension = sourcePath.extension().string();
    rec.sha256 = hash;
    rec.sizeBytes = sz;
    rec.linkedNoteId = linkedNoteId;
    rec.createdAt = currentIsoTimestamp();
    rec.verified = true;

    std::filesystem::path destPath = m_assetsDir / (rec.uuid + "_" + rec.filename);
    std::error_code ec;
    std::filesystem::copy_file(sourcePath, destPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        errorOut = "Failed to copy asset: " + ec.message();
        return empty;
    }

    m_assets.push_back(rec);
    saveManifest();
    return rec;
}

bool AssetManager::verifyAsset(const std::string& uuid) {
    for (auto& rec : m_assets) {
        if (rec.uuid == uuid) {
            auto p = resolveAssetFilePath(rec);
            std::string currentHash;
            size_t currentSize = 0;
            if (!std::filesystem::exists(p) || !computeFileHash(p, currentHash, currentSize)) {
                rec.verified = false;
            } else {
                rec.sizeBytes = currentSize;
                rec.verified = CryptoEngine::secureEquals(rec.sha256, currentHash);
            }
            saveManifest();
            return rec.verified;
        }
    }
    return false;
}

bool AssetManager::verifyAll() {
    bool allOk = true;
    for (auto& rec : m_assets) {
        auto p = resolveAssetFilePath(rec);
        std::string currentHash;
        size_t currentSize = 0;
        if (!std::filesystem::exists(p) || !computeFileHash(p, currentHash, currentSize)) {
            rec.verified = false;
            allOk = false;
        } else {
            rec.sizeBytes = currentSize;
            rec.verified = CryptoEngine::secureEquals(rec.sha256, currentHash);
            if (!rec.verified) allOk = false;
        }
    }
    saveManifest();
    return allOk;
}

bool AssetManager::removeAsset(const std::string& uuid) {
    for (auto it = m_assets.begin(); it != m_assets.end(); ++it) {
        if (it->uuid == uuid) {
            auto p = resolveAssetFilePath(*it);
            std::error_code ec;
            if (std::filesystem::exists(p, ec)) {
                std::filesystem::remove(p, ec);
            }
            m_assets.erase(it);
            saveManifest();
            return true;
        }
    }
    return false;
}

const AssetRecord* AssetManager::findByUuid(const std::string& uuid) const {
    for (const auto& rec : m_assets) {
        if (rec.uuid == uuid) return &rec;
    }
    return nullptr;
}

}
