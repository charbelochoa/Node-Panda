#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace nodepanda {

enum class ProviderId {
    OpenAI,
    Anthropic,
    Google,
    XAI,
    DeepSeek,
    Nvidia,
    Local
};

enum class ModelTier {
    Low,
    Medium,
    High,
    Reasoning
};

struct ProviderSpec {
    ProviderId id = ProviderId::OpenAI;
    std::string idStr;
    std::string name;
    std::string asciiBadge;
    std::string envVar;
    std::string endpoint;
    bool enabled = true;
    bool freeTier = false;
};

struct ModelSpec {
    std::string id;
    std::string name;
    ProviderId provider = ProviderId::OpenAI;
    ModelTier tier = ModelTier::Low;
    double inputPricePer1M = 0.0;
    double outputPricePer1M = 0.0;
    double cachedPricePer1M = 0.0;
    bool vision = true;
};

struct TokenUsage {
    int64_t input = 0;
    int64_t output = 0;
    int64_t cached = 0;
    int64_t cacheWrite = 0;
    int64_t reasoning = 0;
};

struct AgentSettings {
    bool router = true;
    std::string routerModel = "gpt-5.4-nano";
    bool optimizer = true;
    double budgetUsd = 0.50;
    int maxIterations = 8;
    double targetScore = 90.0;
    int patience = 3;
    double minDelta = 1.0;
    bool allowWrites = false;
    bool allowCommands = false;
    std::string localEndpoint = "http://127.0.0.1:8089/v1/chat/completions";
};

class AiCatalog {
public:
    AiCatalog();

    void init(const std::filesystem::path& dataDir);

    const std::vector<ProviderSpec>& providers() const;
    std::vector<ProviderSpec>& providersMutable();
    const std::vector<ModelSpec>& models() const;

    const ProviderSpec* findProvider(const std::string& idStr) const;
    const ProviderSpec* findProvider(ProviderId id) const;
    const ModelSpec* findModel(const std::string& modelId) const;

    void setApiKey(ProviderId id, const std::string& key);
    std::string getApiKey(ProviderId id) const;
    bool hasApiKey(ProviderId id) const;

    AgentSettings& settings();
    const AgentSettings& settings() const;

    static int64_t calculateCostNano(const TokenUsage& usage, const ModelSpec& model, bool freeTier = false);
    static std::string formatUsdFromNano(int64_t costNano, int precision = 4);
    static std::string evaluateStopPolicy(const std::vector<double>& scores, int64_t costNano, const AgentSettings& cfg);

    void saveConfig() const;
    void loadConfig();

private:
    void populateDefaults();
    std::string configFilePath() const;

    std::filesystem::path m_dataDir;
    std::vector<ProviderSpec> m_providers;
    std::vector<ModelSpec> m_models;
    std::map<ProviderId, std::string> m_apiKeys;
    AgentSettings m_settings;
};

}
