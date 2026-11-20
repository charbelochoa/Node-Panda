#pragma once
#include <string>

namespace nodepanda {
class App;

class HttpPanel {
public:
    void render(App& app);
private:
    bool m_wasRunning = false;
};
}
