#pragma once


#include <string>

namespace nodepanda {

class App;

class LuaConsole {
public:
    void render(App& app);

private:
    static constexpr size_t INPUT_SIZE = 4096;
    char m_inputBuffer[INPUT_SIZE] = {};
    bool m_scrollToBottom = false;
    bool m_focusInput     = false;
};

} 
