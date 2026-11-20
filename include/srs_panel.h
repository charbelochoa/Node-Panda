#pragma once
#include "srs_engine.h"

namespace nodepanda {
class App;

class SRSPanel {
public:
    void render(App& app);
private:
    SRSEngine m_engine;
    bool m_initialized = false;
    int m_currentCard = 0;
    bool m_showAnswer = false;
    int m_selectedTab = 0;
};
}
