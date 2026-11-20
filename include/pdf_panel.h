#pragma once

#include <string>

namespace nodepanda {

class App;

class PdfPanel {
public:
    void render(App& app);

private:
    char m_pathBuf[512] = {};
    char m_commentBuf[512] = {};
    int m_currentPage = 0;
    float m_zoom = 1.0f;
    int m_activeColorIndex = 0;
    bool m_filterCurrentPage = false;

    bool m_isDragging = false;
    float m_dragStartNormX = 0.0f;
    float m_dragStartNormY = 0.0f;
    float m_dragCurrNormX = 0.0f;
    float m_dragCurrNormY = 0.0f;

    std::string m_selectedHighlightId;
    std::string m_editingHighlightId;
};

}
