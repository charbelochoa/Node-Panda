#pragma once

#include "vector_engine.h"
#include <string>
#include <vector>

namespace nodepanda {

class App;

class VectorPanel {
public:
    void render(App& app);

private:
    void syncConfigBuffers(const VectorConfig& cfg);
    void applyConfig(VectorEngine& engine);
    void renderResultCard(const VectorHit& hit, int index, App& app);

    char m_queryBuf[512] = {};
    char m_modelBuf[128] = {};
    char m_endpointBuf[256] = {};
    char m_apiKeyBuf[256] = {};
    int m_modeIdx = 0;
    float m_minScore = 0.15f;
    int m_topK = 10;
    bool m_configSynced = false;
    bool m_hasSearched = false;
    bool m_showingSimilar = false;
    std::string m_similarSourceId;
    std::string m_lastQuery;
    std::vector<VectorHit> m_results;
};

}
