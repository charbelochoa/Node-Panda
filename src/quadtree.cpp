#include "quadtree.h"
#include <cmath>
#include <limits>
#include <algorithm>

namespace nodepanda {

void QuadTree::clear() {
    m_nodes.clear();
}

int QuadTree::createNode(float x0, float y0, float x1, float y1) {
    QuadTreeNode node;
    node.x0 = x0; node.y0 = y0;
    node.x1 = x1; node.y1 = y1;
    node.isLeaf = true;
    node.bodyIndex = -1;
    node.totalMass = 0;
    node.cx = 0; node.cy = 0;
    for (int i = 0; i < 4; ++i) node.children[i] = -1;
    m_nodes.push_back(node);
    return static_cast<int>(m_nodes.size()) - 1;
}

void QuadTree::build(const std::vector<float>& posX, const std::vector<float>& posY, int count) {
    clear();
    if (count == 0) return;

    // Pre-reservar para evitar realocaciones frecuentes durante inserciones
    // Un quadtree con N cuerpos tiene ~2*N nodos internos en promedio
    m_nodes.reserve(static_cast<size_t>(count) * 4 + 16);

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (int i = 0; i < count; ++i) {
        minX = std::min(minX, posX[i]);
        minY = std::min(minY, posY[i]);
        maxX = std::max(maxX, posX[i]);
        maxY = std::max(maxY, posY[i]);
    }

    float padding = 10.0f;
    float side = std::max(maxX - minX, maxY - minY) + padding * 2;
    float cx = (minX + maxX) / 2.0f;
    float cy = (minY + maxY) / 2.0f;

    createNode(cx - side / 2, cy - side / 2, cx + side / 2, cy + side / 2);

    for (int i = 0; i < count; ++i) {
        insert(0, i, posX[i], posY[i]);
    }
}

void QuadTree::subdivide(int nodeIdx) {
    // IMPORTANTE: NO mantener referencias a m_nodes[nodeIdx] porque
    // createNode() hace push_back() y puede reasignar el vector,
    // invalidando cualquier referencia o puntero previo.
    float x0   = m_nodes[nodeIdx].x0;
    float y0   = m_nodes[nodeIdx].y0;
    float x1   = m_nodes[nodeIdx].x1;
    float y1   = m_nodes[nodeIdx].y1;
    float midX = (x0 + x1) / 2.0f;
    float midY = (y0 + y1) / 2.0f;

    // Reservar espacio para evitar realocaciones durante la creación
    m_nodes.reserve(m_nodes.size() + 4);

    m_nodes[nodeIdx].children[0] = createNode(x0,   y0,   midX, midY);
    m_nodes[nodeIdx].children[1] = createNode(midX, y0,   x1,   midY);
    m_nodes[nodeIdx].children[2] = createNode(x0,   midY, midX, y1);
    m_nodes[nodeIdx].children[3] = createNode(midX, midY, x1,   y1);
    m_nodes[nodeIdx].isLeaf = false;
}

int QuadTree::getQuadrant(int nodeIdx, float px, float py) const {
    const auto& node = m_nodes[nodeIdx];
    float midX = (node.x0 + node.x1) / 2.0f;
    float midY = (node.y0 + node.y1) / 2.0f;

    if (px <= midX) {
        return (py <= midY) ? 0 : 2; 
    } else {
        return (py <= midY) ? 1 : 3; 
    }
}

void QuadTree::insert(int nodeIdx, int bodyIdx, float bx, float by) {
    // IMPORTANTE: NO mantener referencias persistentes a m_nodes[nodeIdx]
    // porque subdivide() e insert() recursivo hacen push_back() y pueden
    // reasignar el vector, invalidando cualquier referencia previa.

    if (m_nodes[nodeIdx].totalMass == 0 && m_nodes[nodeIdx].isLeaf) {
        m_nodes[nodeIdx].bodyIndex = bodyIdx;
        m_nodes[nodeIdx].cx = bx;
        m_nodes[nodeIdx].cy = by;
        m_nodes[nodeIdx].totalMass = 1;
        return;
    }

    if (m_nodes[nodeIdx].isLeaf && m_nodes[nodeIdx].bodyIndex >= 0) {
        int existingBody = m_nodes[nodeIdx].bodyIndex;
        float existX = m_nodes[nodeIdx].cx;
        float existY = m_nodes[nodeIdx].cy;
        m_nodes[nodeIdx].bodyIndex = -1;

        subdivide(nodeIdx);

        int q1 = getQuadrant(nodeIdx, existX, existY);
        insert(m_nodes[nodeIdx].children[q1], existingBody, existX, existY);
    }

    if (!m_nodes[nodeIdx].isLeaf) {
        int q = getQuadrant(nodeIdx, bx, by);
        insert(m_nodes[nodeIdx].children[q], bodyIdx, bx, by);
    }

    // Actualizar centro de masa (re-indexar cada acceso por seguridad)
    float oldMass = m_nodes[nodeIdx].totalMass;
    float newMass = oldMass + 1;
    m_nodes[nodeIdx].cx = (m_nodes[nodeIdx].cx * oldMass + bx) / newMass;
    m_nodes[nodeIdx].cy = (m_nodes[nodeIdx].cy * oldMass + by) / newMass;
    m_nodes[nodeIdx].totalMass = newMass;
}

void QuadTree::calculateForce(float px, float py, float repulsionStrength,
                               float theta, float& outFx, float& outFy) const {
    outFx = 0;
    outFy = 0;
    if (m_nodes.empty()) return;
    calculateForceRecursive(0, px, py, repulsionStrength, theta, outFx, outFy);
}

void QuadTree::calculateForceRecursive(int nodeIdx, float px, float py,
                                        float repulsionStrength, float theta,
                                        float& fx, float& fy) const {
    const auto& node = m_nodes[nodeIdx];
    if (node.totalMass == 0) return;

    float dx = node.cx - px;
    float dy = node.cy - py;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 0.1f) dist = 0.1f; 

    float width = node.x1 - node.x0;

    if (node.isLeaf || (width / dist) < theta) {
        float force = -repulsionStrength * node.totalMass / (dist * dist);
        fx += (dx / dist) * force;
        fy += (dy / dist) * force;
    } else {
        for (int i = 0; i < 4; ++i) {
            if (node.children[i] >= 0) {
                calculateForceRecursive(node.children[i], px, py,
                                         repulsionStrength, theta, fx, fy);
            }
        }
    }
}

} 
