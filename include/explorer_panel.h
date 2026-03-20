#pragma once
#include "app.h"

namespace nodepanda {

class ExplorerPanel {
public:
    void render(App& app);

private:
    char m_searchBuffer[256] = {};
    char m_newNoteName[128] = {};
    char m_newNoteFolder[128] = {};
    char m_newFolderName[128] = {};
    bool m_showNewNoteDialog = false;
    bool m_showNewFolderDialog = false;
    bool m_showDeleteConfirm = false;
    std::string m_deleteTargetId;
};

} 
