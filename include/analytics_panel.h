#pragma once
#include "graph_analytics.h"

namespace nodepanda {
class App;

class AnalyticsPanel {
public:
    void render(App& app);
private:
    GraphAnalytics m_analytics;
    int m_cachedNodeCount = -1;
    int m_selectedTab = 0;
};
}
