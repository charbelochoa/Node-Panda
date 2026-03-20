#include "graph.h"
#include <cmath>
#include <random>

namespace nodepanda {

void Graph::clear() {
    m_nodes.clear();
    m_edges.clear();
    m_idToIndex.clear();
}

void Graph::buildFromNotes(const std::unordered_map<std::string, std::vector<std::string>>& noteLinks,
                           const std::unordered_map<std::string, std::string>& noteTypes) {
    clear();

    for (const auto& [noteId, links] : noteLinks) {
        GraphNode node;
        node.noteId = noteId;
        node.connectionCount = static_cast<int>(links.size());

        auto typeIt = noteTypes.find(noteId);
        if (typeIt != noteTypes.end()) {
            node.nodeType = typeIt->second;
        }

        m_idToIndex[noteId] = static_cast<int>(m_nodes.size());
        m_nodes.push_back(std::move(node));
    }

    for (const auto& [noteId, links] : noteLinks) {
        int fromIdx = findNodeIndex(noteId);
        if (fromIdx < 0) continue;

        for (const auto& linkTarget : links) {
            int toIdx = findNodeIndex(linkTarget);
            if (toIdx < 0) {
                GraphNode phantom;
                phantom.noteId = linkTarget;
                phantom.color[0] = 0.5f; phantom.color[1] = 0.5f;
                phantom.color[2] = 0.5f; phantom.color[3] = 0.6f;
                toIdx = static_cast<int>(m_nodes.size());
                m_idToIndex[linkTarget] = toIdx;
                m_nodes.push_back(std::move(phantom));
            }

            bool exists = false;
            for (const auto& edge : m_edges) {
                if ((edge.from == fromIdx && edge.to == toIdx) ||
                    (edge.from == toIdx && edge.to == fromIdx)) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                m_edges.push_back({fromIdx, toIdx});
                m_nodes[fromIdx].connectionCount++;
                m_nodes[toIdx].connectionCount++;
            }
        }
    }

    for (auto& node : m_nodes) {
        float intensity = std::min(1.0f, node.connectionCount / 5.0f);
        node.color[0] = 0.2f + intensity * 0.8f;
        node.color[1] = 0.5f + intensity * 0.3f;
        node.color[2] = 1.0f - intensity * 0.6f;
        node.color[3] = 1.0f;
        node.radius = 10.0f + std::min(node.connectionCount * 2.5f, 20.0f);
    }
}

int Graph::findNodeIndex(const std::string& noteId) const {
    auto it = m_idToIndex.find(noteId);
    if (it == m_idToIndex.end()) return -1;
    return it->second;
}

int Graph::getNodeAtPosition(float x, float y) const {
    for (int i = static_cast<int>(m_nodes.size()) - 1; i >= 0; --i) {
        float dx = x - m_nodes[i].x;
        float dy = y - m_nodes[i].y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= m_nodes[i].radius) {
            return i;
        }
    }
    return -1;
}

} 
