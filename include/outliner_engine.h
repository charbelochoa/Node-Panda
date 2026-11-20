#pragma once

#include "note_manager.h"
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace nodepanda {

struct BlockNode {
    std::string uuid;
    std::string pageId;
    std::string parentUuid;
    std::string content;
    int indent = 0;
    int orderIndex = 0;
    bool collapsed = false;
    std::map<std::string, std::string> properties;
    std::vector<std::string> tags;
    std::vector<std::string> pageRefs;
    std::vector<std::string> blockRefs;
    std::vector<std::string> childrenUuids;
    int64_t updatedAt = 0;
};

class OutlinerEngine {
public:
    void rebuildFromNotes(const NoteManager& nm);

    std::vector<std::string> getRootBlocksForPage(const std::string& pageId) const;
    const BlockNode* getBlock(const std::string& uuid) const;
    BlockNode* getBlockMutable(const std::string& uuid);
    const std::unordered_map<std::string, BlockNode>& getAllBlocks() const;

    std::string createBlock(NoteManager& nm, const std::string& pageId, const std::string& content,
                            const std::string& parentUuid = "", bool firstChild = false);
    bool updateBlockContent(NoteManager& nm, const std::string& uuid, const std::string& newContent);
    bool deleteBlock(NoteManager& nm, const std::string& uuid);
    bool indentBlock(NoteManager& nm, const std::string& uuid);
    bool unindentBlock(NoteManager& nm, const std::string& uuid);
    bool moveBlockUp(NoteManager& nm, const std::string& uuid);
    bool moveBlockDown(NoteManager& nm, const std::string& uuid);

    std::string resolveBlockRefs(const std::string& text, int maxDepth = 6) const;
    std::vector<std::string> getBreadcrumbs(const std::string& uuid) const;
    std::vector<const BlockNode*> searchBlocks(const std::string& query) const;
    std::string serializePageMarkdown(const std::string& pageId) const;
    bool syncPageToNote(NoteManager& nm, const std::string& pageId);

private:
    void parseNoteBlocks(const Note& note);
    void parseBlockMetadata(BlockNode& block);
    void reindexSiblings(const std::string& pageId, const std::string& parentUuid);
    void updateSubtreeIndent(const std::string& uuid, int newIndent);
    void removeSubtreeRecursive(const std::string& uuid);
    void serializeBlockRecursive(const std::string& uuid, std::ostringstream& out) const;
    std::vector<std::string>* getSiblingList(const std::string& pageId, const std::string& parentUuid);
    static int64_t currentTimestamp();

    std::unordered_map<std::string, BlockNode> m_blocks;
    std::unordered_map<std::string, std::vector<std::string>> m_pageRoots;
};

}
