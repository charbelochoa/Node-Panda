#include "outliner_engine.h"
#include "crypto_engine.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>

namespace nodepanda {

namespace {

std::string trimCopy(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string toLowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool parsePropertyLine(const std::string& trimmed, std::string& keyOut, std::string& valOut) {
    size_t pos = trimmed.find("::");
    if (pos == std::string::npos || pos == 0) return false;
    std::string key = trimCopy(trimmed.substr(0, pos));
    if (key.empty()) return false;
    for (unsigned char c : key) {
        if (!std::isalnum(c) && c != '_' && c != '-') return false;
    }
    keyOut = key;
    valOut = trimCopy(trimmed.substr(pos + 2));
    return true;
}

void pushUnique(std::vector<std::string>& vec, const std::string& val) {
    if (val.empty()) return;
    if (std::find(vec.begin(), vec.end(), val) == vec.end()) {
        vec.push_back(val);
    }
}

}

int64_t OutlinerEngine::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

void OutlinerEngine::rebuildFromNotes(const NoteManager& nm) {
    std::unordered_map<std::string, bool> collapsedStates;
    for (const auto& [id, block] : m_blocks) {
        if (block.collapsed) {
            collapsedStates[id] = true;
        }
    }

    m_blocks.clear();
    m_pageRoots.clear();

    for (const auto& [noteId, note] : nm.getAllNotes()) {
        parseNoteBlocks(note);
    }

    for (auto& [id, block] : m_blocks) {
        auto it = collapsedStates.find(id);
        if (it != collapsedStates.end()) {
            block.collapsed = it->second;
        }
    }
}

void OutlinerEngine::parseNoteBlocks(const Note& note) {
    std::istringstream iss(note.content);
    std::string rawLine;
    std::vector<std::pair<int, std::string>> indentStack;
    std::string lastBlockUuid;
    int lineIndex = 0;
    bool inFrontmatter = false;
    bool checkedFirstLine = false;

    while (std::getline(iss, rawLine)) {
        if (!rawLine.empty() && rawLine.back() == '\r') {
            rawLine.pop_back();
        }
        int currentLineIdx = lineIndex++;

        if (!checkedFirstLine) {
            checkedFirstLine = true;
            if (trimCopy(rawLine) == "---") {
                inFrontmatter = true;
                continue;
            }
        } else if (inFrontmatter) {
            if (trimCopy(rawLine) == "---") {
                inFrontmatter = false;
            }
            continue;
        }

        std::string trimmed = trimCopy(rawLine);
        if (trimmed.empty()) continue;

        int spaces = 0;
        for (char c : rawLine) {
            if (c == ' ') spaces += 1;
            else if (c == '\t') spaces += 2;
            else break;
        }

        std::string propKey;
        std::string propVal;
        if (!lastBlockUuid.empty() && parsePropertyLine(trimmed, propKey, propVal)) {
            auto blockIt = m_blocks.find(lastBlockUuid);
            if (blockIt != m_blocks.end()) {
                if (propKey == "id" && !propVal.empty() && propVal != lastBlockUuid &&
                    m_blocks.find(propVal) == m_blocks.end()) {
                    BlockNode moved = std::move(blockIt->second);
                    m_blocks.erase(blockIt);
                    std::string oldUuid = moved.uuid;
                    moved.uuid = propVal;
                    moved.properties[propKey] = propVal;

                    if (moved.parentUuid.empty()) {
                        auto& roots = m_pageRoots[note.id];
                        for (auto& r : roots) {
                            if (r == oldUuid) r = propVal;
                        }
                    } else {
                        auto pIt = m_blocks.find(moved.parentUuid);
                        if (pIt != m_blocks.end()) {
                            for (auto& ch : pIt->second.childrenUuids) {
                                if (ch == oldUuid) ch = propVal;
                            }
                        }
                    }

                    for (auto& entry : indentStack) {
                        if (entry.second == oldUuid) entry.second = propVal;
                    }

                    m_blocks[propVal] = std::move(moved);
                    lastBlockUuid = propVal;
                } else {
                    blockIt->second.properties[propKey] = propVal;
                }
                continue;
            }
        }

        int indent = spaces / 2;
        std::string content = trimmed;

        if (content.size() >= 2 &&
            (content[0] == '-' || content[0] == '*' || content[0] == '+') &&
            content[1] == ' ') {
            content = trimCopy(content.substr(2));
        } else if (content.size() == 1 && (content[0] == '-' || content[0] == '*' || content[0] == '+')) {
            content = "";
        }

        BlockNode block;
        std::string seed = note.id + ":" + std::to_string(currentLineIdx) + ":" + content;
        std::string hash = CryptoEngine::sha256(seed).substr(0, 12);
        std::string candidateUuid = hash;
        int disambig = 1;
        while (m_blocks.find(candidateUuid) != m_blocks.end()) {
            candidateUuid = hash + "_" + std::to_string(disambig++);
        }

        block.uuid = candidateUuid;
        block.pageId = note.id;
        block.content = content;
        block.updatedAt = currentTimestamp();

        while (!indentStack.empty() && indentStack.back().first >= indent) {
            indentStack.pop_back();
        }

        if (indentStack.empty()) {
            block.parentUuid.clear();
            block.indent = 0;
            auto& roots = m_pageRoots[note.id];
            block.orderIndex = static_cast<int>(roots.size());
            roots.push_back(block.uuid);
        } else {
            block.parentUuid = indentStack.back().second;
            auto parentIt = m_blocks.find(block.parentUuid);
            if (parentIt != m_blocks.end()) {
                block.indent = parentIt->second.indent + 1;
                block.orderIndex = static_cast<int>(parentIt->second.childrenUuids.size());
                parentIt->second.childrenUuids.push_back(block.uuid);
            } else {
                block.parentUuid.clear();
                block.indent = 0;
                auto& roots = m_pageRoots[note.id];
                block.orderIndex = static_cast<int>(roots.size());
                roots.push_back(block.uuid);
            }
        }

        parseBlockMetadata(block);
        lastBlockUuid = block.uuid;
        indentStack.emplace_back(indent, block.uuid);
        m_blocks[block.uuid] = std::move(block);
    }
}

void OutlinerEngine::parseBlockMetadata(BlockNode& block) {
    block.tags.clear();
    block.pageRefs.clear();
    block.blockRefs.clear();

    const std::string& text = block.content;
    std::string inlineKey;
    std::string inlineVal;
    if (parsePropertyLine(trimCopy(text), inlineKey, inlineVal)) {
        block.properties[inlineKey] = inlineVal;
    }

    size_t pos = 0;
    while ((pos = text.find("[[", pos)) != std::string::npos) {
        size_t end = text.find("]]", pos + 2);
        if (end == std::string::npos) break;
        std::string rawRef = text.substr(pos + 2, end - (pos + 2));
        size_t pipePos = rawRef.find('|');
        if (pipePos != std::string::npos) {
            rawRef = rawRef.substr(0, pipePos);
        }
        pushUnique(block.pageRefs, trimCopy(rawRef));
        pos = end + 2;
    }

    pos = 0;
    while ((pos = text.find("((", pos)) != std::string::npos) {
        size_t end = text.find("))", pos + 2);
        if (end == std::string::npos) break;
        std::string refUuid = trimCopy(text.substr(pos + 2, end - (pos + 2)));
        pushUnique(block.blockRefs, refUuid);
        pos = end + 2;
    }

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '#') {
            if (i > 0 && (std::isalnum(static_cast<unsigned char>(text[i - 1])) || text[i - 1] == '#')) {
                continue;
            }
            size_t j = i + 1;
            while (j < text.size()) {
                unsigned char ch = static_cast<unsigned char>(text[j]);
                if (std::isalnum(ch) || ch == '_' || ch == '-' || ch == '/') {
                    ++j;
                } else {
                    break;
                }
            }
            if (j > i + 1) {
                pushUnique(block.tags, text.substr(i + 1, j - (i + 1)));
                i = j - 1;
            }
        }
    }
}

std::vector<std::string> OutlinerEngine::getRootBlocksForPage(const std::string& pageId) const {
    auto it = m_pageRoots.find(pageId);
    if (it == m_pageRoots.end()) return {};
    return it->second;
}

const BlockNode* OutlinerEngine::getBlock(const std::string& uuid) const {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return nullptr;
    return &it->second;
}

BlockNode* OutlinerEngine::getBlockMutable(const std::string& uuid) {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return nullptr;
    return &it->second;
}

const std::unordered_map<std::string, BlockNode>& OutlinerEngine::getAllBlocks() const {
    return m_blocks;
}

std::vector<std::string>* OutlinerEngine::getSiblingList(const std::string& pageId, const std::string& parentUuid) {
    if (parentUuid.empty()) {
        return &m_pageRoots[pageId];
    }
    auto it = m_blocks.find(parentUuid);
    if (it == m_blocks.end()) return nullptr;
    return &it->second.childrenUuids;
}

void OutlinerEngine::reindexSiblings(const std::string& pageId, const std::string& parentUuid) {
    auto* list = getSiblingList(pageId, parentUuid);
    if (!list) return;
    for (size_t i = 0; i < list->size(); ++i) {
        auto it = m_blocks.find((*list)[i]);
        if (it != m_blocks.end()) {
            it->second.orderIndex = static_cast<int>(i);
        }
    }
}

void OutlinerEngine::updateSubtreeIndent(const std::string& uuid, int newIndent) {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return;
    it->second.indent = newIndent;
    for (const auto& childUuid : it->second.childrenUuids) {
        updateSubtreeIndent(childUuid, newIndent + 1);
    }
}

void OutlinerEngine::removeSubtreeRecursive(const std::string& uuid) {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return;
    std::vector<std::string> children = it->second.childrenUuids;
    m_blocks.erase(it);
    for (const auto& ch : children) {
        removeSubtreeRecursive(ch);
    }
}

std::string OutlinerEngine::createBlock(NoteManager& nm, const std::string& pageId,
                                        const std::string& content, const std::string& parentUuid,
                                        bool firstChild) {
    if (pageId.empty()) return "";

    BlockNode block;
    block.uuid = CryptoEngine::generateUuid();
    block.pageId = pageId;
    block.parentUuid = parentUuid;
    block.content = content;
    block.updatedAt = currentTimestamp();

    if (parentUuid.empty()) {
        block.indent = 0;
        auto& roots = m_pageRoots[pageId];
        if (firstChild) {
            roots.insert(roots.begin(), block.uuid);
        } else {
            roots.push_back(block.uuid);
        }
    } else {
        auto pIt = m_blocks.find(parentUuid);
        if (pIt == m_blocks.end()) return "";
        block.indent = pIt->second.indent + 1;
        if (firstChild) {
            pIt->second.childrenUuids.insert(pIt->second.childrenUuids.begin(), block.uuid);
        } else {
            pIt->second.childrenUuids.push_back(block.uuid);
        }
    }

    parseBlockMetadata(block);
    std::string createdUuid = block.uuid;
    m_blocks[createdUuid] = std::move(block);
    reindexSiblings(pageId, parentUuid);
    syncPageToNote(nm, pageId);
    return createdUuid;
}

bool OutlinerEngine::updateBlockContent(NoteManager& nm, const std::string& uuid, const std::string& newContent) {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return false;
    it->second.content = newContent;
    it->second.updatedAt = currentTimestamp();
    parseBlockMetadata(it->second);
    syncPageToNote(nm, it->second.pageId);
    return true;
}

bool OutlinerEngine::deleteBlock(NoteManager& nm, const std::string& uuid) {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return false;

    std::string pageId = it->second.pageId;
    std::string parentUuid = it->second.parentUuid;

    auto* siblings = getSiblingList(pageId, parentUuid);
    if (siblings) {
        siblings->erase(std::remove(siblings->begin(), siblings->end(), uuid), siblings->end());
    }

    removeSubtreeRecursive(uuid);
    reindexSiblings(pageId, parentUuid);
    syncPageToNote(nm, pageId);
    return true;
}

bool OutlinerEngine::indentBlock(NoteManager& nm, const std::string& uuid) {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return false;

    std::string pageId = it->second.pageId;
    std::string oldParent = it->second.parentUuid;
    auto* siblings = getSiblingList(pageId, oldParent);
    if (!siblings) return false;

    auto posIt = std::find(siblings->begin(), siblings->end(), uuid);
    if (posIt == siblings->end() || posIt == siblings->begin()) return false;

    std::string newParentUuid = *(posIt - 1);
    auto newParentIt = m_blocks.find(newParentUuid);
    if (newParentIt == m_blocks.end()) return false;

    siblings->erase(posIt);
    newParentIt->second.childrenUuids.push_back(uuid);
    newParentIt->second.collapsed = false;

    it->second.parentUuid = newParentUuid;
    it->second.updatedAt = currentTimestamp();
    updateSubtreeIndent(uuid, newParentIt->second.indent + 1);

    reindexSiblings(pageId, oldParent);
    reindexSiblings(pageId, newParentUuid);
    syncPageToNote(nm, pageId);
    return true;
}

bool OutlinerEngine::unindentBlock(NoteManager& nm, const std::string& uuid) {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return false;
    if (it->second.parentUuid.empty()) return false;

    std::string pageId = it->second.pageId;
    std::string oldParentUuid = it->second.parentUuid;
    auto oldParentIt = m_blocks.find(oldParentUuid);
    if (oldParentIt == m_blocks.end()) return false;

    std::string grandParentUuid = oldParentIt->second.parentUuid;
    int newIndent = oldParentIt->second.indent;

    auto& oldSiblings = oldParentIt->second.childrenUuids;
    oldSiblings.erase(std::remove(oldSiblings.begin(), oldSiblings.end(), uuid), oldSiblings.end());

    auto* targetSiblings = getSiblingList(pageId, grandParentUuid);
    if (!targetSiblings) return false;

    auto parentPos = std::find(targetSiblings->begin(), targetSiblings->end(), oldParentUuid);
    if (parentPos != targetSiblings->end()) {
        targetSiblings->insert(parentPos + 1, uuid);
    } else {
        targetSiblings->push_back(uuid);
    }

    it->second.parentUuid = grandParentUuid;
    it->second.updatedAt = currentTimestamp();
    updateSubtreeIndent(uuid, newIndent);

    reindexSiblings(pageId, oldParentUuid);
    reindexSiblings(pageId, grandParentUuid);
    syncPageToNote(nm, pageId);
    return true;
}

bool OutlinerEngine::moveBlockUp(NoteManager& nm, const std::string& uuid) {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return false;

    std::string pageId = it->second.pageId;
    std::string parentUuid = it->second.parentUuid;
    auto* siblings = getSiblingList(pageId, parentUuid);
    if (!siblings) return false;

    auto posIt = std::find(siblings->begin(), siblings->end(), uuid);
    if (posIt == siblings->end() || posIt == siblings->begin()) return false;

    std::iter_swap(posIt, posIt - 1);
    it->second.updatedAt = currentTimestamp();
    reindexSiblings(pageId, parentUuid);
    syncPageToNote(nm, pageId);
    return true;
}

bool OutlinerEngine::moveBlockDown(NoteManager& nm, const std::string& uuid) {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return false;

    std::string pageId = it->second.pageId;
    std::string parentUuid = it->second.parentUuid;
    auto* siblings = getSiblingList(pageId, parentUuid);
    if (!siblings) return false;

    auto posIt = std::find(siblings->begin(), siblings->end(), uuid);
    if (posIt == siblings->end() || posIt + 1 == siblings->end()) return false;

    std::iter_swap(posIt, posIt + 1);
    it->second.updatedAt = currentTimestamp();
    reindexSiblings(pageId, parentUuid);
    syncPageToNote(nm, pageId);
    return true;
}

std::string OutlinerEngine::resolveBlockRefs(const std::string& text, int maxDepth) const {
    if (maxDepth <= 0) return text;

    std::string result = text;

    size_t pos = 0;
    while ((pos = result.find("{{embed ((", pos)) != std::string::npos) {
        size_t end = result.find("))}}", pos + 10);
        if (end == std::string::npos) break;
        std::string refUuid = trimCopy(result.substr(pos + 10, end - (pos + 10)));
        const BlockNode* target = getBlock(refUuid);
        if (target) {
            std::string expanded = resolveBlockRefs(target->content, maxDepth - 1);
            result.replace(pos, (end + 4) - pos, expanded);
            pos += expanded.size();
        } else {
            pos = end + 4;
        }
    }

    pos = 0;
    while ((pos = result.find("((", pos)) != std::string::npos) {
        size_t end = result.find("))", pos + 2);
        if (end == std::string::npos) break;
        std::string refUuid = trimCopy(result.substr(pos + 2, end - (pos + 2)));
        const BlockNode* target = getBlock(refUuid);
        if (target) {
            std::string expanded = resolveBlockRefs(target->content, maxDepth - 1);
            result.replace(pos, (end + 2) - pos, expanded);
            pos += expanded.size();
        } else {
            pos = end + 2;
        }
    }

    return result;
}

std::vector<std::string> OutlinerEngine::getBreadcrumbs(const std::string& uuid) const {
    std::vector<std::string> chain;
    std::string current = uuid;
    int guard = 0;
    while (!current.empty() && guard < 128) {
        auto it = m_blocks.find(current);
        if (it == m_blocks.end()) break;
        chain.push_back(current);
        current = it->second.parentUuid;
        ++guard;
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

std::vector<const BlockNode*> OutlinerEngine::searchBlocks(const std::string& query) const {
    std::vector<const BlockNode*> results;
    std::string lowerQuery = toLowerCopy(trimCopy(query));

    for (const auto& [uuid, block] : m_blocks) {
        if (lowerQuery.empty()) {
            results.push_back(&block);
            continue;
        }
        bool matched = false;
        if (toLowerCopy(block.content).find(lowerQuery) != std::string::npos ||
            toLowerCopy(block.pageId).find(lowerQuery) != std::string::npos ||
            toLowerCopy(block.uuid).find(lowerQuery) != std::string::npos) {
            matched = true;
        }
        if (!matched) {
            for (const auto& tag : block.tags) {
                if (toLowerCopy(tag).find(lowerQuery) != std::string::npos) {
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) {
            for (const auto& [k, v] : block.properties) {
                if (toLowerCopy(k).find(lowerQuery) != std::string::npos ||
                    toLowerCopy(v).find(lowerQuery) != std::string::npos) {
                    matched = true;
                    break;
                }
            }
        }
        if (matched) {
            results.push_back(&block);
        }
    }

    std::sort(results.begin(), results.end(), [](const BlockNode* a, const BlockNode* b) {
        if (a->pageId != b->pageId) return a->pageId < b->pageId;
        if (a->indent != b->indent) return a->indent < b->indent;
        return a->orderIndex < b->orderIndex;
    });
    return results;
}

void OutlinerEngine::serializeBlockRecursive(const std::string& uuid, std::ostringstream& out) const {
    auto it = m_blocks.find(uuid);
    if (it == m_blocks.end()) return;
    const BlockNode& block = it->second;

    std::string prefix(static_cast<size_t>(block.indent * 2), ' ');
    bool isRootHeading = (block.indent == 0 && !block.content.empty() && block.content[0] == '#');
    if (isRootHeading) {
        out << block.content << "\n";
    } else {
        out << prefix << "- " << block.content << "\n";
    }

    std::string inlineKey;
    std::string inlineVal;
    bool contentIsProp = parsePropertyLine(trimCopy(block.content), inlineKey, inlineVal);
    for (const auto& [k, v] : block.properties) {
        if (contentIsProp && k == inlineKey) continue;
        out << prefix << "  " << k << ":: " << v << "\n";
    }

    for (const auto& childUuid : block.childrenUuids) {
        serializeBlockRecursive(childUuid, out);
    }
}

std::string OutlinerEngine::serializePageMarkdown(const std::string& pageId) const {
    auto it = m_pageRoots.find(pageId);
    if (it == m_pageRoots.end()) return "";

    std::ostringstream out;
    for (const auto& rootUuid : it->second) {
        serializeBlockRecursive(rootUuid, out);
    }
    return out.str();
}

bool OutlinerEngine::syncPageToNote(NoteManager& nm, const std::string& pageId) {
    Note* note = nm.getNoteById(pageId);
    if (!note) return false;

    note->content = serializePageMarkdown(pageId);
    note->parseLinks();
    note->parseTitle();
    note->dirty = true;
    note->saveToFile();
    return true;
}

}
