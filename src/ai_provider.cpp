#include "ai_provider.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace nodepanda {

namespace {

struct ParsedUrl {
    bool valid = false;
    bool secure = false;
    std::string host;
    int port = 80;
    std::string path = "/";
};

ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl out;
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return out;

    std::string scheme = url.substr(0, schemeEnd);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (scheme == "https") {
        out.secure = true;
        out.port = 443;
    } else if (scheme == "http") {
        out.secure = false;
        out.port = 80;
    } else {
        return out;
    }

    size_t hostStart = schemeEnd + 3;
    size_t pathStart = url.find('/', hostStart);
    std::string authority;
    if (pathStart == std::string::npos) {
        authority = url.substr(hostStart);
        out.path = "/";
    } else {
        authority = url.substr(hostStart, pathStart - hostStart);
        out.path = url.substr(pathStart);
    }

    size_t colonPos = authority.rfind(':');
    if (colonPos != std::string::npos && authority.find(']') == std::string::npos) {
        out.host = authority.substr(0, colonPos);
        try {
            out.port = std::stoi(authority.substr(colonPos + 1));
        } catch (...) {
            return out;
        }
    } else {
        out.host = authority;
    }

    out.valid = !out.host.empty() && out.port > 0 && out.port <= 65535;
    return out;
}

#ifdef _WIN32
std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring w(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), needed);
    return w;
}
#endif

std::string unescapeJsonString(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char next = raw[++i];
            switch (next) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (i + 4 < raw.size()) {
                        std::string hex = raw.substr(i + 1, 4);
                        unsigned int code = 0;
                        std::istringstream iss(hex);
                        iss >> std::hex >> code;
                        if (code < 0x80) {
                            out.push_back(static_cast<char>(code));
                        } else if (code < 0x800) {
                            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                        i += 4;
                    }
                    break;
                }
                default:
                    out.push_back(next);
                    break;
            }
        } else {
            out.push_back(raw[i]);
        }
    }
    return out;
}

std::string extractJsonStringFromOffset(const std::string& json, const std::string& key, size_t startPos) {
    std::string needle = "\"" + key + "\"";
    size_t pos = startPos;
    while (pos < json.size()) {
        pos = json.find(needle, pos);
        if (pos == std::string::npos) return "";
        size_t afterKey = pos + needle.size();
        while (afterKey < json.size() && std::isspace(static_cast<unsigned char>(json[afterKey]))) {
            ++afterKey;
        }
        if (afterKey >= json.size() || json[afterKey] != ':') {
            pos = afterKey;
            continue;
        }
        size_t valStart = afterKey + 1;
        while (valStart < json.size() && std::isspace(static_cast<unsigned char>(json[valStart]))) {
            ++valStart;
        }
        if (valStart >= json.size() || json[valStart] != '"') {
            pos = valStart;
            continue;
        }
        size_t i = valStart + 1;
        while (i < json.size()) {
            if (json[i] == '\\') {
                i += 2;
                continue;
            }
            if (json[i] == '"') {
                return unescapeJsonString(json.substr(valStart + 1, i - valStart - 1));
            }
            ++i;
        }
        return "";
    }
    return "";
}

int64_t extractJsonIntFromOffset(const std::string& json, const std::string& key, size_t startPos) {
    std::string needle = "\"" + key + "\"";
    size_t pos = startPos;
    while (pos < json.size()) {
        pos = json.find(needle, pos);
        if (pos == std::string::npos) return 0;
        size_t afterKey = pos + needle.size();
        while (afterKey < json.size() && std::isspace(static_cast<unsigned char>(json[afterKey]))) {
            ++afterKey;
        }
        if (afterKey >= json.size() || json[afterKey] != ':') {
            pos = afterKey;
            continue;
        }
        size_t valStart = afterKey + 1;
        while (valStart < json.size() && std::isspace(static_cast<unsigned char>(json[valStart]))) {
            ++valStart;
        }
        if (valStart >= json.size()) return 0;
        bool neg = false;
        if (json[valStart] == '-') {
            neg = true;
            ++valStart;
        }
        if (valStart >= json.size() || !std::isdigit(static_cast<unsigned char>(json[valStart]))) {
            pos = valStart;
            continue;
        }
        int64_t val = 0;
        while (valStart < json.size() && std::isdigit(static_cast<unsigned char>(json[valStart]))) {
            val = val * 10 + (json[valStart] - '0');
            ++valStart;
        }
        return neg ? -val : val;
    }
    return 0;
}

int64_t estimateTokens(const std::string& text) {
    if (text.empty()) return 0;
    return std::max<int64_t>(1, static_cast<int64_t>((text.size() + 3) / 4));
}

std::string buildLocalDiagnosticSynthesis(const ModelSpec& model, const std::string& endpoint, const std::vector<ChatMsg>& messages, const std::string& transportErr) {
    std::string lastUser;
    int contextChunks = 0;
    for (const auto& m : messages) {
        if (m.role == "user") lastUser = m.content;
        if (m.content.find("<vault_context>") != std::string::npos) {
            ++contextChunks;
        }
    }
    if (lastUser.size() > 360) {
        lastUser = lastUser.substr(0, 360) + "...";
    }

    std::ostringstream oss;
    oss << "[Local Fallback Synthesis - " << model.id << "]\n";
    oss << "Daemon local no disponible en " << endpoint << " (" << transportErr << ").\n";
    oss << "Diagnostico determinista de respaldo:\n";
    oss << "- Mensajes en contexto: " << messages.size() << " (bloques de vault: " << contextChunks << ")\n";
    oss << "- Solicitud evaluada: " << (lastUser.empty() ? "(vacia)" : lastUser) << "\n";
    oss << "- Recomendacion: inicia un servidor OpenAI-compatible local en " << endpoint
        << " (llama.cpp / ollama / vllm) o configura una API key cloud en Proveedores para inferencia completa.\n";
    return oss.str();
}

}

std::string AiProviderClient::jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

std::string AiProviderClient::extractJsonStringField(const std::string& json, const std::string& key) {
    return extractJsonStringFromOffset(json, key, 0);
}

int64_t AiProviderClient::extractJsonIntField(const std::string& json, const std::string& key) {
    return extractJsonIntFromOffset(json, key, 0);
}

CompletionResult AiProviderClient::httpPostJson(
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& jsonBody,
    int timeoutMs) {
    CompletionResult res;
    auto t0 = std::chrono::steady_clock::now();

    ParsedUrl parsed = parseUrl(url);
    if (!parsed.valid) {
        res.error = "Invalid URL: " + url;
        return res;
    }

#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(
        L"NodePanda/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        res.error = "WinHttpOpen failed (code " + std::to_string(GetLastError()) + ")";
        return res;
    }

    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    std::wstring wHost = toWide(parsed.host);
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), static_cast<INTERNET_PORT>(parsed.port), 0);
    if (!hConnect) {
        res.error = "WinHttpConnect failed (code " + std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hSession);
        return res;
    }

    std::wstring wPath = toWide(parsed.path);
    DWORD flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        wPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);
    if (!hRequest) {
        res.error = "WinHttpOpenRequest failed (code " + std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return res;
    }

    std::ostringstream hdrStream;
    bool hasContentType = false;
    for (const auto& kv : headers) {
        std::string lowerKey = kv.first;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (lowerKey == "content-type") hasContentType = true;
        hdrStream << kv.first << ": " << kv.second << "\r\n";
    }
    if (!hasContentType) {
        hdrStream << "Content-Type: application/json\r\n";
    }

    std::wstring wHeaders = toWide(hdrStream.str());
    BOOL sent = WinHttpSendRequest(
        hRequest,
        wHeaders.c_str(),
        static_cast<DWORD>(wHeaders.size()),
        const_cast<char*>(jsonBody.data()),
        static_cast<DWORD>(jsonBody.size()),
        static_cast<DWORD>(jsonBody.size()),
        0);

    if (!sent) {
        res.error = "WinHttpSendRequest failed (code " + std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return res;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        res.error = "WinHttpReceiveResponse failed (code " + std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return res;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX);

    std::string responseBody;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD downloaded = 0;
        if (WinHttpReadData(hRequest, chunk.data(), available, &downloaded)) {
            responseBody.append(chunk.data(), downloaded);
        } else {
            break;
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    auto t1 = std::chrono::steady_clock::now();
    res.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (statusCode < 200 || statusCode >= 300) {
        std::string apiErr = extractJsonStringField(responseBody, "message");
        res.error = "HTTP " + std::to_string(statusCode) + (apiErr.empty() ? "" : (": " + apiErr));
        res.text = responseBody;
        res.ok = false;
        return res;
    }

    res.ok = true;
    res.text = responseBody;
    return res;
#else
    if (parsed.secure) {
        res.error = "HTTPS requires WinHTTP on Windows or TLS proxy on POSIX";
        return res;
    }

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(parsed.port);
    if (getaddrinfo(parsed.host.c_str(), portStr.c_str(), &hints, &result) != 0 || !result) {
        res.error = "DNS resolution failed for " + parsed.host;
        return res;
    }

    int sock = -1;
    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;
        struct timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(result);

    if (sock < 0) {
        res.error = "TCP connect failed to " + parsed.host + ":" + portStr;
        return res;
    }

    std::ostringstream req;
    req << "POST " << parsed.path << " HTTP/1.1\r\n";
    req << "Host: " << parsed.host << ":" << parsed.port << "\r\n";
    req << "Connection: close\r\n";
    req << "Content-Length: " << jsonBody.size() << "\r\n";
    bool hasCt = false;
    for (const auto& kv : headers) {
        std::string lk = kv.first;
        std::transform(lk.begin(), lk.end(), lk.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (lk == "content-type") hasCt = true;
        req << kv.first << ": " << kv.second << "\r\n";
    }
    if (!hasCt) {
        req << "Content-Type: application/json\r\n";
    }
    req << "\r\n" << jsonBody;

    std::string reqStr = req.str();
    size_t sentTotal = 0;
    while (sentTotal < reqStr.size()) {
        ssize_t n = send(sock, reqStr.data() + sentTotal, reqStr.size() - sentTotal, 0);
        if (n <= 0) {
            close(sock);
            res.error = "TCP send failed";
            return res;
        }
        sentTotal += static_cast<size_t>(n);
    }

    std::string rawResp;
    char buf[4096];
    ssize_t n = 0;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        rawResp.append(buf, static_cast<size_t>(n));
    }
    close(sock);

    auto t1 = std::chrono::steady_clock::now();
    res.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    size_t hdrEnd = rawResp.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) {
        res.error = "Malformed HTTP response";
        return res;
    }
    std::string statusLine = rawResp.substr(0, rawResp.find("\r\n"));
    std::string body = rawResp.substr(hdrEnd + 4);
    int statusCode = 0;
    {
        std::istringstream iss(statusLine);
        std::string httpVer;
        iss >> httpVer >> statusCode;
    }
    if (statusCode < 200 || statusCode >= 300) {
        std::string apiErr = extractJsonStringField(body, "message");
        res.error = "HTTP " + std::to_string(statusCode) + (apiErr.empty() ? "" : (": " + apiErr));
        res.text = body;
        return res;
    }
    res.ok = true;
    res.text = body;
    return res;
#endif
}

CompletionResult AiProviderClient::complete(
    const AiCatalog& catalog,
    const ModelSpec& model,
    const std::vector<ChatMsg>& messages,
    int maxOutputTokens) {
    CompletionResult out;
    out.actualModel = model.id;

    const ProviderSpec* provider = catalog.findProvider(model.provider);
    if (!provider) {
        out.error = "Unknown provider for model: " + model.id;
        return out;
    }
    if (!provider->enabled) {
        out.error = "Provider disabled: " + provider->name;
        return out;
    }

    std::string endpoint = provider->endpoint;
    if (provider->id == ProviderId::Local && !catalog.settings().localEndpoint.empty()) {
        endpoint = catalog.settings().localEndpoint;
    }

    std::string apiKey = catalog.getApiKey(provider->id);
    if (provider->id != ProviderId::Local && apiKey.empty()) {
        out.error = "Missing API key for " + provider->name +
                    (provider->envVar.empty() ? "" : (" (env " + provider->envVar + ")"));
        return out;
    }

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    std::ostringstream payload;

    if (provider->id == ProviderId::Anthropic) {
        headers["x-api-key"] = apiKey;
        headers["anthropic-version"] = "2023-06-01";

        std::string systemText;
        std::vector<ChatMsg> filtered;
        for (const auto& m : messages) {
            if (m.role == "system") {
                if (!systemText.empty()) systemText += "\n\n";
                systemText += m.content;
            } else {
                filtered.push_back(m);
            }
        }
        if (filtered.empty()) {
            filtered.push_back({ "user", "Continue." });
        }

        payload << "{";
        payload << "\"model\":\"" << jsonEscape(model.id) << "\",";
        payload << "\"max_tokens\":" << std::max(1, maxOutputTokens) << ",";
        if (!systemText.empty()) {
            payload << "\"system\":\"" << jsonEscape(systemText) << "\",";
        }
        payload << "\"messages\":[";
        for (size_t i = 0; i < filtered.size(); ++i) {
            if (i > 0) payload << ",";
            std::string role = (filtered[i].role == "assistant") ? "assistant" : "user";
            payload << "{\"role\":\"" << role << "\",\"content\":\"" << jsonEscape(filtered[i].content) << "\"}";
        }
        payload << "]}";
    } else {
        if (!apiKey.empty()) {
            headers["Authorization"] = "Bearer " + apiKey;
        }
        payload << "{";
        payload << "\"model\":\"" << jsonEscape(model.id) << "\",";
        payload << "\"max_tokens\":" << std::max(1, maxOutputTokens) << ",";
        payload << "\"temperature\":0.2,";
        payload << "\"messages\":[";
        for (size_t i = 0; i < messages.size(); ++i) {
            if (i > 0) payload << ",";
            payload << "{\"role\":\"" << jsonEscape(messages[i].role)
                    << "\",\"content\":\"" << jsonEscape(messages[i].content) << "\"}";
        }
        payload << "]}";
    }

    CompletionResult httpRes = httpPostJson(endpoint, headers, payload.str(), 45000);
    out.latencyMs = httpRes.latencyMs;

    if (!httpRes.ok) {
        if (provider->id == ProviderId::Local) {
            out.ok = true;
            out.usedLocalFallback = true;
            out.actualModel = model.id + " (local-fallback)";
            out.text = buildLocalDiagnosticSynthesis(model, endpoint, messages, httpRes.error);
            int64_t promptTok = 0;
            for (const auto& m : messages) promptTok += estimateTokens(m.content);
            out.usage.input = promptTok;
            out.usage.output = estimateTokens(out.text);
            return out;
        }
        out.ok = false;
        out.error = httpRes.error;
        return out;
    }

    const std::string& raw = httpRes.text;
    std::string respModel = extractJsonStringField(raw, "model");
    if (!respModel.empty()) {
        out.actualModel = respModel;
    }

    if (provider->id == ProviderId::Anthropic) {
        size_t contentPos = raw.find("\"content\"");
        if (contentPos != std::string::npos) {
            out.text = extractJsonStringFromOffset(raw, "text", contentPos);
        }
        if (out.text.empty()) {
            out.text = extractJsonStringField(raw, "text");
        }
        out.usage.input = extractJsonIntField(raw, "input_tokens");
        out.usage.output = extractJsonIntField(raw, "output_tokens");
        out.usage.cached = extractJsonIntField(raw, "cache_read_input_tokens");
        out.usage.cacheWrite = extractJsonIntField(raw, "cache_creation_input_tokens");
    } else {
        size_t choicesPos = raw.find("\"choices\"");
        size_t msgPos = (choicesPos != std::string::npos) ? raw.find("\"message\"", choicesPos) : std::string::npos;
        if (msgPos != std::string::npos) {
            out.text = extractJsonStringFromOffset(raw, "content", msgPos);
            if (out.text.empty()) {
                out.text = extractJsonStringFromOffset(raw, "reasoning_content", msgPos);
            }
        }
        if (out.text.empty()) {
            out.text = extractJsonStringField(raw, "content");
        }
        size_t usagePos = raw.find("\"usage\"");
        size_t uOff = (usagePos != std::string::npos) ? usagePos : 0;
        out.usage.input = extractJsonIntFromOffset(raw, "prompt_tokens", uOff);
        out.usage.output = extractJsonIntFromOffset(raw, "completion_tokens", uOff);
        out.usage.cached = extractJsonIntFromOffset(raw, "cached_tokens", uOff);
        out.usage.reasoning = extractJsonIntFromOffset(raw, "reasoning_tokens", uOff);
    }

    if (out.usage.input == 0) {
        int64_t est = 0;
        for (const auto& m : messages) est += estimateTokens(m.content);
        out.usage.input = est;
    }
    if (out.usage.output == 0 && !out.text.empty()) {
        out.usage.output = estimateTokens(out.text);
    }

    if (out.text.empty()) {
        out.ok = false;
        out.error = "Empty completion content from " + provider->name;
        return out;
    }

    out.ok = true;
    return out;
}

}
