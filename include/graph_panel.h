#pragma once
#include "app.h"
#include <cmath>

namespace nodepanda {

class GraphPanel {
public:
    void render(App& app);

private:
    void render2D(App& app);
    void render3D(App& app);

    float m_zoom = 1.0f;
    float m_panX = 0.0f, m_panY = 0.0f;
    int m_draggedNode = -1;
    bool m_isPanning = false;
    bool m_autoCenter = true;
    float m_lastMouseX = 0.0f, m_lastMouseY = 0.0f;
    int   m_lodLevel = 0;
    int   m_nodesVisible = 0;

    bool  m_mode3D     = false;
    float m_camYaw     = 0.4f;
    float m_camPitch   = 0.3f;
    float m_camDist    = 600.0f;
    float m_fov        = 800.0f;

    struct Projected { float sx, sy, depth, scale; };
    Projected project3D(float gx, float gy, float gz,
                        float cx, float cy, float cw, float ch) const {

        float cosY = std::cos(m_camYaw),  sinY = std::sin(m_camYaw);
        float cosP = std::cos(m_camPitch), sinP = std::sin(m_camPitch);

        float x1 = gx * cosY - gz * sinY;
        float z1 = gx * sinY + gz * cosY;
        float y1 = gy;

        float y2 = y1 * cosP - z1 * sinP;
        float z2 = y1 * sinP + z1 * cosP;

        z2 += m_camDist;

        float s = (z2 > 10.0f) ? m_fov / z2 : m_fov / 10.0f;

        return {
            cx + cw * 0.5f + x1 * s,
            cy + ch * 0.5f + y2 * s,
            z2,
            s
        };
    }
};

}
