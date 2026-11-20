#pragma once

#include "data_lab.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nodepanda {

struct ProcedureRule {
    std::string key;
    std::string question;
    AnalysisPlan plan;
    int acceptedCount = 0;
    int rejectedCount = 0;
};

struct PolicyVersion {
    std::string id;
    std::string createdAt;
    std::vector<ProcedureRule> procedures;
    double validationScore = 0.0;
};

struct BenchmarkCase {
    std::string id;
    std::string prompt;
    std::string filePath;
    std::string split;
    double expectedTotal = 0.0;
};

struct BenchmarkTrial {
    std::string id;
    std::string caseId;
    std::string versionId;
    std::string strategy;
    bool success = false;
    double score = 0.0;
    int64_t costNano = 0;
    int64_t latencyMs = 0;
};

class EvolutionEngine {
public:
    void init(const std::filesystem::path& dataDir);

    BenchmarkCase addCase(const std::string& prompt, const std::string& filePath, const std::string& split, double expectedTotal);
    BenchmarkTrial runBenchmark(const std::string& caseId, const std::string& strategy, DataLab& lab);
    std::string proposeOptimization(const std::string& mode = "adaptive");
    std::string decidePromotion();
    bool restoreVersion(const std::string& versionId);

    const std::vector<PolicyVersion>& versions() const;
    const std::vector<BenchmarkCase>& cases() const;
    const std::vector<BenchmarkTrial>& trials() const;
    std::string activeVersionId() const;

private:
    AnalysisPlan inferPlanFromPrompt(const std::string& prompt, const std::string& strategy) const;
    void saveState() const;
    void loadState();

    std::filesystem::path m_dataDir;
    std::vector<PolicyVersion> m_versions;
    std::vector<BenchmarkCase> m_cases;
    std::vector<BenchmarkTrial> m_trials;
    std::string m_activeVersionId;
    std::string m_candidateVersionId;
};

}
