#pragma once

#include "ai_catalog.h"
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace nodepanda {

struct UsageEntry {
    std::string id;
    std::string runId;
    std::string owner;
    std::string provider;
    std::string requestedModel;
    std::string actualModel;
    TokenUsage usage;
    int64_t costNano = 0;
    int64_t latencyMs = 0;
    bool freeTier = false;
    std::string createdAt;
};

class UsageLedger {
public:
    void init(const std::filesystem::path& dataDir);
    void record(const UsageEntry& entry);
    const std::vector<UsageEntry>& entries() const;
    int64_t totalCostNano() const;
    int64_t totalTokens() const;
    bool exportCsv(const std::filesystem::path& outPath) const;
    void save() const;
    void load();

private:
    std::filesystem::path m_dataDir;
    std::vector<UsageEntry> m_entries;
    mutable std::mutex m_mutex;
};

}
