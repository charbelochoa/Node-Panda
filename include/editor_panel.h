#pragma once
#include "app.h"
#include "markdown_parser.h"
#include <map>
#include <string>
#include <vector>
#include <utility>

namespace nodepanda {

class EditorPanel {
public:
    void render(App& app);

private:
    
    void         renderPandaBanner(App& app);
    unsigned int m_pandaTexId  = 0;     
    bool         m_pandaLoaded = false; 

    static constexpr size_t BUFFER_SIZE = 1024 * 64; 
    char m_editBuffer[BUFFER_SIZE] = {};
    std::string m_currentNoteId;
    bool m_previewMode = false;

    static constexpr size_t FIELD_SIZE = 256;
    char m_newKey[FIELD_SIZE] = {};
    char m_newValue[FIELD_SIZE] = {};
    std::map<std::string, std::string> m_fieldBuffers;

    std::string m_backlinksCachedId;
    std::vector<std::string> m_cachedBacklinks;

    
    struct LuaCodeBlock {
        std::string code;
        size_t      startLine;  
    };
    std::vector<LuaCodeBlock> parseLuaBlocks(const std::string& content);

    
    int         m_exportDepth          = 1;
    int         m_exportCachedDepth    = -1;  
    std::string m_exportCachedNoteId;
    int         m_exportNoteCount      = 0;
    bool        m_justCopied           = false;
};

} 
