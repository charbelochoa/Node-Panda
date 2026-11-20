#pragma once

#include "note_manager.h"
#include <string>
#include <vector>

namespace nodepanda {

struct ContextHit {
    std::string noteId;
    std::string title;
    std::string excerpt;
    int relevance = 0;
    int estimatedTokens = 0;
};

struct ContextRetrieval {
    std::vector<ContextHit> hits;
    int estimatedTokens = 0;
    int maxTokens = 1800;
    std::string method = "lexical-datalog";
    std::string formattedXml;
};

class ContextBrain {
public:
    ContextRetrieval retrieve(const NoteManager& nm, const std::string& query, int maxTokens = 1800) const;
    std::vector<std::string> extractWikiLinks(const std::string& text) const;
    std::vector<std::string> extractTags(const std::string& text) const;
    std::vector<std::string> getBacklinks(const NoteManager& nm, const std::string& title) const;
};

}
