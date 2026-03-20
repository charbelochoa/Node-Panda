#pragma once
#include "app.h"

namespace nodepanda {

class BacklinksPanel {
public:
    void render(App& app);

private:
    
    std::string m_cachedNoteId;
    std::vector<std::string> m_backlinks;          
    std::vector<std::pair<std::string, std::string>> m_unlinkedMentions; 

    void recalculate(App& app);
};

} 
