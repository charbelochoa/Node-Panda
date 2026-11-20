#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nodepanda {

struct ColumnProfile {
    std::string name;
    std::string inferredType;
    int nonEmpty = 0;
    int nullCount = 0;
    double minVal = 0.0;
    double maxVal = 0.0;
};

struct TableProfile {
    bool ok = false;
    std::string sourcePath;
    int rowCount = 0;
    int colCount = 0;
    std::vector<ColumnProfile> columns;
    std::string sha256;
    std::string error;
};

struct AnalysisPlan {
    std::string aggregation = "count";
    std::string metric;
    std::string groupBy;
};

struct AnalysisPoint {
    std::string label;
    double value = 0.0;
};

struct AnalysisArtifact {
    std::string id;
    std::string sourcePath;
    std::string sha256;
    AnalysisPlan plan;
    std::vector<AnalysisPoint> points;
    double totalValue = 0.0;
    int processedRows = 0;
    bool ok = false;
    std::string error;
};

struct DocumentHit {
    int lineNumber = 0;
    std::string excerpt;
};

struct CadFinding {
    std::string pathology;
    double probability = 0.0;
    bool positive = false;
};

struct CadReport {
    std::string sourceFile;
    double threshold = 0.5;
    double meanAucEstimate = 0.0;
    std::vector<CadFinding> findings;
};

class DataLab {
public:
    TableProfile profileCsv(const std::filesystem::path& filePath) const;
    AnalysisArtifact analyzeCsv(const std::filesystem::path& filePath, const AnalysisPlan& plan);
    std::vector<DocumentHit> searchDocument(const std::filesystem::path& filePath, const std::string& query) const;
    std::string renderChartSvg(const AnalysisArtifact& artifact, const std::string& kind = "bar") const;
    CadReport evaluateCadMetrics(const std::string& sourceLabel, const std::vector<double>& logits, double threshold = 0.5) const;
    static double computeRocAuc(const std::vector<int>& targets, const std::vector<double>& scores);
    const std::vector<AnalysisArtifact>& artifacts() const;

private:
    std::vector<AnalysisArtifact> m_artifacts;
};

}
