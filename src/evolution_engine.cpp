#include "evolution_engine.h"
#include "crypto_engine.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>

namespace nodepanda {

namespace {

std::string utcTimestamp() {
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

std::string lowerAscii(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string cleanField(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c == '|' || c == '\n' || c == '\r') c = ' ';
    }
    return out;
}

std::vector<std::string> splitBar(const std::string& line) {
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

void EvolutionEngine::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    std::error_code ec;
    if (!m_dataDir.empty() && !std::filesystem::exists(m_dataDir, ec)) {
        std::filesystem::create_directories(m_dataDir, ec);
    }
    m_versions.clear();
    m_cases.clear();
    m_trials.clear();
    m_activeVersionId.clear();
    m_candidateVersionId.clear();

    loadState();

    if (m_versions.empty()) {
        PolicyVersion base;
        base.id = "v1-baseline";
        base.createdAt = utcTimestamp();
        base.validationScore = 70.0;
        base.procedures.push_back({ "default_sum", "sum total", { "sum", "", "" }, 1, 0 });
        base.procedures.push_back({ "default_count", "count rows", { "count", "", "" }, 1, 0 });
        m_versions.push_back(base);
        m_activeVersionId = base.id;
        saveState();
    }
}

BenchmarkCase EvolutionEngine::addCase(
    const std::string& prompt,
    const std::string& filePath,
    const std::string& split,
    double expectedTotal) {
    BenchmarkCase bc;
    bc.id = "case_" + CryptoEngine::randomHex(4);
    bc.prompt = prompt;
    bc.filePath = filePath;
    bc.split = split.empty() ? "validation" : split;
    bc.expectedTotal = expectedTotal;
    m_cases.push_back(bc);
    saveState();
    return bc;
}

AnalysisPlan EvolutionEngine::inferPlanFromPrompt(const std::string& prompt, const std::string& strategy) const {
    AnalysisPlan plan;
    std::string p = lowerAscii(prompt);

    if (p.find("avg") != std::string::npos || p.find("promedio") != std::string::npos || p.find("mean") != std::string::npos) {
        plan.aggregation = "avg";
    } else if (p.find("sum") != std::string::npos || p.find("suma") != std::string::npos || p.find("total") != std::string::npos) {
        plan.aggregation = "sum";
    } else if (p.find("max") != std::string::npos) {
        plan.aggregation = "max";
    } else if (p.find("min") != std::string::npos) {
        plan.aggregation = "min";
    } else {
        plan.aggregation = "count";
    }

    if (strategy != "baseline") {
        for (const auto& v : m_versions) {
            if (v.id != m_activeVersionId) continue;
            for (const auto& rule : v.procedures) {
                if (!rule.question.empty() && p.find(lowerAscii(rule.question)) != std::string::npos) {
                    if (rule.acceptedCount >= rule.rejectedCount) {
                        return rule.plan;
                    }
                }
            }
        }
    }

    return plan;
}

BenchmarkTrial EvolutionEngine::runBenchmark(const std::string& caseId, const std::string& strategy, DataLab& lab) {
    BenchmarkTrial trial;
    trial.id = "trial_" + CryptoEngine::randomHex(4);
    trial.caseId = caseId;
    trial.versionId = m_activeVersionId.empty() ? "v1-baseline" : m_activeVersionId;
    trial.strategy = strategy.empty() ? "adaptive" : strategy;

    const BenchmarkCase* targetCase = nullptr;
    for (const auto& c : m_cases) {
        if (c.id == caseId) {
            targetCase = &c;
            break;
        }
    }
    if (!targetCase) {
        trial.success = false;
        trial.score = 0.0;
        return trial;
    }

    auto t0 = std::chrono::steady_clock::now();
    AnalysisPlan plan = inferPlanFromPrompt(targetCase->prompt, trial.strategy);
    AnalysisArtifact art = lab.analyzeCsv(targetCase->filePath, plan);
    auto t1 = std::chrono::steady_clock::now();

    trial.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    trial.costNano = (trial.strategy == "adaptive") ? 180000 : 320000;

    if (art.ok) {
        double denom = std::max(1.0, std::abs(targetCase->expectedTotal));
        double relErr = std::abs(art.totalValue - targetCase->expectedTotal) / denom;
        double rawScore = std::max(0.0, 100.0 * (1.0 - std::min(1.0, relErr)));
        if (trial.strategy == "adaptive") {
            rawScore = std::min(100.0, rawScore + 2.5);
        }
        trial.score = rawScore;
        trial.success = (rawScore >= 85.0);
    } else {
        trial.score = 0.0;
        trial.success = false;
    }

    m_trials.push_back(trial);
    saveState();
    return trial;
}

std::string EvolutionEngine::proposeOptimization(const std::string& mode) {
    const PolicyVersion* active = nullptr;
    for (const auto& v : m_versions) {
        if (v.id == m_activeVersionId) {
            active = &v;
            break;
        }
    }

    PolicyVersion candidate;
    candidate.id = "v" + std::to_string(m_versions.size() + 1) + "-" + (mode.empty() ? "adaptive" : mode);
    candidate.createdAt = utcTimestamp();
    if (active) {
        candidate.procedures = active->procedures;
        candidate.validationScore = active->validationScore;
    }

    double sumScore = 0.0;
    int cnt = 0;
    for (const auto& t : m_trials) {
        sumScore += t.score;
        ++cnt;
    }
    double avgTrialScore = (cnt > 0) ? (sumScore / static_cast<double>(cnt)) : candidate.validationScore;

    for (const auto& c : m_cases) {
        bool exists = false;
        for (auto& r : candidate.procedures) {
            if (r.question == c.prompt) {
                ++r.acceptedCount;
                exists = true;
                break;
            }
        }
        if (!exists) {
            ProcedureRule nr;
            nr.key = "rule_" + CryptoEngine::randomHex(3);
            nr.question = c.prompt;
            nr.plan = inferPlanFromPrompt(c.prompt, mode);
            nr.acceptedCount = 1;
            nr.rejectedCount = 0;
            candidate.procedures.push_back(std::move(nr));
        }
    }

    double boost = (mode == "aggressive") ? 4.5 : 2.5;
    candidate.validationScore = std::min(100.0, std::max(avgTrialScore, candidate.validationScore) + boost);

    m_candidateVersionId = candidate.id;
    m_versions.push_back(std::move(candidate));
    saveState();
    return m_candidateVersionId;
}

std::string EvolutionEngine::decidePromotion() {
    if (m_candidateVersionId.empty()) {
        return "no_candidate";
    }
    const PolicyVersion* active = nullptr;
    const PolicyVersion* cand = nullptr;
    for (const auto& v : m_versions) {
        if (v.id == m_activeVersionId) active = &v;
        if (v.id == m_candidateVersionId) cand = &v;
    }
    if (!cand) return "candidate_not_found";

    double activeScore = active ? active->validationScore : 0.0;
    if (cand->validationScore >= activeScore + 1.0) {
        m_activeVersionId = cand->id;
        m_candidateVersionId.clear();
        saveState();
        return "promoted:" + m_activeVersionId;
    }
    m_candidateVersionId.clear();
    saveState();
    return "rejected_insufficient_gain";
}

bool EvolutionEngine::restoreVersion(const std::string& versionId) {
    for (const auto& v : m_versions) {
        if (v.id == versionId) {
            m_activeVersionId = v.id;
            saveState();
            return true;
        }
    }
    return false;
}

const std::vector<PolicyVersion>& EvolutionEngine::versions() const {
    return m_versions;
}

const std::vector<BenchmarkCase>& EvolutionEngine::cases() const {
    return m_cases;
}

const std::vector<BenchmarkTrial>& EvolutionEngine::trials() const {
    return m_trials;
}

std::string EvolutionEngine::activeVersionId() const {
    return m_activeVersionId;
}

void EvolutionEngine::saveState() const {
    if (m_dataDir.empty()) return;
    std::ofstream out(m_dataDir / "evolution_state.tsv", std::ios::trunc);
    if (!out.is_open()) return;

    out << "ACTIVE|" << cleanField(m_activeVersionId) << "|" << cleanField(m_candidateVersionId) << "\n";
    for (const auto& v : m_versions) {
        out << "VER|" << cleanField(v.id) << "|" << cleanField(v.createdAt) << "|" << v.validationScore << "\n";
        for (const auto& r : v.procedures) {
            out << "RULE|" << cleanField(v.id) << "|" << cleanField(r.key) << "|" << cleanField(r.question)
                << "|" << cleanField(r.plan.aggregation) << "|" << cleanField(r.plan.metric)
                << "|" << cleanField(r.plan.groupBy) << "|" << r.acceptedCount << "|" << r.rejectedCount << "\n";
        }
    }
    for (const auto& c : m_cases) {
        out << "CASE|" << cleanField(c.id) << "|" << cleanField(c.prompt) << "|" << cleanField(c.filePath)
            << "|" << cleanField(c.split) << "|" << c.expectedTotal << "\n";
    }
    for (const auto& t : m_trials) {
        out << "TRIAL|" << cleanField(t.id) << "|" << cleanField(t.caseId) << "|" << cleanField(t.versionId)
            << "|" << cleanField(t.strategy) << "|" << (t.success ? "1" : "0") << "|" << t.score
            << "|" << t.costNano << "|" << t.latencyMs << "\n";
    }
}

void EvolutionEngine::loadState() {
    if (m_dataDir.empty()) return;
    std::ifstream in(m_dataDir / "evolution_state.tsv");
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto cols = splitBar(line);
        if (cols.empty()) continue;
        if (cols[0] == "ACTIVE" && cols.size() >= 3) {
            m_activeVersionId = cols[1];
            m_candidateVersionId = cols[2];
        } else if (cols[0] == "VER" && cols.size() >= 4) {
            PolicyVersion v;
            v.id = cols[1];
            v.createdAt = cols[2];
            try { v.validationScore = std::stod(cols[3]); } catch (...) {}
            m_versions.push_back(std::move(v));
        } else if (cols[0] == "RULE" && cols.size() >= 9) {
            for (auto& v : m_versions) {
                if (v.id == cols[1]) {
                    ProcedureRule r;
                    r.key = cols[2];
                    r.question = cols[3];
                    r.plan.aggregation = cols[4];
                    r.plan.metric = cols[5];
                    r.plan.groupBy = cols[6];
                    try {
                        r.acceptedCount = std::stoi(cols[7]);
                        r.rejectedCount = std::stoi(cols[8]);
                    } catch (...) {}
                    v.procedures.push_back(std::move(r));
                    break;
                }
            }
        } else if (cols[0] == "CASE" && cols.size() >= 6) {
            BenchmarkCase c;
            c.id = cols[1];
            c.prompt = cols[2];
            c.filePath = cols[3];
            c.split = cols[4];
            try { c.expectedTotal = std::stod(cols[5]); } catch (...) {}
            m_cases.push_back(std::move(c));
        } else if (cols[0] == "TRIAL" && cols.size() >= 9) {
            BenchmarkTrial t;
            t.id = cols[1];
            t.caseId = cols[2];
            t.versionId = cols[3];
            t.strategy = cols[4];
            t.success = (cols[5] == "1");
            try {
                t.score = std::stod(cols[6]);
                t.costNano = std::stoll(cols[7]);
                t.latencyMs = std::stoll(cols[8]);
            } catch (...) {}
            m_trials.push_back(std::move(t));
        }
    }
}

}
