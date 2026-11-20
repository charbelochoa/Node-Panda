#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace nodepanda {

class Graph;
class App;

struct NodeMetrics {
    std::string noteId;
    float pageRank = 0.0f;
    float betweenness = 0.0f;
    int communityId = -1;
    int degree = 0;
};

class GraphAnalytics {
public:
    void analyze(const Graph& graph);

    const std::vector<NodeMetrics>& getMetrics() const { return m_metrics; }
    int communityCount() const { return m_communityCount; }
    bool isReady() const { return m_ready; }
    void invalidate() { m_ready = false; }

    std::vector<NodeMetrics> topByPageRank(int n) const;

    std::vector<NodeMetrics> topByBetweenness(int n) const;

    std::vector<std::string> getCommunityMembers(int communityId) const;

private:
    void computePageRank(const Graph& graph);
    void computeBetweenness(const Graph& graph);
    void detectCommunities(const Graph& graph);

    std::vector<NodeMetrics> m_metrics;
    int m_communityCount = 0;
    bool m_ready = false;
};

}
