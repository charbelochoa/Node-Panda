#pragma once
#include <vector>

namespace nodepanda {

struct QuadTreeNode {
    float cx = 0, cy = 0;    
    float totalMass = 0;      
    float x0, y0, x1, y1;    
    int bodyIndex = -1;       
    bool isLeaf = true;
    int children[4] = {-1, -1, -1, -1}; 
};

class QuadTree {
public:
    void build(const std::vector<float>& posX, const std::vector<float>& posY, int count);

    void calculateForce(float px, float py, float repulsionStrength,
                        float theta, float& outFx, float& outFy) const;

    void clear();

private:
    std::vector<QuadTreeNode> m_nodes;

    int createNode(float x0, float y0, float x1, float y1);
    void insert(int nodeIdx, int bodyIdx, float bx, float by);
    void subdivide(int nodeIdx);
    int getQuadrant(int nodeIdx, float px, float py) const;
    void calculateForceRecursive(int nodeIdx, float px, float py,
                                  float repulsionStrength, float theta,
                                  float& fx, float& fy) const;
};

} 
