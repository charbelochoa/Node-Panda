#pragma once

namespace nodepanda {

class App;

class PluginPanel {
public:
    void render(App& app);

private:
    char m_searchBuf[128] = {};
};

}
