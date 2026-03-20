#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace nodepanda {

struct GraphNode {
    std::string noteId;
    std::string nodeType;
    float x = 0.0f, y = 0.0f, z = 0.0f;       // posición 2D/3D
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;    // velocidad 2D/3D
    float radius = 20.0f;
    float color[4] = {0.3f, 0.7f, 1.0f, 1.0f};
    int connectionCount = 0;
    bool pinned = false;
};

struct GraphEdge {
    int from;  
    int to;
};

class Graph {
public:
    void clear();

    
    void buildFromNotes(const std::unordered_map<std::string, std::vector<std::string>>& noteLinks,
                        const std::unordered_map<std::string, std::string>& noteTypes = {});

    
    std::vector<GraphNode>& getNodes() { return m_nodes; }
    const std::vector<GraphNode>& getNodes() const { return m_nodes; }
    std::vector<GraphEdge>& getEdges() { return m_edges; }
    const std::vector<GraphEdge>& getEdges() const { return m_edges; }

    int findNodeIndex(const std::string& noteId) const;

    
    int getNodeAtPosition(float x, float y) const;

private:
    std::vector<GraphNode> m_nodes;
    std::vector<GraphEdge> m_edges;
    std::unordered_map<std::string, int> m_idToIndex;
};

} 
