#pragma once

#include <string>

namespace nodepanda {

class App;

class AssetPanel {
public:
    void render(App& app);

private:
    char m_importPathBuf[512] = {};
    char m_filterBuf[256] = {};
    std::string m_statusMsg;
    bool m_statusIsError = false;
};

}
