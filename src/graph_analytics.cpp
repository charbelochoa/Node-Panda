#include "graph_analytics.h"
#include "graph.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>

namespace nodepanda {

void GraphAnalytics::analyze(const Graph& graph) {
    m_metrics.clear();
    m_communityCount = 0;
    m_ready = false;

    const auto& nodes = graph.getNodes();

    for (const auto& node : nodes) {
        NodeMetrics metric;
        metric.noteId = node.noteId;
        m_metrics.push_back(metric);
    }

    if (m_metrics.empty()) {
        m_ready = true;
        return;
    }

    computePageRank(graph);
    computeBetweenness(graph);
    detectCommunities(graph);

    m_ready = true;
}

void GraphAnalytics::computePageRank(const Graph& graph) {
    const auto& nodes = graph.getNodes();
    const auto& edges = graph.getEdges();
    size_t n = nodes.size();

    if (n == 0) return;

    std::vector<float> pageRank(n, 1.0f / n);
    std::vector<float> newPageRank(n);

    std::vector<std::vector<int>> adj(n);
    std::vector<int> outDegree(n, 0);

    for (const auto& edge : edges) {
        if (edge.from >= 0 && edge.from < static_cast<int>(n) &&
            edge.to >= 0 && edge.to < static_cast<int>(n)) {
            adj[edge.to].push_back(edge.from);
            outDegree[edge.from]++;
            adj[edge.from].push_back(edge.to);
            outDegree[edge.to]++;
        }
    }

    const float d = 0.85f;
    const float threshold = 1e-6f;
    const int maxIterations = 50;

    for (int iter = 0; iter < maxIterations; ++iter) {
        std::fill(newPageRank.begin(), newPageRank.end(), (1.0f - d) / n);

        float maxDiff = 0.0f;
        for (size_t i = 0; i < n; ++i) {
            for (int j : adj[i]) {
                if (outDegree[j] > 0) {
                    newPageRank[i] += d * pageRank[j] / outDegree[j];
                }
            }
            maxDiff = std::max(maxDiff, std::abs(newPageRank[i] - pageRank[i]));
        }

        pageRank = newPageRank;

        if (maxDiff < threshold) {
            break;
        }
    }

    float maxPR = *std::max_element(pageRank.begin(), pageRank.end());
    if (maxPR > 0.0f) {
        for (float& pr : pageRank) {
            pr /= maxPR;
        }
    }

    for (size_t i = 0; i < n; ++i) {
        m_metrics[i].pageRank = pageRank[i];
    }
}

void GraphAnalytics::computeBetweenness(const Graph& graph) {
    const auto& nodes = graph.getNodes();
    const auto& edges = graph.getEdges();
    size_t n = nodes.size();

    if (n < 2) return;

    std::vector<float> betweenness(n, 0.0f);

    std::vector<std::vector<int>> adj(n);
    for (const auto& edge : edges) {
        if (edge.from >= 0 && edge.from < static_cast<int>(n) &&
            edge.to >= 0 && edge.to < static_cast<int>(n)) {
            adj[edge.from].push_back(edge.to);
            adj[edge.to].push_back(edge.from);
        }
    }

    for (size_t s = 0; s < n; ++s) {
        std::vector<std::vector<int>> predecessors(n);
        std::vector<int> pathCount(n, 0);
        std::vector<float> distance(n, -1.0f);
        std::vector<float> dependency(n, 0.0f);

        pathCount[s] = 1;
        distance[s] = 0.0f;

        std::queue<int> q;
        q.push(s);

        std::vector<int> order;
        while (!q.empty()) {
            int v = q.front();
            q.pop();
            order.push_back(v);

            for (int w : adj[v]) {
                if (distance[w] < 0.0f) {
                    distance[w] = distance[v] + 1.0f;
                    q.push(w);
                }
                if (distance[w] == distance[v] + 1.0f) {
                    pathCount[w] += pathCount[v];
                    predecessors[w].push_back(v);
                }
            }
        }

        for (int i = static_cast<int>(order.size()) - 1; i >= 0; --i) {
            int w = order[i];
            for (int v : predecessors[w]) {
                dependency[v] += (static_cast<float>(pathCount[v]) / pathCount[w]) *
                                 (1.0f + dependency[w]);
            }
            if (w != s) {
                betweenness[w] += dependency[w];
            }
        }
    }

    float normalization = (n - 1) * (n - 2) / 2.0f;
    if (normalization > 0.0f) {
        for (float& bc : betweenness) {
            bc /= normalization;
        }
    }

    for (size_t i = 0; i < n; ++i) {
        m_metrics[i].betweenness = betweenness[i];
    }
}

void GraphAnalytics::detectCommunities(const Graph& graph) {
    const auto& nodes = graph.getNodes();
    const auto& edges = graph.getEdges();
    size_t n = nodes.size();

    if (n == 0) return;

    std::vector<int> labels(n);
    for (size_t i = 0; i < n; ++i) {
        labels[i] = static_cast<int>(i);
    }

    std::vector<std::vector<int>> adj(n);
    for (const auto& edge : edges) {
        if (edge.from >= 0 && edge.from < static_cast<int>(n) &&
            edge.to >= 0 && edge.to < static_cast<int>(n)) {
            adj[edge.from].push_back(edge.to);
            adj[edge.to].push_back(edge.from);
        }
    }

    const int maxIterations = 100;
    for (int iter = 0; iter < maxIterations; ++iter) {
        bool changed = false;

        for (size_t i = 0; i < n; ++i) {
            std::unordered_map<int, int> labelCount;

            for (int neighbor : adj[i]) {
                labelCount[labels[neighbor]]++;
            }

            if (!labelCount.empty()) {
                int mostFrequentLabel = labels[i];
                int maxCount = 0;
                int smallestLabel = labels[i];

                for (const auto& [label, count] : labelCount) {
                    if (count > maxCount || (count == maxCount && label < smallestLabel)) {
                        mostFrequentLabel = label;
                        maxCount = count;
                        smallestLabel = label;
                    }
                }

                if (mostFrequentLabel != labels[i]) {
                    labels[i] = mostFrequentLabel;
                    changed = true;
                }
            }
        }

        if (!changed) {
            break;
        }
    }

    std::unordered_map<int, int> labelMap;
    int communityCounter = 0;

    for (size_t i = 0; i < n; ++i) {
        if (labelMap.find(labels[i]) == labelMap.end()) {
            labelMap[labels[i]] = communityCounter++;
        }
        m_metrics[i].communityId = labelMap[labels[i]];
    }

    m_communityCount = communityCounter;
}

std::vector<NodeMetrics> GraphAnalytics::topByPageRank(int n) const {
    auto sorted = m_metrics;
    std::sort(sorted.begin(), sorted.end(),
        [](const NodeMetrics& a, const NodeMetrics& b) {
            return a.pageRank > b.pageRank;
        });

    if (static_cast<int>(sorted.size()) < n) {
        return sorted;
    }
    return std::vector<NodeMetrics>(sorted.begin(), sorted.begin() + n);
}

std::vector<NodeMetrics> GraphAnalytics::topByBetweenness(int n) const {
    auto sorted = m_metrics;
    std::sort(sorted.begin(), sorted.end(),
        [](const NodeMetrics& a, const NodeMetrics& b) {
            return a.betweenness > b.betweenness;
        });

    if (static_cast<int>(sorted.size()) < n) {
        return sorted;
    }
    return std::vector<NodeMetrics>(sorted.begin(), sorted.begin() + n);
}

std::vector<std::string> GraphAnalytics::getCommunityMembers(int communityId) const {
    std::vector<std::string> members;
    for (const auto& metric : m_metrics) {
        if (metric.communityId == communityId) {
            members.push_back(metric.noteId);
        }
    }
    return members;
}

}
