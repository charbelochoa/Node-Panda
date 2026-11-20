#pragma once

#include <functional>
#include <string>
#include <vector>

namespace nodepanda {

class App;

struct PaletteCommand {
    std::string id;
    std::string title;
    std::string category;
    std::string shortcut;
    std::function<void(App&)> action;
};

class CommandPalette {
public:
    CommandPalette();

    void open();
    void close();
    bool isOpen() const { return m_open; }

    void registerCommand(const PaletteCommand& cmd);
    void registerDefaultCommands();
    void render(App& app);

private:
    struct MatchEntry {
        std::string title;
        std::string category;
        std::string shortcut;
        std::string noteId;
        int commandIndex = -1;
        int score = 0;
    };

    static int fuzzyScore(const std::string& query, const std::string& candidate);

    bool m_open = false;
    bool m_focusInput = false;
    int m_selectedIndex = 0;
    char m_searchBuf[256] = {};
    std::vector<PaletteCommand> m_commands;
};

}
