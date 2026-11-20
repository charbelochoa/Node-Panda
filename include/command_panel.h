#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace nodepanda {

class App;

class CommandPanel {
public:
    void render(App& app);

private:

    struct Stats {
        int totalNotes      = 0;
        int totalLinks      = 0;
        int orphanNotes     = 0;
        int brokenLinks     = 0;
        int typeCount       = 0;
        float avgLinksPerNote = 0.0f;
        float graphDensity   = 0.0f;
        std::vector<std::pair<std::string, int>> topConnected;
        std::vector<std::pair<std::string, int>> typeDistribution;
        std::vector<std::string> orphanList;
        std::vector<std::string> brokenLinkList;
        std::vector<std::pair<std::string, std::string>> recentNotes;
    };

    Stats m_stats;
    int   m_cachedNoteCount = -1;
    bool  m_showOrphans     = false;
    bool  m_showBroken      = false;

    void recalcStats(App& app);
};

}
