#include "ai_catalog.h"
#include "crypto_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace nodepanda {

namespace {

std::string trimCopy(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string localVaultSecret() {
    return "nodepanda-catalog-v1-local-seal";
}

}

AiCatalog::AiCatalog() {
    populateDefaults();
}

void AiCatalog::populateDefaults() {
    m_providers = {
        { ProviderId::OpenAI,    "openai",    "OpenAI",         "[OAI]", "OPENAI_API_KEY",    "https://api.openai.com/v1/chat/completions", true, false },
        { ProviderId::Anthropic, "anthropic", "Anthropic",      "[ANT]", "ANTHROPIC_API_KEY", "https://api.anthropic.com/v1/messages",        true, false },
        { ProviderId::Google,    "google",    "Google Gemini",  "[GEM]", "GEMINI_API_KEY",    "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions", true, false },
        { ProviderId::XAI,       "xai",       "xAI Grok",       "[XAI]", "XAI_API_KEY",       "https://api.x.ai/v1/chat/completions",       true, false },
        { ProviderId::DeepSeek,  "deepseek",  "DeepSeek",       "[DSK]", "DEEPSEEK_API_KEY",  "https://api.deepseek.com/chat/completions",  true, false },
        { ProviderId::Nvidia,    "nvidia",    "NVIDIA NIM",     "[NIM]", "NVIDIA_API_KEY",    "https://integrate.api.nvidia.com/v1/chat/completions", true, true },
        { ProviderId::Local,     "local",     "Local Runtime",  "[LOC]", "",                  "http://127.0.0.1:8089/v1/chat/completions",  true, true }
    };

    m_models = {
        { "gpt-5.4-nano",               "GPT-5.4 Nano",              ProviderId::OpenAI,    ModelTier::Low,       0.20,  1.25,  0.02, true  },
        { "gpt-5.4-mini",               "GPT-5.4 Mini",              ProviderId::OpenAI,    ModelTier::Medium,    0.75,  4.50,  0.08, true  },
        { "gpt-5.4",                    "GPT-5.4",                   ProviderId::OpenAI,    ModelTier::High,      2.50, 15.00,  0.25, true  },
        { "claude-haiku-4-5",           "Claude Haiku 4.5",          ProviderId::Anthropic, ModelTier::Low,       1.00,  5.00,  0.10, true  },
        { "claude-sonnet-5-5",          "Claude Sonnet 5.5",         ProviderId::Anthropic, ModelTier::High,      3.00, 15.00,  0.30, true  },
        { "gemini-3.1-flash-lite",      "Gemini 3.1 Flash-Lite",     ProviderId::Google,    ModelTier::Low,       0.10,  0.40,  0.025, true },
        { "grok-4.7",                   "Grok 4.7",                  ProviderId::XAI,       ModelTier::High,      3.00, 15.00,  0.75, true  },
        { "deepseek-chat",              "DeepSeek Chat",             ProviderId::DeepSeek,  ModelTier::Medium,    0.27,  1.10,  0.07, false },
        { "deepseek-reasoner",          "DeepSeek Reasoner",         ProviderId::DeepSeek,  ModelTier::Reasoning, 0.55,  2.19,  0.14, false },
        { "meta/llama-3.3-70b-instruct","Llama 3.3 70B (NIM)",       ProviderId::Nvidia,    ModelTier::Medium,    0.00,  0.00,  0.00, false },
        { "qwen2.5-0.5b-instruct",      "Qwen 2.5 0.5B Local",       ProviderId::Local,     ModelTier::Low,       0.00,  0.00,  0.00, false },
        { "llama3.2",                   "Llama 3.2 Local",           ProviderId::Local,     ModelTier::Medium,    0.00,  0.00,  0.00, false }
    };
}

void AiCatalog::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    std::error_code ec;
    if (!m_dataDir.empty() && !std::filesystem::exists(m_dataDir, ec)) {
        std::filesystem::create_directories(m_dataDir, ec);
    }
    loadConfig();
}

const std::vector<ProviderSpec>& AiCatalog::providers() const {
    return m_providers;
}

std::vector<ProviderSpec>& AiCatalog::providersMutable() {
    return m_providers;
}

const std::vector<ModelSpec>& AiCatalog::models() const {
    return m_models;
}

const ProviderSpec* AiCatalog::findProvider(const std::string& idStr) const {
    for (const auto& p : m_providers) {
        if (p.idStr == idStr) return &p;
    }
    return nullptr;
}

const ProviderSpec* AiCatalog::findProvider(ProviderId id) const {
    for (const auto& p : m_providers) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

const ModelSpec* AiCatalog::findModel(const std::string& modelId) const {
    for (const auto& m : m_models) {
        if (m.id == modelId) return &m;
    }
    return nullptr;
}

void AiCatalog::setApiKey(ProviderId id, const std::string& key) {
    std::string cleaned = trimCopy(key);
    if (cleaned.empty()) {
        m_apiKeys.erase(id);
    } else {
        m_apiKeys[id] = cleaned;
    }
}

std::string AiCatalog::getApiKey(ProviderId id) const {
    auto it = m_apiKeys.find(id);
    if (it != m_apiKeys.end() && !it->second.empty()) {
        return it->second;
    }
    const ProviderSpec* spec = findProvider(id);
    if (spec && !spec->envVar.empty()) {
        const char* envVal = std::getenv(spec->envVar.c_str());
        if (envVal && envVal[0] != '\0') {
            return std::string(envVal);
        }
    }
    return "";
}

bool AiCatalog::hasApiKey(ProviderId id) const {
    if (id == ProviderId::Local) return true;
    return !getApiKey(id).empty();
}

AgentSettings& AiCatalog::settings() {
    return m_settings;
}

const AgentSettings& AiCatalog::settings() const {
    return m_settings;
}

int64_t AiCatalog::calculateCostNano(const TokenUsage& usage, const ModelSpec& model, bool freeTier) {
    if (freeTier) return 0;
    int64_t cachedTokens = std::max<int64_t>(0, std::min(usage.cached, usage.input));
    int64_t normalInput = std::max<int64_t>(0, usage.input - cachedTokens);
    int64_t outputTokens = std::max<int64_t>(0, usage.output + usage.reasoning);
    int64_t cacheWriteTokens = std::max<int64_t>(0, usage.cacheWrite);

    double cachedRate = model.cachedPricePer1M > 0.0 ? model.cachedPricePer1M : (model.inputPricePer1M * 0.25);
    double cacheWriteRate = model.inputPricePer1M * 1.25;

    double nanoCost = (static_cast<double>(normalInput) * model.inputPricePer1M
                    + static_cast<double>(cachedTokens) * cachedRate
                    + static_cast<double>(cacheWriteTokens) * cacheWriteRate
                    + static_cast<double>(outputTokens) * model.outputPricePer1M) * 1000.0;

    if (nanoCost < 0.0) nanoCost = 0.0;
    return static_cast<int64_t>(std::llround(nanoCost));
}

std::string AiCatalog::formatUsdFromNano(int64_t costNano, int precision) {
    double usd = static_cast<double>(costNano) / 1000000000.0;
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(std::max(0, precision)) << usd;
    return oss.str();
}

std::string AiCatalog::evaluateStopPolicy(const std::vector<double>& scores, int64_t costNano, const AgentSettings& cfg) {
    if (cfg.budgetUsd > 0.0) {
        int64_t budgetNano = static_cast<int64_t>(std::llround(cfg.budgetUsd * 1000000000.0));
        if (costNano >= budgetNano) {
            return "budget_exhausted";
        }
    }
    if (!scores.empty() && scores.back() >= cfg.targetScore) {
        return "target_reached";
    }
    if (cfg.maxIterations > 0 && static_cast<int>(scores.size()) >= cfg.maxIterations) {
        return "max_iterations";
    }
    if (cfg.patience > 0 && static_cast<int>(scores.size()) > cfg.patience) {
        size_t cutoff = scores.size() - static_cast<size_t>(cfg.patience);
        double bestBefore = scores[0];
        for (size_t i = 1; i < cutoff; ++i) {
            if (scores[i] > bestBefore) bestBefore = scores[i];
        }
        double bestRecent = scores[cutoff];
        for (size_t i = cutoff + 1; i < scores.size(); ++i) {
            if (scores[i] > bestRecent) bestRecent = scores[i];
        }
        if ((bestRecent - bestBefore) < cfg.minDelta) {
            return "plateau";
        }
    }
    return "";
}

std::string AiCatalog::configFilePath() const {
    if (m_dataDir.empty()) return "";
    return (m_dataDir / "ai_catalog.cfg").string();
}

void AiCatalog::saveConfig() const {
    std::string path = configFilePath();
    if (path.empty()) return;

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;

    out << "router=" << (m_settings.router ? "1" : "0") << "\n";
    out << "routerModel=" << m_settings.routerModel << "\n";
    out << "optimizer=" << (m_settings.optimizer ? "1" : "0") << "\n";
    out << "budgetUsd=" << std::fixed << std::setprecision(6) << m_settings.budgetUsd << "\n";
    out << "maxIterations=" << m_settings.maxIterations << "\n";
    out << "targetScore=" << std::fixed << std::setprecision(4) << m_settings.targetScore << "\n";
    out << "patience=" << m_settings.patience << "\n";
    out << "minDelta=" << std::fixed << std::setprecision(4) << m_settings.minDelta << "\n";
    out << "allowWrites=" << (m_settings.allowWrites ? "1" : "0") << "\n";
    out << "allowCommands=" << (m_settings.allowCommands ? "1" : "0") << "\n";
    out << "localEndpoint=" << m_settings.localEndpoint << "\n";

    for (const auto& p : m_providers) {
        out << "provider." << p.idStr << ".enabled=" << (p.enabled ? "1" : "0") << "\n";
        out << "provider." << p.idStr << ".freeTier=" << (p.freeTier ? "1" : "0") << "\n";
        out << "provider." << p.idStr << ".endpoint=" << p.endpoint << "\n";
        auto it = m_apiKeys.find(p.id);
        if (it != m_apiKeys.end() && !it->second.empty()) {
            std::string sealed = CryptoEngine::encryptPayload(it->second, localVaultSecret());
            out << "provider." << p.idStr << ".key=" << sealed << "\n";
        }
    }
}

void AiCatalog::loadConfig() {
    std::string path = configFilePath();
    if (path.empty()) return;

    std::ifstream in(path);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        line = trimCopy(line);
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trimCopy(line.substr(0, eq));
        std::string val = trimCopy(line.substr(eq + 1));

        if (key == "router") m_settings.router = (val == "1" || val == "true");
        else if (key == "routerModel" && !val.empty()) m_settings.routerModel = val;
        else if (key == "optimizer") m_settings.optimizer = (val == "1" || val == "true");
        else if (key == "budgetUsd") { try { m_settings.budgetUsd = std::stod(val); } catch (...) {} }
        else if (key == "maxIterations") { try { m_settings.maxIterations = std::stoi(val); } catch (...) {} }
        else if (key == "targetScore") { try { m_settings.targetScore = std::stod(val); } catch (...) {} }
        else if (key == "patience") { try { m_settings.patience = std::stoi(val); } catch (...) {} }
        else if (key == "minDelta") { try { m_settings.minDelta = std::stod(val); } catch (...) {} }
        else if (key == "allowWrites") m_settings.allowWrites = (val == "1" || val == "true");
        else if (key == "allowCommands") m_settings.allowCommands = (val == "1" || val == "true");
        else if (key == "localEndpoint" && !val.empty()) m_settings.localEndpoint = val;
        else if (key.rfind("provider.", 0) == 0) {
            std::string rest = key.substr(9);
            size_t dot = rest.find('.');
            if (dot == std::string::npos) continue;
            std::string pid = rest.substr(0, dot);
            std::string field = rest.substr(dot + 1);
            for (auto& p : m_providers) {
                if (p.idStr != pid) continue;
                if (field == "enabled") p.enabled = (val == "1" || val == "true");
                else if (field == "freeTier") p.freeTier = (val == "1" || val == "true");
                else if (field == "endpoint" && !val.empty()) p.endpoint = val;
                else if (field == "key" && !val.empty()) {
                    bool ok = false;
                    std::string plain = CryptoEngine::decryptPayload(val, localVaultSecret(), ok);
                    if (ok && !plain.empty()) {
                        m_apiKeys[p.id] = plain;
                    }
                }
            }
        }
    }

    for (auto& p : m_providers) {
        if (p.id == ProviderId::Local && !m_settings.localEndpoint.empty()) {
            p.endpoint = m_settings.localEndpoint;
        }
    }
}

}
