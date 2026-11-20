#pragma once

#include <string>

namespace nodepanda {

class App;

class JournalPanel {
public:
    JournalPanel();
    void render(App& app);

private:
    void renderCalendar(App& app);
    void renderTimeline(App& app);

    int m_viewYear = 2026;
    int m_viewMonth = 1;
    char m_captureBuffer[512] = {};
};

}
