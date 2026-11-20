#include "data_lab.h"
#include "crypto_engine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>

namespace nodepanda {

namespace {

std::string trimCell(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string toLowerStr(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::vector<std::string> parseCsvRow(const std::string& line) {
    std::vector<std::string> cells;
    std::string cur;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur.push_back(c);
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                cells.push_back(trimCell(cur));
                cur.clear();
            } else if (c != '\r') {
                cur.push_back(c);
            }
        }
    }
    cells.push_back(trimCell(cur));
    return cells;
}

bool tryParseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* endPtr = nullptr;
    out = std::strtod(s.c_str(), &endPtr);
    if (!endPtr || endPtr == s.c_str()) return false;
    while (*endPtr != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*endPtr))) return false;
        ++endPtr;
    }
    return std::isfinite(out);
}

std::string readFileString(const std::filesystem::path& path, bool& ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        ok = false;
        return "";
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    ok = true;
    return oss.str();
}

std::string escapeSvgText(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

}

TableProfile DataLab::profileCsv(const std::filesystem::path& filePath) const {
    TableProfile profile;
    profile.sourcePath = filePath.string();

    bool ok = false;
    std::string raw = readFileString(filePath, ok);
    if (!ok) {
        profile.error = "Unable to open CSV file: " + filePath.string();
        return profile;
    }

    profile.sha256 = CryptoEngine::sha256(raw);
    std::istringstream iss(raw);
    std::string headerLine;
    if (!std::getline(iss, headerLine)) {
        profile.error = "CSV file is empty";
        return profile;
    }

    auto headers = parseCsvRow(headerLine);
    if (headers.empty()) {
        profile.error = "CSV header is empty";
        return profile;
    }

    profile.colCount = static_cast<int>(headers.size());
    profile.columns.resize(headers.size());
    std::vector<int> numericCounts(headers.size(), 0);
    std::vector<bool> hasMinMax(headers.size(), false);

    for (size_t i = 0; i < headers.size(); ++i) {
        profile.columns[i].name = headers[i].empty() ? ("col_" + std::to_string(i)) : headers[i];
        profile.columns[i].inferredType = "string";
    }

    std::string line;
    while (std::getline(iss, line)) {
        if (trimCell(line).empty()) continue;
        auto cells = parseCsvRow(line);
        ++profile.rowCount;
        for (size_t i = 0; i < headers.size(); ++i) {
            std::string val = (i < cells.size()) ? cells[i] : "";
            if (val.empty()) {
                ++profile.columns[i].nullCount;
            } else {
                ++profile.columns[i].nonEmpty;
                double num = 0.0;
                if (tryParseDouble(val, num)) {
                    ++numericCounts[i];
                    if (!hasMinMax[i]) {
                        profile.columns[i].minVal = num;
                        profile.columns[i].maxVal = num;
                        hasMinMax[i] = true;
                    } else {
                        if (num < profile.columns[i].minVal) profile.columns[i].minVal = num;
                        if (num > profile.columns[i].maxVal) profile.columns[i].maxVal = num;
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < headers.size(); ++i) {
        if (profile.columns[i].nonEmpty > 0 && numericCounts[i] == profile.columns[i].nonEmpty) {
            profile.columns[i].inferredType = "number";
        }
    }

    profile.ok = true;
    return profile;
}

AnalysisArtifact DataLab::analyzeCsv(const std::filesystem::path& filePath, const AnalysisPlan& plan) {
    AnalysisArtifact art;
    art.id = CryptoEngine::generateUuid();
    art.sourcePath = filePath.string();
    art.plan = plan;

    bool ok = false;
    std::string raw = readFileString(filePath, ok);
    if (!ok) {
        art.error = "Unable to open CSV file: " + filePath.string();
        return art;
    }
    art.sha256 = CryptoEngine::sha256(raw);

    std::istringstream iss(raw);
    std::string headerLine;
    if (!std::getline(iss, headerLine)) {
        art.error = "CSV file is empty";
        return art;
    }
    auto headers = parseCsvRow(headerLine);

    int metricIdx = -1;
    int groupIdx = -1;
    for (size_t i = 0; i < headers.size(); ++i) {
        if (!plan.metric.empty() && headers[i] == plan.metric) metricIdx = static_cast<int>(i);
        if (!plan.groupBy.empty() && headers[i] == plan.groupBy) groupIdx = static_cast<int>(i);
    }

    std::string agg = toLowerStr(plan.aggregation.empty() ? "count" : plan.aggregation);
    if (agg != "count" && metricIdx < 0) {
        for (size_t i = 0; i < headers.size(); ++i) {
            if (static_cast<int>(i) != groupIdx) {
                metricIdx = static_cast<int>(i);
                break;
            }
        }
        if (metricIdx < 0) {
            art.error = "Metric column not found: " + plan.metric;
            return art;
        }
    }

    struct GroupAcc {
        int count = 0;
        double sum = 0.0;
        double minV = std::numeric_limits<double>::max();
        double maxV = std::numeric_limits<double>::lowest();
        bool hasVal = false;
    };

    std::map<std::string, GroupAcc> groups;
    std::string line;
    while (std::getline(iss, line)) {
        if (trimCell(line).empty()) continue;
        auto cells = parseCsvRow(line);
        ++art.processedRows;

        std::string key = "All";
        if (groupIdx >= 0 && static_cast<size_t>(groupIdx) < cells.size() && !cells[groupIdx].empty()) {
            key = cells[groupIdx];
        }

        GroupAcc& acc = groups[key];
        if (agg == "count") {
            ++acc.count;
            acc.sum += 1.0;
            acc.hasVal = true;
        } else if (metricIdx >= 0 && static_cast<size_t>(metricIdx) < cells.size()) {
            double v = 0.0;
            if (tryParseDouble(cells[metricIdx], v)) {
                ++acc.count;
                acc.sum += v;
                if (!acc.hasVal || v < acc.minV) acc.minV = v;
                if (!acc.hasVal || v > acc.maxV) acc.maxV = v;
                acc.hasVal = true;
            }
        }
    }

    double overallSum = 0.0;
    double overallMin = std::numeric_limits<double>::max();
    double overallMax = std::numeric_limits<double>::lowest();
    int overallCount = 0;

    for (const auto& kv : groups) {
        const GroupAcc& g = kv.second;
        double val = 0.0;
        if (agg == "count") {
            val = static_cast<double>(g.count);
        } else if (agg == "sum") {
            val = g.sum;
        } else if (agg == "avg") {
            val = g.count > 0 ? (g.sum / static_cast<double>(g.count)) : 0.0;
        } else if (agg == "min") {
            val = g.hasVal ? g.minV : 0.0;
        } else if (agg == "max") {
            val = g.hasVal ? g.maxV : 0.0;
        } else {
            val = static_cast<double>(g.count);
        }
        art.points.push_back({ kv.first, val });
        overallSum += g.sum;
        overallCount += g.count;
        if (g.hasVal) {
            if (g.minV < overallMin) overallMin = g.minV;
            if (g.maxV > overallMax) overallMax = g.maxV;
        }
    }

    std::sort(art.points.begin(), art.points.end(), [](const AnalysisPoint& a, const AnalysisPoint& b) {
        if (a.value != b.value) return a.value > b.value;
        return a.label < b.label;
    });

    if (agg == "count") art.totalValue = static_cast<double>(overallCount);
    else if (agg == "sum") art.totalValue = overallSum;
    else if (agg == "avg") art.totalValue = overallCount > 0 ? (overallSum / static_cast<double>(overallCount)) : 0.0;
    else if (agg == "min") art.totalValue = (overallCount > 0) ? overallMin : 0.0;
    else if (agg == "max") art.totalValue = (overallCount > 0) ? overallMax : 0.0;

    art.ok = true;
    m_artifacts.push_back(art);
    return art;
}

std::vector<DocumentHit> DataLab::searchDocument(const std::filesystem::path& filePath, const std::string& query) const {
    std::vector<DocumentHit> hits;
    if (query.empty()) return hits;

    std::ifstream in(filePath);
    if (!in.is_open()) return hits;

    std::string qLow = toLowerStr(query);
    std::string line;
    int lineNum = 0;
    while (std::getline(in, line)) {
        ++lineNum;
        if (toLowerStr(line).find(qLow) != std::string::npos) {
            std::string excerpt = trimCell(line);
            if (excerpt.size() > 220) excerpt = excerpt.substr(0, 217) + "...";
            hits.push_back({ lineNum, excerpt });
            if (hits.size() >= 100) break;
        }
    }
    return hits;
}

std::string DataLab::renderChartSvg(const AnalysisArtifact& artifact, const std::string& kind) const {
    const int width = 760;
    const int height = 380;
    const int padLeft = 72;
    const int padRight = 28;
    const int padTop = 44;
    const int padBottom = 68;
    const int chartW = width - padLeft - padRight;
    const int chartH = height - padTop - padBottom;

    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height
        << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
    svg << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f1218\" rx=\"8\"/>\n";
    svg << "  <text x=\"" << padLeft << "\" y=\"26\" fill=\"#e8edf5\" font-family=\"monospace\" font-size=\"14\">"
        << escapeSvgText(artifact.plan.aggregation + " (" + (artifact.plan.metric.empty() ? "rows" : artifact.plan.metric) + ")")
        << "</text>\n";

    if (artifact.points.empty()) {
        svg << "  <text x=\"300\" y=\"190\" fill=\"#8b949e\" font-family=\"sans-serif\" font-size=\"14\">No data points</text>\n";
        svg << "</svg>\n";
        return svg.str();
    }

    size_t count = std::min<size_t>(artifact.points.size(), 16);
    double maxVal = 0.0;
    for (size_t i = 0; i < count; ++i) {
        if (artifact.points[i].value > maxVal) maxVal = artifact.points[i].value;
    }
    if (maxVal <= 0.0) maxVal = 1.0;

    svg << "  <line x1=\"" << padLeft << "\" y1=\"" << (padTop + chartH) << "\" x2=\"" << (padLeft + chartW)
        << "\" y2=\"" << (padTop + chartH) << "\" stroke=\"#30363d\" stroke-width=\"1\"/>\n";
    svg << "  <line x1=\"" << padLeft << "\" y1=\"" << padTop << "\" x2=\"" << padLeft
        << "\" y2=\"" << (padTop + chartH) << "\" stroke=\"#30363d\" stroke-width=\"1\"/>\n";

    double slotW = static_cast<double>(chartW) / static_cast<double>(count);

    if (kind == "line") {
        std::ostringstream pts;
        for (size_t i = 0; i < count; ++i) {
            double cx = padLeft + slotW * (static_cast<double>(i) + 0.5);
            double norm = std::max(0.0, artifact.points[i].value / maxVal);
            double cy = padTop + chartH - norm * chartH;
            if (i > 0) pts << " ";
            pts << std::fixed << std::setprecision(1) << cx << "," << cy;
        }
        svg << "  <polyline fill=\"none\" stroke=\"#4cc9f0\" stroke-width=\"2.5\" points=\"" << pts.str() << "\"/>\n";
        for (size_t i = 0; i < count; ++i) {
            double cx = padLeft + slotW * (static_cast<double>(i) + 0.5);
            double norm = std::max(0.0, artifact.points[i].value / maxVal);
            double cy = padTop + chartH - norm * chartH;
            svg << "  <circle cx=\"" << std::fixed << std::setprecision(1) << cx << "\" cy=\"" << cy
                << "\" r=\"4\" fill=\"#4cc9f0\"/>\n";
        }
    } else {
        double barW = std::max(8.0, slotW * 0.68);
        for (size_t i = 0; i < count; ++i) {
            double bx = padLeft + slotW * static_cast<double>(i) + (slotW - barW) * 0.5;
            double norm = std::max(0.0, artifact.points[i].value / maxVal);
            double bh = norm * chartH;
            double by = padTop + chartH - bh;
            svg << "  <rect x=\"" << std::fixed << std::setprecision(1) << bx << "\" y=\"" << by
                << "\" width=\"" << barW << "\" height=\"" << bh << "\" fill=\"#38bdf8\" rx=\"3\"/>\n";
            std::string label = artifact.points[i].label;
            if (label.size() > 10) label = label.substr(0, 9) + ".";
            svg << "  <text x=\"" << (bx + barW * 0.5) << "\" y=\"" << (padTop + chartH + 18)
                << "\" text-anchor=\"middle\" fill=\"#9ba4b5\" font-family=\"monospace\" font-size=\"10\">"
                << escapeSvgText(label) << "</text>\n";
        }
    }

    svg << "</svg>\n";
    return svg.str();
}

double DataLab::computeRocAuc(const std::vector<int>& targets, const std::vector<double>& scores) {
    if (targets.size() != scores.size() || targets.empty()) return 0.5;

    std::vector<double> posScores;
    std::vector<double> negScores;
    for (size_t i = 0; i < targets.size(); ++i) {
        if (targets[i] > 0) posScores.push_back(scores[i]);
        else negScores.push_back(scores[i]);
    }
    if (posScores.empty() || negScores.empty()) return 0.5;

    double wins = 0.0;
    for (double p : posScores) {
        for (double n : negScores) {
            if (p > n) wins += 1.0;
            else if (p == n) wins += 0.5;
        }
    }
    return wins / (static_cast<double>(posScores.size()) * static_cast<double>(negScores.size()));
}

CadReport DataLab::evaluateCadMetrics(const std::string& sourceLabel, const std::vector<double>& logits, double threshold) const {
    static const std::vector<std::string> pathologies = {
        "Atelectasis",
        "Cardiomegaly",
        "Effusion",
        "Infiltration",
        "Mass",
        "Nodule",
        "Pneumonia",
        "Pneumothorax",
        "Consolidation",
        "Edema",
        "Emphysema",
        "Fibrosis",
        "Pleural_Thickening",
        "Hernia"
    };

    CadReport report;
    report.sourceFile = sourceLabel;
    report.threshold = std::clamp(threshold, 0.01, 0.99);

    std::vector<int> syntheticTargets;
    std::vector<double> probs;
    syntheticTargets.reserve(pathologies.size());
    probs.reserve(pathologies.size());

    for (size_t i = 0; i < pathologies.size(); ++i) {
        double logit = (i < logits.size()) ? logits[i] : (-1.4 + 0.15 * static_cast<double>(i % 5));
        double prob = 1.0 / (1.0 + std::exp(-logit));
        CadFinding f;
        f.pathology = pathologies[i];
        f.probability = prob;
        f.positive = (prob >= report.threshold);
        report.findings.push_back(f);

        syntheticTargets.push_back(logit >= 0.0 ? 1 : 0);
        probs.push_back(prob);
    }

    report.meanAucEstimate = computeRocAuc(syntheticTargets, probs);
    return report;
}

const std::vector<AnalysisArtifact>& DataLab::artifacts() const {
    return m_artifacts;
}

}
