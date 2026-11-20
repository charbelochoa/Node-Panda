#pragma once

#include <string>

namespace nodepanda {

class App;

class MemoryPanel {
public:
    void render(App& app);

private:

    static constexpr size_t SEARCH_SIZE = 512;
    char m_searchBuffer[SEARCH_SIZE] = {};
    bool m_searchTriggered = false;

    std::string m_cachedNoteId;
    int         m_cachedNoteCount = -1;
};

}
