#include "whiteboard_engine.h"
#include "crypto_engine.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace nodepanda {

namespace {

std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) >= 0x20) {
                    out.push_back(c);
                }
                break;
        }
    }
    return out;
}

class SimpleJsonReader {
public:
    explicit SimpleJsonReader(const std::string& src) : m_src(src), m_pos(0) {}

    void skipWs() {
        while (m_pos < m_src.size() && std::isspace(static_cast<unsigned char>(m_src[m_pos]))) {
            ++m_pos;
        }
    }

    bool consume(char expected) {
        skipWs();
        if (m_pos < m_src.size() && m_src[m_pos] == expected) {
            ++m_pos;
            return true;
        }
        return false;
    }

    char peek() {
        skipWs();
        return m_pos < m_src.size() ? m_src[m_pos] : '\0';
    }

    bool readString(std::string& out) {
        skipWs();
        if (m_pos >= m_src.size() || m_src[m_pos] != '\"') return false;
        ++m_pos;
        out.clear();
        while (m_pos < m_src.size()) {
            char c = m_src[m_pos++];
            if (c == '\"') return true;
            if (c == '\\' && m_pos < m_src.size()) {
                char esc = m_src[m_pos++];
                switch (esc) {
                    case '\"': out.push_back('\"'); break;
                    case '\\': out.push_back('\\'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default: out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        return false;
    }

    bool readNumber(double& out) {
        skipWs();
        size_t start = m_pos;
        if (m_pos < m_src.size() && (m_src[m_pos] == '-' || m_src[m_pos] == '+')) ++m_pos;
        bool hasDigit = false;
        while (m_pos < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) {
            hasDigit = true;
            ++m_pos;
        }
        if (m_pos < m_src.size() && m_src[m_pos] == '.') {
            ++m_pos;
            while (m_pos < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) {
                hasDigit = true;
                ++m_pos;
            }
        }
        if (m_pos < m_src.size() && (m_src[m_pos] == 'e' || m_src[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_src.size() && (m_src[m_pos] == '-' || m_src[m_pos] == '+')) ++m_pos;
            while (m_pos < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) {
                ++m_pos;
            }
        }
        if (!hasDigit) return false;
        try {
            out = std::stod(m_src.substr(start, m_pos - start));
            return true;
        } catch (...) {
            return false;
        }
    }

    bool skipValue() {
        char c = peek();
        if (c == '\"') {
            std::string dummy;
            return readString(dummy);
        }
        if (c == '{') {
            consume('{');
            if (consume('}')) return true;
            while (m_pos < m_src.size()) {
                std::string k;
                if (!readString(k) || !consume(':') || !skipValue()) return false;
                if (consume('}')) return true;
                if (!consume(',')) return false;
            }
            return false;
        }
        if (c == '[') {
            consume('[');
            if (consume(']')) return true;
            while (m_pos < m_src.size()) {
                if (!skipValue()) return false;
                if (consume(']')) return true;
                if (!consume(',')) return false;
            }
            return false;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            double d = 0.0;
            return readNumber(d);
        }
        while (m_pos < m_src.size() && std::isalpha(static_cast<unsigned char>(m_src[m_pos]))) {
            ++m_pos;
        }
        return true;
    }

private:
    const std::string& m_src;
    size_t m_pos;
};

}

std::string WhiteboardEngine::kindToString(WhiteboardNodeKind k) {
    switch (k) {
        case WhiteboardNodeKind::NoteCard: return "NoteCard";
        case WhiteboardNodeKind::BlockCard: return "BlockCard";
        case WhiteboardNodeKind::TextBox: return "TextBox";
        case WhiteboardNodeKind::StickyNote: return "StickyNote";
        case WhiteboardNodeKind::Frame: return "Frame";
    }
    return "TextBox";
}

WhiteboardNodeKind WhiteboardEngine::stringToKind(const std::string& s) {
    if (s == "NoteCard") return WhiteboardNodeKind::NoteCard;
    if (s == "BlockCard") return WhiteboardNodeKind::BlockCard;
    if (s == "StickyNote") return WhiteboardNodeKind::StickyNote;
    if (s == "Frame") return WhiteboardNodeKind::Frame;
    return WhiteboardNodeKind::TextBox;
}

void WhiteboardEngine::init(const std::filesystem::path& dir) {
    m_dir = dir;
    if (!m_dir.empty() && !std::filesystem::exists(m_dir)) {
        std::filesystem::create_directories(m_dir);
    }
    loadAll();
    if (m_boards.empty()) {
        Whiteboard& defBoard = createBoard("Principal");
        saveBoard(defBoard.id);
    } else if (m_activeBoardId.empty()) {
        m_activeBoardId = listBoards().front();
    }
}

std::vector<std::string> WhiteboardEngine::listBoards() const {
    std::vector<std::string> ids;
    ids.reserve(m_boards.size());
    for (const auto& [id, board] : m_boards) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

Whiteboard& WhiteboardEngine::createBoard(const std::string& name) {
    Whiteboard wb;
    wb.id = CryptoEngine::randomHex(8);
    wb.name = name.empty() ? "Pizarra" : name;
    std::string id = wb.id;
    m_boards[id] = std::move(wb);
    m_activeBoardId = id;
    return m_boards[id];
}

Whiteboard* WhiteboardEngine::getBoard(const std::string& id) {
    auto it = m_boards.find(id);
    if (it == m_boards.end()) return nullptr;
    return &it->second;
}

Whiteboard* WhiteboardEngine::activeBoard() {
    if (m_activeBoardId.empty() && !m_boards.empty()) {
        m_activeBoardId = m_boards.begin()->first;
    }
    return getBoard(m_activeBoardId);
}

void WhiteboardEngine::setActiveBoard(const std::string& id) {
    if (m_boards.find(id) != m_boards.end()) {
        m_activeBoardId = id;
    }
}

std::string WhiteboardEngine::addNode(WhiteboardNodeKind kind, const std::string& title,
                                      const std::string& body, float x, float y,
                                      const std::string& linkedNoteId) {
    Whiteboard* wb = activeBoard();
    if (!wb) {
        wb = &createBoard("Principal");
    }

    WhiteboardNode node;
    node.id = CryptoEngine::randomHex(8);
    node.kind = kind;
    node.title = title;
    node.body = body;
    node.linkedNoteId = linkedNoteId;
    node.x = x;
    node.y = y;

    switch (kind) {
        case WhiteboardNodeKind::Frame:
            node.w = 420.0f;
            node.h = 280.0f;
            node.colorIndex = 5;
            break;
        case WhiteboardNodeKind::StickyNote:
            node.w = 210.0f;
            node.h = 160.0f;
            node.colorIndex = 2;
            break;
        case WhiteboardNodeKind::NoteCard:
            node.w = 240.0f;
            node.h = 140.0f;
            node.colorIndex = 0;
            break;
        case WhiteboardNodeKind::BlockCard:
            node.w = 230.0f;
            node.h = 120.0f;
            node.colorIndex = 1;
            break;
        case WhiteboardNodeKind::TextBox:
            node.w = 220.0f;
            node.h = 120.0f;
            node.colorIndex = 4;
            break;
    }

    std::string id = node.id;
    wb->nodes.push_back(std::move(node));
    return id;
}

bool WhiteboardEngine::removeNode(const std::string& nodeId) {
    Whiteboard* wb = activeBoard();
    if (!wb) return false;

    auto oldSize = wb->nodes.size();
    wb->nodes.erase(
        std::remove_if(wb->nodes.begin(), wb->nodes.end(), [&](const WhiteboardNode& n) {
            return n.id == nodeId;
        }),
        wb->nodes.end());

    if (wb->nodes.size() == oldSize) return false;

    wb->edges.erase(
        std::remove_if(wb->edges.begin(), wb->edges.end(), [&](const WhiteboardEdge& e) {
            return e.fromId == nodeId || e.toId == nodeId;
        }),
        wb->edges.end());

    return true;
}

std::string WhiteboardEngine::addEdge(const std::string& fromId, const std::string& toId,
                                      const std::string& label) {
    Whiteboard* wb = activeBoard();
    if (!wb || fromId.empty() || toId.empty() || fromId == toId) return "";

    bool hasFrom = false;
    bool hasTo = false;
    for (const auto& n : wb->nodes) {
        if (n.id == fromId) hasFrom = true;
        if (n.id == toId) hasTo = true;
    }
    if (!hasFrom || !hasTo) return "";

    for (const auto& e : wb->edges) {
        if (e.fromId == fromId && e.toId == toId && e.label == label) {
            return e.id;
        }
    }

    WhiteboardEdge edge;
    edge.id = CryptoEngine::randomHex(8);
    edge.fromId = fromId;
    edge.toId = toId;
    edge.label = label;
    std::string id = edge.id;
    wb->edges.push_back(std::move(edge));
    return id;
}

bool WhiteboardEngine::removeEdge(const std::string& edgeId) {
    Whiteboard* wb = activeBoard();
    if (!wb) return false;

    auto oldSize = wb->edges.size();
    wb->edges.erase(
        std::remove_if(wb->edges.begin(), wb->edges.end(), [&](const WhiteboardEdge& e) {
            return e.id == edgeId;
        }),
        wb->edges.end());
    return wb->edges.size() != oldSize;
}

bool WhiteboardEngine::saveBoard(const std::string& id) const {
    auto it = m_boards.find(id);
    if (it == m_boards.end() || m_dir.empty()) return false;

    if (!std::filesystem::exists(m_dir)) {
        std::filesystem::create_directories(m_dir);
    }

    const Whiteboard& wb = it->second;
    std::filesystem::path filePath = m_dir / (wb.id + ".npboard.json");
    std::ofstream ofs(filePath, std::ios::binary);
    if (!ofs.is_open()) return false;

    ofs << "{\n";
    ofs << "  \"id\": \"" << jsonEscape(wb.id) << "\",\n";
    ofs << "  \"name\": \"" << jsonEscape(wb.name) << "\",\n";
    ofs << "  \"nodes\": [\n";
    for (size_t i = 0; i < wb.nodes.size(); ++i) {
        const auto& n = wb.nodes[i];
        ofs << "    {\n";
        ofs << "      \"id\": \"" << jsonEscape(n.id) << "\",\n";
        ofs << "      \"kind\": \"" << kindToString(n.kind) << "\",\n";
        ofs << "      \"title\": \"" << jsonEscape(n.title) << "\",\n";
        ofs << "      \"body\": \"" << jsonEscape(n.body) << "\",\n";
        ofs << "      \"linkedNoteId\": \"" << jsonEscape(n.linkedNoteId) << "\",\n";
        ofs << "      \"x\": " << n.x << ",\n";
        ofs << "      \"y\": " << n.y << ",\n";
        ofs << "      \"w\": " << n.w << ",\n";
        ofs << "      \"h\": " << n.h << ",\n";
        ofs << "      \"colorIndex\": " << n.colorIndex << "\n";
        ofs << "    }" << (i + 1 < wb.nodes.size() ? "," : "") << "\n";
    }
    ofs << "  ],\n";
    ofs << "  \"edges\": [\n";
    for (size_t i = 0; i < wb.edges.size(); ++i) {
        const auto& e = wb.edges[i];
        ofs << "    {\n";
        ofs << "      \"id\": \"" << jsonEscape(e.id) << "\",\n";
        ofs << "      \"fromId\": \"" << jsonEscape(e.fromId) << "\",\n";
        ofs << "      \"toId\": \"" << jsonEscape(e.toId) << "\",\n";
        ofs << "      \"label\": \"" << jsonEscape(e.label) << "\"\n";
        ofs << "    }" << (i + 1 < wb.edges.size() ? "," : "") << "\n";
    }
    ofs << "  ]\n";
    ofs << "}\n";
    return true;
}

bool WhiteboardEngine::loadBoardFile(const std::filesystem::path& filePath) {
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open()) return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();

    SimpleJsonReader reader(content);
    if (!reader.consume('{')) return false;

    Whiteboard wb;
    if (!reader.consume('}')) {
        while (true) {
            std::string key;
            if (!reader.readString(key) || !reader.consume(':')) return false;

            if (key == "id") {
                if (!reader.readString(wb.id)) return false;
            } else if (key == "name") {
                if (!reader.readString(wb.name)) return false;
            } else if (key == "nodes") {
                if (!reader.consume('[')) return false;
                if (!reader.consume(']')) {
                    while (true) {
                        if (!reader.consume('{')) return false;
                        WhiteboardNode node;
                        if (!reader.consume('}')) {
                            while (true) {
                                std::string nk;
                                if (!reader.readString(nk) || !reader.consume(':')) return false;
                                if (nk == "id") {
                                    if (!reader.readString(node.id)) return false;
                                } else if (nk == "kind") {
                                    std::string ks;
                                    if (!reader.readString(ks)) return false;
                                    node.kind = stringToKind(ks);
                                } else if (nk == "title") {
                                    if (!reader.readString(node.title)) return false;
                                } else if (nk == "body") {
                                    if (!reader.readString(node.body)) return false;
                                } else if (nk == "linkedNoteId") {
                                    if (!reader.readString(node.linkedNoteId)) return false;
                                } else if (nk == "x") {
                                    double v = 0.0;
                                    if (!reader.readNumber(v)) return false;
                                    node.x = static_cast<float>(v);
                                } else if (nk == "y") {
                                    double v = 0.0;
                                    if (!reader.readNumber(v)) return false;
                                    node.y = static_cast<float>(v);
                                } else if (nk == "w") {
                                    double v = 220.0;
                                    if (!reader.readNumber(v)) return false;
                                    node.w = static_cast<float>(v);
                                } else if (nk == "h") {
                                    double v = 120.0;
                                    if (!reader.readNumber(v)) return false;
                                    node.h = static_cast<float>(v);
                                } else if (nk == "colorIndex") {
                                    double v = 0.0;
                                    if (!reader.readNumber(v)) return false;
                                    node.colorIndex = static_cast<int>(v);
                                } else {
                                    if (!reader.skipValue()) return false;
                                }
                                if (reader.consume('}')) break;
                                if (!reader.consume(',')) return false;
                            }
                        }
                        if (!node.id.empty()) {
                            wb.nodes.push_back(std::move(node));
                        }
                        if (reader.consume(']')) break;
                        if (!reader.consume(',')) return false;
                    }
                }
            } else if (key == "edges") {
                if (!reader.consume('[')) return false;
                if (!reader.consume(']')) {
                    while (true) {
                        if (!reader.consume('{')) return false;
                        WhiteboardEdge edge;
                        if (!reader.consume('}')) {
                            while (true) {
                                std::string ek;
                                if (!reader.readString(ek) || !reader.consume(':')) return false;
                                if (ek == "id") {
                                    if (!reader.readString(edge.id)) return false;
                                } else if (ek == "fromId") {
                                    if (!reader.readString(edge.fromId)) return false;
                                } else if (ek == "toId") {
                                    if (!reader.readString(edge.toId)) return false;
                                } else if (ek == "label") {
                                    if (!reader.readString(edge.label)) return false;
                                } else {
                                    if (!reader.skipValue()) return false;
                                }
                                if (reader.consume('}')) break;
                                if (!reader.consume(',')) return false;
                            }
                        }
                        if (!edge.id.empty() && !edge.fromId.empty() && !edge.toId.empty()) {
                            wb.edges.push_back(std::move(edge));
                        }
                        if (reader.consume(']')) break;
                        if (!reader.consume(',')) return false;
                    }
                }
            } else {
                if (!reader.skipValue()) return false;
            }

            if (reader.consume('}')) break;
            if (!reader.consume(',')) return false;
        }
    }

    if (wb.id.empty()) {
        wb.id = filePath.stem().string();
    }
    if (wb.name.empty()) {
        wb.name = wb.id;
    }
    std::string id = wb.id;
    m_boards[id] = std::move(wb);
    return true;
}

void WhiteboardEngine::loadAll() {
    m_boards.clear();
    if (m_dir.empty() || !std::filesystem::exists(m_dir)) return;

    for (const auto& entry : std::filesystem::directory_iterator(m_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string fname = entry.path().filename().string();
        if (fname.size() >= 13 && fname.substr(fname.size() - 13) == ".npboard.json") {
            loadBoardFile(entry.path());
        }
    }

    if (!m_boards.empty() && m_boards.find(m_activeBoardId) == m_boards.end()) {
        m_activeBoardId = listBoards().front();
    }
}

}
