#include "force_layout.h"
#include <cmath>
#include <random>

namespace nodepanda {

void ForceLayout::initialize(Graph& graph, float canvasWidth, float canvasHeight) {
    m_canvasWidth = canvasWidth;
    m_canvasHeight = canvasHeight;
    m_temperature = 1.0f;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> distX(canvasWidth * 0.2f, canvasWidth * 0.8f);
    std::uniform_real_distribution<float> distY(canvasHeight * 0.2f, canvasHeight * 0.8f);

    std::uniform_real_distribution<float> distZ(-canvasWidth * 0.3f, canvasWidth * 0.3f);
    for (auto& node : graph.getNodes()) {
        if (!node.pinned) {
            node.x = distX(rng);
            node.y = distY(rng);
            if (mode3D) node.z = distZ(rng);
        }
        node.vx = 0;
        node.vy = 0;
        node.vz = 0;
    }
    m_initialized = true;
}

void ForceLayout::step(Graph& graph) {
    if (!m_initialized) return;
    if (m_temperature < minTemperature) return;

    auto& nodes = graph.getNodes();
    auto& edges = graph.getEdges();
    int n = static_cast<int>(nodes.size());
    if (n == 0) return;

    for (auto& node : nodes) {
        if (!node.pinned) {
            node.vx = 0;
            node.vy = 0;
            node.vz = 0;
        }
    }

    
    if (useBarnesHut && n > 20) {
        std::vector<float> posX(n), posY(n);
        for (int i = 0; i < n; ++i) {
            posX[i] = nodes[i].x;
            posY[i] = nodes[i].y;
        }
        m_quadTree.build(posX, posY, n);

        for (int i = 0; i < n; ++i) {
            if (nodes[i].pinned) continue;
            float fx = 0, fy = 0;
            m_quadTree.calculateForce(nodes[i].x, nodes[i].y,
                                       repulsionStrength, barnesHutTheta, fx, fy);
            nodes[i].vx += fx;
            nodes[i].vy += fy;
        }
    } else {
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                float dx = nodes[i].x - nodes[j].x;
                float dy = nodes[i].y - nodes[j].y;
                float dz = mode3D ? (nodes[i].z - nodes[j].z) : 0.0f;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist < 1.0f) dist = 1.0f;

                float force = repulsionStrength / (dist * dist);
                float fx = (dx / dist) * force;
                float fy = (dy / dist) * force;
                float fz = mode3D ? (dz / dist) * force : 0.0f;

                if (!nodes[i].pinned) { nodes[i].vx += fx; nodes[i].vy += fy; nodes[i].vz += fz; }
                if (!nodes[j].pinned) { nodes[j].vx -= fx; nodes[j].vy -= fy; nodes[j].vz -= fz; }
            }
        }
    }


    for (const auto& edge : edges) {
        if (edge.from < 0 || edge.from >= n || edge.to < 0 || edge.to >= n) continue;
        auto& a = nodes[edge.from];
        auto& b = nodes[edge.to];
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float dz = mode3D ? (b.z - a.z) : 0.0f;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < 1.0f) dist = 1.0f;

        float force = dist * attractionStrength;
        float fx = (dx / dist) * force;
        float fy = (dy / dist) * force;
        float fz = mode3D ? (dz / dist) * force : 0.0f;

        if (!a.pinned) { a.vx += fx; a.vy += fy; a.vz += fz; }
        if (!b.pinned) { b.vx -= fx; b.vy -= fy; b.vz -= fz; }
    }

    
    float centroidX = 0.0f, centroidY = 0.0f, centroidZ = 0.0f;
    for (const auto& node : nodes) {
        centroidX += node.x;
        centroidY += node.y;
        if (mode3D) centroidZ += node.z;
    }
    centroidX /= n;
    centroidY /= n;
    if (mode3D) centroidZ /= n;

    for (auto& node : nodes) {
        if (!node.pinned) {
            float dx = centroidX - node.x;
            float dy = centroidY - node.y;
            float dz = mode3D ? (centroidZ - node.z) : 0.0f;
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            node.vx += dx * centralGravity;
            node.vy += dy * centralGravity;
            if (mode3D) node.vz += dz * centralGravity;
            if (dist > 200.0f) {
                float extraForce = (dist - 200.0f) * centralGravity * 2.0f;
                if (dist > 0.0f) {
                    node.vx += (dx / dist) * extraForce;
                    node.vy += (dy / dist) * extraForce;
                    if (mode3D) node.vz += (dz / dist) * extraForce;
                }
            }
        }
    }

    
    float maxDisplacement = std::max(10.0f, 50.0f * m_temperature);
    float totalMovement = 0.0f;  

    for (auto& node : nodes) {
        if (node.pinned) continue;

        float speed = std::sqrt(node.vx * node.vx + node.vy * node.vy + node.vz * node.vz);
        if (speed > maxDisplacement) {
            float scale = maxDisplacement / speed;
            node.vx *= scale;
            node.vy *= scale;
            node.vz *= scale;
        }

        node.x += node.vx * damping;
        node.y += node.vy * damping;
        if (mode3D) node.z += node.vz * damping;
        totalMovement += std::abs(node.vx) + std::abs(node.vy) + std::abs(node.vz);
    }

    m_temperature *= coolingRate;

    if (n > 0 && (totalMovement / (float)n) < 0.01f) {
        m_temperature = minTemperature * 0.5f;
    }
}

} 
