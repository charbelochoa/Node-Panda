#pragma once
#include "git_manager.h"
#include <string>

namespace nodepanda {
class App;

class GitPanel {
public:
    void render(App& app);
private:
    std::vector<GitCommitInfo> m_history;
    std::vector<GitDiffLine> m_diff;
    std::string m_selectedCommit;
    bool m_showDiff = false;
    int m_cachedNoteCount = -1;
    bool m_autoCommit = true;
    int m_frameCounter = 0;
};
}
