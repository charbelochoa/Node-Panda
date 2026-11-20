#pragma once

#include <string>

namespace nodepanda {

class App;

class TaskPanel {
public:
    void render(App& app);

private:
    void renderListView(App& app);
    void renderKanbanView(App& app);

    int m_viewMode = 0;
    int m_statusFilterIdx = 0;
    int m_priorityFilterIdx = 0;
    int m_newTaskPriorityIdx = 2;
    char m_searchBuffer[256] = {};
    char m_newTitleBuffer[256] = {};
    char m_newDeadlineBuffer[32] = {};
    bool m_initialized = false;
};

}
