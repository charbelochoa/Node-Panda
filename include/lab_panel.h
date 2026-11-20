#pragma once

#include "data_lab.h"
#include <array>
#include <string>
#include <vector>

namespace nodepanda {

class App;

class LabPanel {
public:
    LabPanel();
    void render(App& app);

private:
    void renderTabularTab(App& app);
    void renderDocumentsTab(App& app);
    void renderCadTab(App& app);
    void renderEvolutionTab(App& app);

    int m_activeTab = 0;

    char m_csvPathBuf[256] = "data/sample.csv";
    char m_metricColBuf[64] = {};
    char m_groupColBuf[64] = {};
    char m_svgOutPathBuf[256] = "data/chart_export.svg";
    int m_aggIdx = 0;
    TableProfile m_profile;
    AnalysisArtifact m_artifact;
    bool m_hasProfile = false;
    bool m_hasArtifact = false;

    char m_docPathBuf[256] = "README.md";
    char m_docQueryBuf[128] = {};
    std::vector<DocumentHit> m_docHits;

    char m_cadLabelBuf[128] = "chest_xray_sample_01";
    float m_cadThreshold = 0.50f;
    std::array<float, 14> m_cadLogits{};
    CadReport m_cadReport;
    bool m_hasCadReport = false;

    char m_casePromptBuf[256] = "sum revenue by region";
    char m_caseFileBuf[256] = "data/sample.csv";
    char m_caseSplitBuf[32] = "validation";
    float m_caseExpected = 100.0f;
    int m_optModeIdx = 0;
    std::string m_evoStatus;
};

}
