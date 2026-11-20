#pragma once

#include "imgui.h"
#include <string>

namespace nodepanda {

class App;

class WhiteboardPanel {
public:
    void render(App& app);

private:
    ImVec2 worldToScreen(const ImVec2& canvasOrigin, float wx, float wy) const;
    ImVec2 screenToWorld(const ImVec2& canvasOrigin, const ImVec2& sp) const;

    float m_panX = 40.0f;
    float m_panY = 40.0f;
    float m_zoom = 1.0f;

    std::string m_selectedNodeId;
    std::string m_draggingNodeId;
    std::string m_resizingNodeId;
    std::string m_connectFromId;
    bool m_connectMode = false;
    bool m_isPanning = false;
    ImVec2 m_dragOffsetWorld = ImVec2(0.0f, 0.0f);
    ImVec2 m_lastMousePos = ImVec2(0.0f, 0.0f);

    char m_newBoardName[128] = {};
    char m_edgeLabelBuffer[128] = {};
    char m_editTitleBuffer[256] = {};
    char m_editBodyBuffer[1024] = {};
    char m_editLinkBuffer[256] = {};
    std::string m_inspectorNodeId;
};

}
