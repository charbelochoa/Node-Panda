#pragma once

#include <string>

namespace nodepanda {

class App;

class OutlinerPanel {
public:
    void render(App& app);
    void zoomToBlock(const std::string& uuid);

private:
    void renderBlockTree(App& app, const std::string& uuid, int visualDepth);

    std::string m_zoomedBlockUuid;
    std::string m_editingBlockUuid;
    std::string m_selectedBlockUuid;
    std::string m_lastPageId;
    char m_editBuffer[2048] = {};
    char m_searchBuffer[256] = {};
    char m_newBlockBuffer[512] = {};
    bool m_focusEditInput = false;
    bool m_initialized = false;
};

}
