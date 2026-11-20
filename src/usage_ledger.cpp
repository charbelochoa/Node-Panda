#include "usage_ledger.h"
#include "crypto_engine.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace nodepanda {

namespace {

std::string nowIsoUtc() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm buf{};
#ifdef _WIN32
    gmtime_s(&buf, &t);
#else
    gmtime_r(&t, &buf);
#endif
    char out[32];
    std::snprintf(out, sizeof(out), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  buf.tm_year + 1900, buf.tm_mon + 1, buf.tm_mday,
                  buf.tm_hour, buf.tm_min, buf.tm_sec);
    return std::string(out);
}

std::string csvEscape(const std::string& field) {
    bool needsQuotes = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return field;
    std::string out = "\"";
    for (char c : field) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    out += "\"";
    return out;
}

std::string sanitizePipe(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c == '|' || c == '\n' || c == '\r') c = ' ';
    }
    return out;
}

std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : line) {
        if (c == '|') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    return parts;
}

}

void UsageLedger::init(const std::filesystem::path& dataDir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dataDir = dataDir;
    std::error_code ec;
    if (!m_dataDir.empty() && !std::filesystem::exists(m_dataDir, ec)) {
        std::filesystem::create_directories(m_dataDir, ec);
    }
    m_entries.clear();
    std::ifstream in(m_dataDir / "usage_ledger.tsv");
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto cols = splitPipe(line);
        if (cols.size() < 15) continue;
        UsageEntry e;
        e.id = cols[0];
        e.runId = cols[1];
        e.owner = cols[2];
        e.provider = cols[3];
        e.requestedModel = cols[4];
        e.actualModel = cols[5];
        try {
            e.usage.input = std::stoll(cols[6]);
            e.usage.output = std::stoll(cols[7]);
            e.usage.cached = std::stoll(cols[8]);
            e.usage.cacheWrite = std::stoll(cols[9]);
            e.usage.reasoning = std::stoll(cols[10]);
            e.costNano = std::stoll(cols[11]);
            e.latencyMs = std::stoll(cols[12]);
        } catch (...) {
            continue;
        }
        e.freeTier = (cols[13] == "1");
        e.createdAt = cols[14];
        m_entries.push_back(std::move(e));
    }
}

void UsageLedger::record(const UsageEntry& entry) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        UsageEntry copy = entry;
        if (copy.id.empty()) copy.id = CryptoEngine::generateUuid();
        if (copy.createdAt.empty()) copy.createdAt = nowIsoUtc();
        m_entries.push_back(std::move(copy));
    }
    save();
}

const std::vector<UsageEntry>& UsageLedger::entries() const {
    return m_entries;
}

int64_t UsageLedger::totalCostNano() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int64_t total = 0;
    for (const auto& e : m_entries) {
        total += e.costNano;
    }
    return total;
}

int64_t UsageLedger::totalTokens() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int64_t total = 0;
    for (const auto& e : m_entries) {
        total += e.usage.input + e.usage.output + e.usage.reasoning;
    }
    return total;
}

bool UsageLedger::exportCsv(const std::filesystem::path& outPath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::error_code ec;
    if (outPath.has_parent_path() && !std::filesystem::exists(outPath.parent_path(), ec)) {
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }
    std::ofstream out(outPath, std::ios::trunc);
    if (!out.is_open()) return false;

    out << "id,run_id,owner,provider,requested_model,actual_model,input_tokens,output_tokens,cached_tokens,cache_write_tokens,reasoning_tokens,cost_usd,latency_ms,free_tier,created_at\n";
    for (const auto& e : m_entries) {
        double usd = static_cast<double>(e.costNano) / 1000000000.0;
        std::ostringstream usdStream;
        usdStream << std::fixed << std::setprecision(6) << usd;
        out << csvEscape(e.id) << ","
            << csvEscape(e.runId) << ","
            << csvEscape(e.owner) << ","
            << csvEscape(e.provider) << ","
            << csvEscape(e.requestedModel) << ","
            << csvEscape(e.actualModel) << ","
            << e.usage.input << ","
            << e.usage.output << ","
            << e.usage.cached << ","
            << e.usage.cacheWrite << ","
            << e.usage.reasoning << ","
            << usdStream.str() << ","
            << e.latencyMs << ","
            << (e.freeTier ? "true" : "false") << ","
            << csvEscape(e.createdAt) << "\n";
    }
    return true;
}

void UsageLedger::save() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_dataDir.empty()) return;
    std::ofstream out(m_dataDir / "usage_ledger.tsv", std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& e : m_entries) {
        out << sanitizePipe(e.id) << "|"
            << sanitizePipe(e.runId) << "|"
            << sanitizePipe(e.owner) << "|"
            << sanitizePipe(e.provider) << "|"
            << sanitizePipe(e.requestedModel) << "|"
            << sanitizePipe(e.actualModel) << "|"
            << e.usage.input << "|"
            << e.usage.output << "|"
            << e.usage.cached << "|"
            << e.usage.cacheWrite << "|"
            << e.usage.reasoning << "|"
            << e.costNano << "|"
            << e.latencyMs << "|"
            << (e.freeTier ? "1" : "0") << "|"
            << sanitizePipe(e.createdAt) << "\n";
    }
}

void UsageLedger::load() {
    if (!m_dataDir.empty()) {
        init(m_dataDir);
    }
}

}
