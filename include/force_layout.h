#pragma once
#include "graph.h"
#include "quadtree.h"

namespace nodepanda {

class ForceLayout {
public:
    float repulsionStrength  = 25000.0f;
    float attractionStrength =  0.003f;
    float damping = 0.85f;
    float minTemperature = 0.1f;
    float coolingRate = 0.995f;
    float centralGravity = 0.002f;
    float barnesHutTheta = 0.5f;
    bool useBarnesHut = true;
    bool mode3D = false;  // habilitar fuerzas en eje Z

    void initialize(Graph& graph, float canvasWidth, float canvasHeight);

    void step(Graph& graph);

    bool isStable() const { return m_temperature < minTemperature; }

    void reset() { m_temperature = 1.0f; m_initialized = false; }

private:
    float m_temperature = 1.0f;
    float m_canvasWidth = 800.0f;
    float m_canvasHeight = 600.0f;
    bool m_initialized = false;
    QuadTree m_quadTree;
};

} 
