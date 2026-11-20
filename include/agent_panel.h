#pragma once

#include "context_brain.h"
#include <map>
#include <string>

namespace nodepanda {

class App;

class AgentPanel {
public:
    void render(App& app);

private:
    void renderMissionsTab(App& app);
    void renderRoomsTab(App& app);
    void renderContextTab(App& app);
    void renderSkillsTab(App& app);
    void renderProvidersTab(App& app);
    void renderUsageTab(App& app);

    int m_activeTab = 0;

    char m_promptBuf[2048] = {};
    char m_roomBuf[128] = "general";
    int m_modelIdx = 0;
    int m_reviewerIdx = 0;
    float m_budgetUsd = 0.50f;
    int m_maxIters = 6;
    std::string m_selectedRunId;

    char m_chatRoomBuf[128] = "general";
    char m_chatFromBuf[64] = "operator";
    char m_chatToBuf[64] = "all";
    char m_chatTextBuf[512] = {};

    char m_ctxQueryBuf[256] = {};
    int m_ctxMaxTokens = 1800;
    ContextRetrieval m_lastRetrieval;
    bool m_hasRetrieval = false;

    char m_newSkillName[128] = {};
    char m_newSkillDesc[256] = {};
    char m_newSkillContent[2048] = {};

    std::map<std::string, std::string> m_keyInputs;
    char m_localEndpointBuf[256] = {};
    bool m_localEndpointInit = false;

    char m_exportCsvPath[256] = "data/usage_export.csv";
};

}
