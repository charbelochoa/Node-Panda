#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace nodepanda {

struct AssetRecord {
    std::string uuid;
    std::string filename;
    std::string extension;
    std::string sha256;
    size_t sizeBytes = 0;
    std::string linkedNoteId;
    std::string createdAt;
    bool verified = true;
};

class AssetManager {
public:
    void init(const std::filesystem::path& dataDir);
    void scanAssets();
    AssetRecord importAsset(const std::filesystem::path& sourcePath, const std::string& linkedNoteId, std::string& errorOut);
    bool verifyAsset(const std::string& uuid);
    bool verifyAll();
    bool removeAsset(const std::string& uuid);
    const std::vector<AssetRecord>& getAssets() const { return m_assets; }
    const AssetRecord* findByUuid(const std::string& uuid) const;
    std::filesystem::path getAssetsDir() const { return m_assetsDir; }

private:
    std::filesystem::path resolveAssetFilePath(const AssetRecord& rec) const;
    bool computeFileHash(const std::filesystem::path& filePath, std::string& hashOut, size_t& sizeOut) const;
    void loadManifest();
    void saveManifest() const;

    static std::string currentIsoTimestamp();
    static std::string sanitizeFilename(const std::string& name);

    std::filesystem::path m_dataDir;
    std::filesystem::path m_assetsDir;
    std::filesystem::path m_manifestPath;
    std::vector<AssetRecord> m_assets;
};

}
