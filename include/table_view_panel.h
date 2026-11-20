#pragma once

#include <string>

namespace nodepanda {

class App;

class TableViewPanel {
public:
    TableViewPanel() = default;
    ~TableViewPanel() = default;

    void render(App& app);

private:
    void syncFilterBuffers(const std::string& viewId,
                           const std::string& tag,
                           const std::string& type,
                           const std::string& ns,
                           const std::string& search);
    void renderNewViewModal(App& app);
    void renderExportModal();
    void clearInlineEdit();

    std::string m_syncedViewId;
    std::string m_lastTableViewId;

    char m_tagFilterBuf[128] = {};
    char m_typeFilterBuf[128] = {};
    char m_nsFilterBuf[128] = {};
    char m_searchBuf[256] = {};

    bool m_openNewViewModal = false;
    char m_newViewNameBuf[128] = {};
    char m_newViewTagBuf[128] = {};
    char m_newViewTypeBuf[128] = {};
    char m_newViewNsBuf[128] = {};

    char m_newColKeyBuf[64] = {};
    int m_newColTypeIdx = 0;

    std::string m_editNoteId;
    std::string m_editColKey;
    char m_editCellBuf[512] = {};
    bool m_editFocusRequested = false;

    bool m_openExportModal = false;
    std::string m_exportTitle;
    std::string m_exportContent;
    std::string m_exportSavedPath;
};

}
