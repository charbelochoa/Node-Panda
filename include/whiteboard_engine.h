#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace nodepanda {

enum class WhiteboardNodeKind {
    NoteCard,
    BlockCard,
    TextBox,
    StickyNote,
    Frame
};

struct WhiteboardNode {
    std::string id;
    WhiteboardNodeKind kind = WhiteboardNodeKind::TextBox;
    std::string title;
    std::string body;
    std::string linkedNoteId;
    float x = 0.0f;
    float y = 0.0f;
    float w = 220.0f;
    float h = 120.0f;
    int colorIndex = 0;
};

struct WhiteboardEdge {
    std::string id;
    std::string fromId;
    std::string toId;
    std::string label;
};

struct Whiteboard {
    std::string id;
    std::string name;
    std::vector<WhiteboardNode> nodes;
    std::vector<WhiteboardEdge> edges;
};

class WhiteboardEngine {
public:
    void init(const std::filesystem::path& dir);

    std::vector<std::string> listBoards() const;
    Whiteboard& createBoard(const std::string& name);
    Whiteboard* getBoard(const std::string& id);
    Whiteboard* activeBoard();
    void setActiveBoard(const std::string& id);

    std::string addNode(WhiteboardNodeKind kind, const std::string& title, const std::string& body,
                        float x, float y, const std::string& linkedNoteId = "");
    bool removeNode(const std::string& nodeId);
    std::string addEdge(const std::string& fromId, const std::string& toId, const std::string& label = "");
    bool removeEdge(const std::string& edgeId);

    bool saveBoard(const std::string& id) const;
    void loadAll();

private:
    static std::string kindToString(WhiteboardNodeKind k);
    static WhiteboardNodeKind stringToKind(const std::string& s);
    bool loadBoardFile(const std::filesystem::path& filePath);

    std::filesystem::path m_dir;
    std::unordered_map<std::string, Whiteboard> m_boards;
    std::string m_activeBoardId;
};

}
