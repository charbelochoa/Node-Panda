#pragma once

#include "ai_catalog.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodepanda {

struct ChatMsg {
    std::string role;
    std::string content;
};

struct CompletionResult {
    bool ok = false;
    std::string text;
    std::string actualModel;
    std::string error;
    TokenUsage usage;
    int64_t latencyMs = 0;
    bool usedLocalFallback = false;
};

class AiProviderClient {
public:
    static CompletionResult complete(
        const AiCatalog& catalog,
        const ModelSpec& model,
        const std::vector<ChatMsg>& messages,
        int maxOutputTokens = 2048);

    static CompletionResult httpPostJson(
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& jsonBody,
        int timeoutMs = 45000);

    static std::string jsonEscape(const std::string& s);
    static std::string extractJsonStringField(const std::string& json, const std::string& key);
    static int64_t extractJsonIntField(const std::string& json, const std::string& key);
};

}
