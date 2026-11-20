#include "math_renderer.h"
#include "ui_theme.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <unordered_map>

namespace nodepanda {

namespace {

struct SymbolEntry {
    const char* cmd;
    const char* utf8;
    const char* ascii;
    bool padBinary;
    bool bigOp;
};

const std::vector<SymbolEntry>& symbolTable() {
    static const std::vector<SymbolEntry> table = {
        {"alpha", "\xCE\xB1", "alpha", false, false},
        {"beta", "\xCE\xB2", "beta", false, false},
        {"gamma", "\xCE\xB3", "gamma", false, false},
        {"delta", "\xCE\xB4", "delta", false, false},
        {"epsilon", "\xCE\xB5", "eps", false, false},
        {"varepsilon", "\xCE\xB5", "eps", false, false},
        {"zeta", "\xCE\xB6", "zeta", false, false},
        {"eta", "\xCE\xB7", "eta", false, false},
        {"theta", "\xCE\xB8", "theta", false, false},
        {"vartheta", "\xCE\xB8", "theta", false, false},
        {"iota", "\xCE\xB9", "iota", false, false},
        {"kappa", "\xCE\xBA", "kappa", false, false},
        {"lambda", "\xCE\xBB", "lambda", false, false},
        {"mu", "\xCE\xBC", "mu", false, false},
        {"nu", "\xCE\xBD", "nu", false, false},
        {"xi", "\xCE\xBE", "xi", false, false},
        {"omicron", "\xCE\xBF", "o", false, false},
        {"pi", "\xCF\x80", "pi", false, false},
        {"varpi", "\xCF\x80", "pi", false, false},
        {"rho", "\xCF\x81", "rho", false, false},
        {"varrho", "\xCF\x81", "rho", false, false},
        {"sigma", "\xCF\x83", "sigma", false, false},
        {"varsigma", "\xCF\x82", "sigma", false, false},
        {"tau", "\xCF\x84", "tau", false, false},
        {"upsilon", "\xCF\x85", "upsilon", false, false},
        {"phi", "\xCF\x86", "phi", false, false},
        {"varphi", "\xCF\x86", "phi", false, false},
        {"chi", "\xCF\x87", "chi", false, false},
        {"psi", "\xCF\x88", "psi", false, false},
        {"omega", "\xCF\x89", "omega", false, false},
        {"Gamma", "\xCE\x93", "Gamma", false, false},
        {"Delta", "\xCE\x94", "Delta", false, false},
        {"Theta", "\xCE\x98", "Theta", false, false},
        {"Lambda", "\xCE\x9B", "Lambda", false, false},
        {"Xi", "\xCE\x9E", "Xi", false, false},
        {"Pi", "\xCE\xA0", "Pi", false, false},
        {"Sigma", "\xCE\xA3", "Sigma", false, false},
        {"Upsilon", "\xCE\xA5", "Upsilon", false, false},
        {"Phi", "\xCE\xA6", "Phi", false, false},
        {"Psi", "\xCE\xA8", "Psi", false, false},
        {"Omega", "\xCE\xA9", "Omega", false, false},
        {"sum", "\xE2\x88\x91", "SUM", false, true},
        {"int", "\xE2\x88\xAB", "INT", false, true},
        {"iint", "\xE2\x88\xAC", "IINT", false, true},
        {"oint", "\xE2\x88\xAE", "OINT", false, true},
        {"prod", "\xE2\x88\x8F", "PROD", false, true},
        {"infty", "\xE2\x88\x9E", "inf", false, false},
        {"partial", "\xE2\x88\x82", "d", false, false},
        {"nabla", "\xE2\x88\x87", "nabla", false, false},
        {"pm", "\xC2\xB1", "+/-", true, false},
        {"mp", "\xE2\x88\x93", "-/+", true, false},
        {"times", "\xC3\x97", "x", true, false},
        {"div", "\xC3\xB7", "/", true, false},
        {"cdot", "\xC2\xB7", "*", true, false},
        {"circ", "\xE2\x88\x98", "o", true, false},
        {"le", "\xE2\x89\xA4", "<=", true, false},
        {"leq", "\xE2\x89\xA4", "<=", true, false},
        {"ge", "\xE2\x89\xA5", ">=", true, false},
        {"geq", "\xE2\x89\xA5", ">=", true, false},
        {"neq", "\xE2\x89\xA0", "!=", true, false},
        {"ne", "\xE2\x89\xA0", "!=", true, false},
        {"approx", "\xE2\x89\x88", "~=", true, false},
        {"equiv", "\xE2\x89\xA1", "===", true, false},
        {"sim", "\xE2\x88\xBC", "~", true, false},
        {"propto", "\xE2\x88\x9D", "~", true, false},
        {"to", "\xE2\x86\x92", "->", true, false},
        {"rightarrow", "\xE2\x86\x92", "->", true, false},
        {"leftarrow", "\xE2\x86\x90", "<-", true, false},
        {"leftrightarrow", "\xE2\x86\x94", "<->", true, false},
        {"Rightarrow", "\xE2\x87\x92", "=>", true, false},
        {"Leftarrow", "\xE2\x87\x90", "<=", true, false},
        {"Leftrightarrow", "\xE2\x87\x94", "<=>", true, false},
        {"forall", "\xE2\x88\x80", "forall ", false, false},
        {"exists", "\xE2\x88\x83", "exists ", false, false},
        {"in", "\xE2\x88\x88", "in", true, false},
        {"notin", "\xE2\x88\x89", "notin", true, false},
        {"subset", "\xE2\x8A\x82", "subset", true, false},
        {"subseteq", "\xE2\x8A\x86", "subseteq", true, false},
        {"cup", "\xE2\x88\xAA", "U", true, false},
        {"cap", "\xE2\x88\xA9", "n", true, false},
        {"emptyset", "\xE2\x88\x85", "{}", false, false},
        {"land", "\xE2\x88\xA7", "and", true, false},
        {"lor", "\xE2\x88\xA8", "or", true, false},
        {"neg", "\xC2\xAC", "!", false, false},
        {"ldots", "...", "...", false, false},
        {"cdots", "...", "...", false, false},
        {"dots", "...", "...", false, false},
        {"quad", "  ", "  ", false, false},
        {"qquad", "    ", "    ", false, false}
    };
    return table;
}

const std::unordered_map<std::string, const SymbolEntry*>& symbolByCommand() {
    static std::unordered_map<std::string, const SymbolEntry*> map = []() {
        std::unordered_map<std::string, const SymbolEntry*> m;
        for (const auto& entry : symbolTable()) {
            m[entry.cmd] = &entry;
        }
        return m;
    }();
    return map;
}

const std::unordered_map<std::string, const SymbolEntry*>& symbolByUtf8() {
    static std::unordered_map<std::string, const SymbolEntry*> map = []() {
        std::unordered_map<std::string, const SymbolEntry*> m;
        for (const auto& entry : symbolTable()) {
            if (m.find(entry.utf8) == m.end()) {
                m[entry.utf8] = &entry;
            }
        }
        return m;
    }();
    return map;
}

bool isStandardFunction(const std::string& cmd) {
    static const std::unordered_map<std::string, bool> funcs = {
        {"sin", true}, {"cos", true}, {"tan", true}, {"cot", true},
        {"sec", true}, {"csc", true}, {"arcsin", true}, {"arccos", true},
        {"arctan", true}, {"sinh", true}, {"cosh", true}, {"tanh", true},
        {"log", true}, {"ln", true}, {"exp", true}, {"lim", true},
        {"max", true}, {"min", true}, {"sup", true}, {"inf", true},
        {"arg", true}, {"det", true}, {"dim", true}, {"ker", true},
        {"gcd", true}, {"Pr", true}
    };
    return funcs.find(cmd) != funcs.end();
}

bool isWrapperCommand(const std::string& cmd) {
    return cmd == "text" || cmd == "mathrm" || cmd == "mathbf" ||
           cmd == "mathit" || cmd == "mathcal" || cmd == "mathbb" ||
           cmd == "mathsf" || cmd == "mathtt" || cmd == "operatorname" ||
           cmd == "boldsymbol" || cmd == "vec" || cmd == "hat" ||
           cmd == "bar" || cmd == "tilde" || cmd == "overline";
}

std::string trimString(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool decodeUtf8At(const std::string& s, size_t pos, unsigned int& cp, size_t& len) {
    if (pos >= s.size()) return false;
    unsigned char b0 = static_cast<unsigned char>(s[pos]);
    if (b0 < 0x80) {
        cp = b0;
        len = 1;
        return true;
    }
    if ((b0 & 0xE0) == 0xC0 && pos + 1 < s.size()) {
        unsigned char b1 = static_cast<unsigned char>(s[pos + 1]);
        cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
        len = 2;
        return true;
    }
    if ((b0 & 0xF0) == 0xE0 && pos + 2 < s.size()) {
        unsigned char b1 = static_cast<unsigned char>(s[pos + 1]);
        unsigned char b2 = static_cast<unsigned char>(s[pos + 2]);
        cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        len = 3;
        return true;
    }
    if ((b0 & 0xF8) == 0xF0 && pos + 3 < s.size()) {
        unsigned char b1 = static_cast<unsigned char>(s[pos + 1]);
        unsigned char b2 = static_cast<unsigned char>(s[pos + 2]);
        unsigned char b3 = static_cast<unsigned char>(s[pos + 3]);
        cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        len = 4;
        return true;
    }
    cp = b0;
    len = 1;
    return true;
}

bool isBigOperatorText(const std::string& txt) {
    std::string t = trimString(txt);
    return t == "\xE2\x88\x91" || t == "\xE2\x88\xAB" ||
           t == "\xE2\x88\xAC" || t == "\xE2\x88\xAE" ||
           t == "\xE2\x88\x8F";
}

class LatexParser {
public:
    explicit LatexParser(const std::string& src) : m_src(src) {}

    MathNode parseRoot() {
        MathNode root;
        root.kind = MathNodeKind::Sequence;
        root.children = parseSequence('\0');
        return root;
    }

private:
    std::string m_src;
    size_t m_pos = 0;

    bool eof() const { return m_pos >= m_src.size(); }
    char peek() const { return eof() ? '\0' : m_src[m_pos]; }
    char get() { return eof() ? '\0' : m_src[m_pos++]; }

    void skipSpaces() {
        while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
            ++m_pos;
        }
    }

    std::string readCommandName() {
        std::string cmd;
        if (eof()) return cmd;
        if (!std::isalpha(static_cast<unsigned char>(peek()))) {
            cmd.push_back(get());
            return cmd;
        }
        while (!eof() && std::isalpha(static_cast<unsigned char>(peek()))) {
            cmd.push_back(get());
        }
        return cmd;
    }

    std::vector<MathNode> parseArgNodes() {
        skipSpaces();
        if (eof()) return {};
        if (peek() == '{') {
            get();
            return parseSequence('}');
        }
        MathNode atom = parseAtom();
        if (atom.kind == MathNodeKind::Text && atom.text.empty()) {
            return {};
        }
        return {atom};
    }

    MathNode parseAtom() {
        if (eof()) {
            MathNode empty;
            empty.kind = MathNodeKind::Text;
            return empty;
        }

        char c = peek();
        if (c == '{') {
            get();
            MathNode grp;
            grp.kind = MathNodeKind::Group;
            grp.children = parseSequence('}');
            return grp;
        }

        if (c == '\\') {
            get();
            std::string cmd = readCommandName();
            if (cmd == "frac" || cmd == "dfrac" || cmd == "tfrac") {
                MathNode frac;
                frac.kind = MathNodeKind::Fraction;
                frac.num = parseArgNodes();
                frac.den = parseArgNodes();
                return frac;
            }
            if (cmd == "sqrt") {
                MathNode sq;
                sq.kind = MathNodeKind::Sqrt;
                skipSpaces();
                if (!eof() && peek() == '[') {
                    get();
                    sq.sup = parseSequence(']');
                }
                sq.children = parseArgNodes();
                return sq;
            }
            if (cmd == "left" || cmd == "right" || cmd == "big" || cmd == "Big" ||
                cmd == "bigg" || cmd == "Bigg") {
                skipSpaces();
                MathNode delim;
                delim.kind = MathNodeKind::Text;
                if (!eof()) {
                    char d = get();
                    if (d == '.') {
                        delim.text = "";
                    } else if (d == '\\' && !eof()) {
                        char esc = get();
                        delim.text = std::string(1, esc);
                    } else {
                        delim.text = std::string(1, d);
                    }
                }
                return delim;
            }
            if (isWrapperCommand(cmd)) {
                MathNode grp;
                grp.kind = MathNodeKind::Group;
                grp.children = parseArgNodes();
                return grp;
            }
            if (cmd == "," || cmd == ";" || cmd == ":" || cmd == " ") {
                MathNode sp;
                sp.kind = MathNodeKind::Text;
                sp.text = " ";
                return sp;
            }
            if (cmd == "!") {
                MathNode sp;
                sp.kind = MathNodeKind::Text;
                return sp;
            }
            if (cmd == "\\" || cmd == "newline") {
                MathNode sp;
                sp.kind = MathNodeKind::Text;
                sp.text = "  ";
                return sp;
            }
            if (cmd.size() == 1 && !std::isalpha(static_cast<unsigned char>(cmd[0]))) {
                MathNode lit;
                lit.kind = MathNodeKind::Text;
                lit.text = cmd;
                return lit;
            }

            const auto& symMap = symbolByCommand();
            auto it = symMap.find(cmd);
            if (it != symMap.end()) {
                MathNode sym;
                sym.kind = MathNodeKind::Text;
                if (it->second->padBinary) {
                    sym.text = std::string(" ") + it->second->utf8 + " ";
                } else {
                    sym.text = it->second->utf8;
                }
                return sym;
            }

            if (isStandardFunction(cmd)) {
                MathNode fn;
                fn.kind = MathNodeKind::Text;
                fn.text = cmd;
                return fn;
            }

            MathNode unknown;
            unknown.kind = MathNodeKind::Text;
            unknown.text = cmd;
            return unknown;
        }

        if (std::isspace(static_cast<unsigned char>(c))) {
            while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
                ++m_pos;
            }
            MathNode sp;
            sp.kind = MathNodeKind::Text;
            sp.text = " ";
            return sp;
        }

        MathNode ch;
        ch.kind = MathNodeKind::Text;
        unsigned char b0 = static_cast<unsigned char>(c);
        size_t charLen = 1;
        if ((b0 & 0xE0) == 0xC0) charLen = 2;
        else if ((b0 & 0xF0) == 0xE0) charLen = 3;
        else if ((b0 & 0xF8) == 0xF0) charLen = 4;
        if (m_pos + charLen > m_src.size()) charLen = m_src.size() - m_pos;
        ch.text = m_src.substr(m_pos, charLen);
        m_pos += charLen;
        return ch;
    }

    std::vector<MathNode> parseSequence(char stopChar) {
        std::vector<MathNode> out;
        while (!eof()) {
            char c = peek();
            if (stopChar != '\0' && c == stopChar) {
                get();
                break;
            }
            if (c == '}' && stopChar == '\0') {
                get();
                continue;
            }
            if (c == '^' || c == '_') {
                MathNode emptyBase;
                emptyBase.kind = MathNodeKind::Text;
                MathNode sc;
                sc.kind = MathNodeKind::Script;
                sc.children.push_back(emptyBase);
                attachScripts(sc);
                out.push_back(std::move(sc));
                continue;
            }

            MathNode atom = parseAtom();
            if (atom.kind == MathNodeKind::Text && atom.text.empty()) {
                continue;
            }

            size_t savedPos = m_pos;
            skipSpaces();
            if (!eof() && (peek() == '^' || peek() == '_')) {
                MathNode sc;
                sc.kind = MathNodeKind::Script;
                sc.children.push_back(std::move(atom));
                attachScripts(sc);
                out.push_back(std::move(sc));
            } else {
                m_pos = savedPos;
                bool canMerge = !out.empty() &&
                                out.back().kind == MathNodeKind::Text &&
                                atom.kind == MathNodeKind::Text &&
                                !isBigOperatorText(out.back().text) &&
                                !isBigOperatorText(atom.text);
                if (canMerge) {
                    if (!(out.back().text.back() == ' ' && atom.text == " ")) {
                        out.back().text += atom.text;
                    }
                } else {
                    out.push_back(std::move(atom));
                }
            }
        }
        return out;
    }

    void attachScripts(MathNode& sc) {
        while (!eof()) {
            size_t saved = m_pos;
            skipSpaces();
            if (eof()) {
                m_pos = saved;
                break;
            }
            char c = peek();
            if (c == '^') {
                get();
                auto nodes = parseArgNodes();
                sc.sup.insert(sc.sup.end(), nodes.begin(), nodes.end());
            } else if (c == '_') {
                get();
                auto nodes = parseArgNodes();
                sc.sub.insert(sc.sub.end(), nodes.begin(), nodes.end());
            } else {
                m_pos = saved;
                break;
            }
        }
    }
};

struct MathMetrics {
    float width = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
};

ImFont* activeFontSafe() {
    if (ImGui::GetCurrentContext() == nullptr) return nullptr;
    return ImGui::GetFont();
}

std::string resolveDrawableText(ImFont* font, const std::string& raw) {
    if (!font) return raw;
    const auto& utfMap = symbolByUtf8();
    std::string out;
    out.reserve(raw.size());
    size_t pos = 0;
    while (pos < raw.size()) {
        unsigned int cp = 0;
        size_t len = 1;
        decodeUtf8At(raw, pos, cp, len);
        if (cp < 0x80 || font->FindGlyphNoFallback(static_cast<ImWchar>(cp)) != nullptr) {
            out.append(raw, pos, len);
        } else {
            std::string seq = raw.substr(pos, len);
            auto it = utfMap.find(seq);
            if (it != utfMap.end()) {
                out += it->second->ascii;
            } else {
                out.push_back('?');
            }
        }
        pos += len;
    }
    return out;
}

float measureTextWidth(ImFont* font, const std::string& text, float fontSize) {
    if (text.empty()) return 0.0f;
    if (!font) {
        return static_cast<float>(text.size()) * fontSize * 0.55f;
    }
    ImVec2 sz = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
    return sz.x;
}

bool isVectorSymbol(const std::string& raw, ImFont* font) {
    std::string t = trimString(raw);
    if (t.empty()) return false;
    if (font) {
        unsigned int cp = 0;
        size_t len = 0;
        if (decodeUtf8At(t, 0, cp, len) && len == t.size() &&
            font->FindGlyphNoFallback(static_cast<ImWchar>(cp)) != nullptr) {
            return false;
        }
    }
    return t == "\xE2\x88\x91" || t == "\xE2\x88\xAB" ||
           t == "\xE2\x88\x8F" || t == "\xE2\x88\x9E" ||
           t == "\xE2\x86\x92" || t == "\xE2\x87\x92";
}

MathMetrics measureVectorSymbol(const std::string& raw, float fontSize) {
    std::string t = trimString(raw);
    MathMetrics m;
    if (t == "\xE2\x88\x91" || t == "\xE2\x88\x8F") {
        m.width = fontSize * 0.92f;
        m.ascent = fontSize * 0.98f;
        m.descent = fontSize * 0.36f;
    } else if (t == "\xE2\x88\xAB") {
        m.width = fontSize * 0.65f;
        m.ascent = fontSize * 1.08f;
        m.descent = fontSize * 0.44f;
    } else if (t == "\xE2\x88\x9E") {
        m.width = fontSize * 0.95f;
        m.ascent = fontSize * 0.68f;
        m.descent = fontSize * 0.14f;
    } else {
        m.width = fontSize * 1.05f;
        m.ascent = fontSize * 0.72f;
        m.descent = fontSize * 0.20f;
    }
    return m;
}

void drawVectorSymbol(ImDrawList* dl, const std::string& raw, float x, float baseline, float fontSize, ImU32 color) {
    std::string t = trimString(raw);
    float thick = std::max(1.4f, fontSize * 0.08f);
    if (t == "\xE2\x88\x91") {
        float w = fontSize * 0.80f;
        float topY = baseline - fontSize * 0.92f;
        float botY = baseline + fontSize * 0.30f;
        float midY = (topY + botY) * 0.5f;
        float lx = x + fontSize * 0.06f;
        ImVec2 pts[5] = {
            ImVec2(lx + w, topY),
            ImVec2(lx, topY),
            ImVec2(lx + w * 0.56f, midY),
            ImVec2(lx, botY),
            ImVec2(lx + w, botY)
        };
        dl->AddPolyline(pts, 5, color, 0, thick);
        return;
    }
    if (t == "\xE2\x88\x8F") {
        float w = fontSize * 0.80f;
        float topY = baseline - fontSize * 0.92f;
        float botY = baseline + fontSize * 0.30f;
        float lx = x + fontSize * 0.06f;
        dl->AddLine(ImVec2(lx, topY), ImVec2(lx + w, topY), color, thick * 1.15f);
        dl->AddLine(ImVec2(lx + w * 0.20f, topY), ImVec2(lx + w * 0.20f, botY), color, thick);
        dl->AddLine(ImVec2(lx + w * 0.80f, topY), ImVec2(lx + w * 0.80f, botY), color, thick);
        return;
    }
    if (t == "\xE2\x88\xAB") {
        float cx = x + fontSize * 0.32f;
        float topY = baseline - fontSize * 1.02f;
        float botY = baseline + fontSize * 0.38f;
        float r = fontSize * 0.16f;
        dl->AddBezierCubic(
            ImVec2(cx + r, topY),
            ImVec2(cx - r * 0.3f, topY),
            ImVec2(cx + r * 0.3f, botY),
            ImVec2(cx - r, botY),
            color, thick * 1.05f, 16);
        return;
    }
    if (t == "\xE2\x88\x9E") {
        float cy = baseline - fontSize * 0.32f;
        float r = fontSize * 0.20f;
        dl->AddCircle(ImVec2(x + r * 1.35f, cy), r, color, 16, thick);
        dl->AddCircle(ImVec2(x + r * 3.35f, cy), r, color, 16, thick);
        return;
    }
    float cy = baseline - fontSize * 0.32f;
    float lx = x + fontSize * 0.10f;
    float rx = x + fontSize * 0.92f;
    float ah = fontSize * 0.20f;
    if (t == "\xE2\x87\x92") {
        float sep = fontSize * 0.09f;
        dl->AddLine(ImVec2(lx, cy - sep), ImVec2(rx - ah * 0.6f, cy - sep), color, thick * 0.85f);
        dl->AddLine(ImVec2(lx, cy + sep), ImVec2(rx - ah * 0.6f, cy + sep), color, thick * 0.85f);
        dl->AddLine(ImVec2(rx - ah, cy - ah), ImVec2(rx, cy), color, thick);
        dl->AddLine(ImVec2(rx - ah, cy + ah), ImVec2(rx, cy), color, thick);
        return;
    }
    dl->AddLine(ImVec2(lx, cy), ImVec2(rx, cy), color, thick);
    dl->AddLine(ImVec2(rx - ah, cy - ah * 0.85f), ImVec2(rx, cy), color, thick);
    dl->AddLine(ImVec2(rx - ah, cy + ah * 0.85f), ImVec2(rx, cy), color, thick);
}

MathMetrics measureNodeList(const std::vector<MathNode>& nodes, float fontSize);
MathMetrics measureNode(const MathNode& node, float fontSize);

void drawNodeList(ImDrawList* dl, const std::vector<MathNode>& nodes, float x, float baseline, float fontSize, ImU32 color);
void drawNode(ImDrawList* dl, const MathNode& node, float x, float baseline, float fontSize, ImU32 color);

MathMetrics measureNodeList(const std::vector<MathNode>& nodes, float fontSize) {
    MathMetrics total;
    total.ascent = fontSize * 0.76f;
    total.descent = fontSize * 0.24f;
    if (nodes.empty()) return total;
    total.ascent = 0.0f;
    total.descent = 0.0f;
    for (const auto& child : nodes) {
        MathMetrics cm = measureNode(child, fontSize);
        total.width += cm.width;
        total.ascent = std::max(total.ascent, cm.ascent);
        total.descent = std::max(total.descent, cm.descent);
    }
    return total;
}

MathMetrics measureNode(const MathNode& node, float fontSize) {
    ImFont* font = activeFontSafe();
    switch (node.kind) {
        case MathNodeKind::Sequence:
        case MathNodeKind::Group:
            return measureNodeList(node.children, fontSize);

        case MathNodeKind::Text: {
            if (isVectorSymbol(node.text, font)) {
                return measureVectorSymbol(node.text, fontSize);
            }
            float effSize = isBigOperatorText(node.text) ? fontSize * 1.28f : fontSize;
            std::string drawable = resolveDrawableText(font, node.text);
            MathMetrics m;
            m.width = measureTextWidth(font, drawable, effSize);
            m.ascent = effSize * 0.78f;
            m.descent = effSize * 0.22f;
            return m;
        }

        case MathNodeKind::Fraction: {
            float childFont = std::max(10.0f, fontSize * 0.82f);
            MathMetrics numM = measureNodeList(node.num, childFont);
            MathMetrics denM = measureNodeList(node.den, childFont);
            float padX = fontSize * 0.22f;
            float gapY = std::max(2.5f, fontSize * 0.14f);
            float barThick = std::max(1.2f, fontSize * 0.065f);
            float axis = fontSize * 0.32f;
            MathMetrics m;
            m.width = std::max(numM.width, denM.width) + padX * 2.0f;
            m.ascent = axis + barThick * 0.5f + gapY + numM.ascent + numM.descent;
            float belowAxis = barThick * 0.5f + gapY + denM.ascent + denM.descent;
            m.descent = std::max(fontSize * 0.24f, belowAxis - axis);
            return m;
        }

        case MathNodeKind::Sqrt: {
            MathMetrics innerM = measureNodeList(node.children, fontSize);
            float hookW = fontSize * 0.62f;
            if (!node.sup.empty()) {
                MathMetrics idxM = measureNodeList(node.sup, std::max(9.0f, fontSize * 0.56f));
                hookW = std::max(hookW, idxM.width + fontSize * 0.28f);
            }
            float topGap = std::max(3.0f, fontSize * 0.15f);
            float thick = std::max(1.3f, fontSize * 0.07f);
            float padR = fontSize * 0.16f;
            MathMetrics m;
            m.width = hookW + innerM.width + padR;
            m.ascent = innerM.ascent + topGap + thick;
            m.descent = innerM.descent;
            return m;
        }

        case MathNodeKind::Script: {
            MathMetrics baseM = measureNodeList(node.children, fontSize);
            float scriptFont = std::max(9.0f, fontSize * 0.66f);
            MathMetrics supM = node.sup.empty() ? MathMetrics{} : measureNodeList(node.sup, scriptFont);
            MathMetrics subM = node.sub.empty() ? MathMetrics{} : measureNodeList(node.sub, scriptFont);
            float gapX = fontSize * 0.05f;
            float scriptW = std::max(supM.width, subM.width);
            MathMetrics m;
            m.width = baseM.width + (scriptW > 0.0f ? scriptW + gapX : 0.0f);
            m.ascent = baseM.ascent;
            m.descent = baseM.descent;
            if (!node.sup.empty()) {
                float supShift = baseM.ascent * 0.52f + supM.descent * 0.35f;
                m.ascent = std::max(m.ascent, supShift + supM.ascent);
            }
            if (!node.sub.empty()) {
                float subShift = std::max(baseM.descent, fontSize * 0.20f) + subM.ascent * 0.38f;
                m.descent = std::max(m.descent, subShift + subM.descent);
            }
            return m;
        }
    }
    return {};
}

void drawNodeList(ImDrawList* dl, const std::vector<MathNode>& nodes, float x, float baseline, float fontSize, ImU32 color) {
    float curX = x;
    for (const auto& child : nodes) {
        MathMetrics cm = measureNode(child, fontSize);
        drawNode(dl, child, curX, baseline, fontSize, color);
        curX += cm.width;
    }
}

void drawNode(ImDrawList* dl, const MathNode& node, float x, float baseline, float fontSize, ImU32 color) {
    if (!dl) return;
    ImFont* font = activeFontSafe();

    switch (node.kind) {
        case MathNodeKind::Sequence:
        case MathNodeKind::Group:
            drawNodeList(dl, node.children, x, baseline, fontSize, color);
            return;

        case MathNodeKind::Text: {
            if (isVectorSymbol(node.text, font)) {
                drawVectorSymbol(dl, node.text, x, baseline, fontSize, color);
                return;
            }
            float effSize = isBigOperatorText(node.text) ? fontSize * 1.28f : fontSize;
            std::string drawable = resolveDrawableText(font, node.text);
            float topY = baseline - effSize * 0.78f;
            if (font) {
                dl->AddText(font, effSize, ImVec2(x, topY), color, drawable.c_str());
            } else {
                dl->AddText(ImVec2(x, topY), color, drawable.c_str());
            }
            return;
        }

        case MathNodeKind::Fraction: {
            float childFont = std::max(10.0f, fontSize * 0.82f);
            MathMetrics numM = measureNodeList(node.num, childFont);
            MathMetrics denM = measureNodeList(node.den, childFont);
            float padX = fontSize * 0.22f;
            float gapY = std::max(2.5f, fontSize * 0.14f);
            float barThick = std::max(1.2f, fontSize * 0.065f);
            float axis = fontSize * 0.32f;
            float totalW = std::max(numM.width, denM.width) + padX * 2.0f;
            float axisY = baseline - axis;

            dl->AddLine(ImVec2(x + 2.0f, axisY), ImVec2(x + totalW - 2.0f, axisY), color, barThick);

            float numX = x + (totalW - numM.width) * 0.5f;
            float numBaseY = axisY - barThick * 0.5f - gapY - numM.descent;
            drawNodeList(dl, node.num, numX, numBaseY, childFont, color);

            float denX = x + (totalW - denM.width) * 0.5f;
            float denBaseY = axisY + barThick * 0.5f + gapY + denM.ascent;
            drawNodeList(dl, node.den, denX, denBaseY, childFont, color);
            return;
        }

        case MathNodeKind::Sqrt: {
            MathMetrics innerM = measureNodeList(node.children, fontSize);
            float idxFont = std::max(9.0f, fontSize * 0.56f);
            float hookW = fontSize * 0.62f;
            MathMetrics idxM{};
            if (!node.sup.empty()) {
                idxM = measureNodeList(node.sup, idxFont);
                hookW = std::max(hookW, idxM.width + fontSize * 0.28f);
            }
            float topGap = std::max(3.0f, fontSize * 0.15f);
            float thick = std::max(1.3f, fontSize * 0.07f);
            float padR = fontSize * 0.16f;
            float totalW = hookW + innerM.width + padR;

            float topY = baseline - innerM.ascent - topGap;
            float botY = baseline + innerM.descent * 0.65f;
            float midY = baseline - innerM.ascent * 0.22f;

            ImVec2 pts[5] = {
                ImVec2(x + 1.0f, midY),
                ImVec2(x + hookW * 0.28f, midY - fontSize * 0.05f),
                ImVec2(x + hookW * 0.54f, botY),
                ImVec2(x + hookW * 0.90f, topY),
                ImVec2(x + totalW - 1.0f, topY)
            };
            dl->AddPolyline(pts, 5, color, 0, thick);

            if (!node.sup.empty()) {
                float idxX = x + 1.0f;
                float idxBaseY = midY - fontSize * 0.16f;
                drawNodeList(dl, node.sup, idxX, idxBaseY, idxFont, color);
            }

            drawNodeList(dl, node.children, x + hookW, baseline, fontSize, color);
            return;
        }

        case MathNodeKind::Script: {
            MathMetrics baseM = measureNodeList(node.children, fontSize);
            float scriptFont = std::max(9.0f, fontSize * 0.66f);
            float gapX = fontSize * 0.05f;
            drawNodeList(dl, node.children, x, baseline, fontSize, color);

            if (!node.sup.empty()) {
                MathMetrics supM = measureNodeList(node.sup, scriptFont);
                float supBaseY = baseline - baseM.ascent * 0.52f - supM.descent * 0.35f;
                drawNodeList(dl, node.sup, x + baseM.width + gapX, supBaseY, scriptFont, color);
            }
            if (!node.sub.empty()) {
                MathMetrics subM = measureNodeList(node.sub, scriptFont);
                float subBaseY = baseline + std::max(baseM.descent, fontSize * 0.20f) + subM.ascent * 0.38f;
                drawNodeList(dl, node.sub, x + baseM.width + gapX, subBaseY, scriptFont, color);
            }
            return;
        }
    }
}

}

MathNode MathRenderer::parseLatex(const std::string& latex) {
    LatexParser parser(latex);
    return parser.parseRoot();
}

std::string MathRenderer::replaceSymbols(const std::string& latex) {
    const auto& symMap = symbolByCommand();
    std::string out;
    out.reserve(latex.size());
    size_t i = 0;
    while (i < latex.size()) {
        if (latex[i] == '\\' && i + 1 < latex.size()) {
            size_t j = i + 1;
            if (std::isalpha(static_cast<unsigned char>(latex[j]))) {
                while (j < latex.size() && std::isalpha(static_cast<unsigned char>(latex[j]))) {
                    ++j;
                }
                std::string cmd = latex.substr(i + 1, j - (i + 1));
                auto it = symMap.find(cmd);
                if (it != symMap.end()) {
                    out += it->second->utf8;
                    i = j;
                    continue;
                }
                out.append(latex, i, j - i);
                i = j;
                continue;
            }
        }
        out.push_back(latex[i]);
        ++i;
    }
    return out;
}

std::vector<MathBlock> MathRenderer::extractMathBlocks(const std::string& markdown) {
    std::vector<MathBlock> blocks;
    bool inCodeFence = false;
    bool inInlineCode = false;
    int line = 1;
    size_t i = 0;
    const size_t n = markdown.size();

    while (i < n) {
        bool lineStart = (i == 0 || markdown[i - 1] == '\n');
        if (lineStart && !inInlineCode && i + 2 < n &&
            markdown[i] == '`' && markdown[i + 1] == '`' && markdown[i + 2] == '`') {
            inCodeFence = !inCodeFence;
            while (i < n && markdown[i] != '\n') ++i;
            continue;
        }
        if (markdown[i] == '\n') {
            ++line;
            ++i;
            continue;
        }
        if (inCodeFence) {
            ++i;
            continue;
        }
        if (markdown[i] == '`') {
            inInlineCode = !inInlineCode;
            ++i;
            continue;
        }
        if (inInlineCode) {
            ++i;
            continue;
        }
        if (markdown[i] == '\\' && i + 1 < n && markdown[i + 1] == '$') {
            i += 2;
            continue;
        }
        if (markdown[i] == '$') {
            if (i + 1 < n && markdown[i + 1] == '$') {
                int startLine = line;
                size_t contentStart = i + 2;
                size_t pos = contentStart;
                int scanLine = line;
                bool found = false;
                while (pos + 1 < n) {
                    if (markdown[pos] == '\n') ++scanLine;
                    if (markdown[pos] == '\\' && pos + 1 < n) {
                        pos += 2;
                        continue;
                    }
                    if (markdown[pos] == '$' && markdown[pos + 1] == '$') {
                        found = true;
                        break;
                    }
                    ++pos;
                }
                if (found) {
                    std::string raw = trimString(markdown.substr(contentStart, pos - contentStart));
                    if (!raw.empty()) {
                        blocks.push_back({raw, true, startLine});
                    }
                    line = scanLine;
                    i = pos + 2;
                    continue;
                }
                i += 2;
                continue;
            }

            int startLine = line;
            size_t contentStart = i + 1;
            size_t pos = contentStart;
            bool found = false;
            while (pos < n && markdown[pos] != '\n') {
                if (markdown[pos] == '\\' && pos + 1 < n) {
                    pos += 2;
                    continue;
                }
                if (markdown[pos] == '$') {
                    found = true;
                    break;
                }
                ++pos;
            }
            if (found && pos > contentStart) {
                std::string raw = trimString(markdown.substr(contentStart, pos - contentStart));
                if (!raw.empty()) {
                    blocks.push_back({raw, false, startLine});
                }
                i = pos + 1;
                continue;
            }
        }
        ++i;
    }

    return blocks;
}

ImVec2 MathRenderer::measure(const MathNode& node, float fontSize) {
    MathMetrics m = measureNode(node, fontSize);
    return ImVec2(m.width, m.ascent + m.descent);
}

void MathRenderer::draw(ImDrawList* dl, const MathNode& node, ImVec2 origin, float fontSize, ImU32 color) {
    if (!dl) return;
    MathMetrics m = measureNode(node, fontSize);
    drawNode(dl, node, origin.x, origin.y + m.ascent, fontSize, color);
}

void MathRenderer::renderInlineWidget(const std::string& latex, float fontSize) {
    const auto& pal = UITheme::palette();
    MathNode root = parseLatex(latex);
    MathMetrics m = measureNode(root, fontSize);
    float padX = 6.0f;
    float padY = 3.0f;
    float w = std::max(m.width + padX * 2.0f, 18.0f);
    float h = std::max(m.ascent + m.descent + padY * 2.0f, fontSize + padY * 2.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(w, h));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pMax(pos.x + w, pos.y + h);
    dl->AddRectFilled(pos, pMax, ImGui::GetColorU32(pal.bgSurface3), 4.0f);
    dl->AddRect(pos, pMax, ImGui::GetColorU32(pal.border), 4.0f);

    ImVec2 drawOrigin(pos.x + padX, pos.y + (h - (m.ascent + m.descent)) * 0.5f);
    draw(dl, root, drawOrigin, fontSize, ImGui::GetColorU32(pal.textPrimary));
}

void MathRenderer::renderDisplayBox(const std::string& latex, float fontSize) {
    const auto& pal = UITheme::palette();
    MathNode root = parseLatex(latex);
    MathMetrics m = measureNode(root, fontSize);
    float contentH = m.ascent + m.descent;
    float availW = ImGui::GetContentRegionAvail().x;
    float boxW = std::max(availW, m.width + 44.0f);
    float boxH = std::max(contentH + 34.0f, fontSize + 34.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(boxW, boxH));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pMax(pos.x + boxW, pos.y + boxH);
    dl->AddRectFilled(pos, pMax, ImGui::GetColorU32(pal.bgSurface2), 8.0f);
    dl->AddRect(pos, pMax, ImGui::GetColorU32(pal.borderLine), 8.0f);
    dl->AddRectFilled(
        ImVec2(pos.x, pos.y + 6.0f),
        ImVec2(pos.x + 3.0f, pMax.y - 6.0f),
        ImGui::GetColorU32(pal.accent),
        2.0f);

    float originX = pos.x + std::max(20.0f, (boxW - m.width) * 0.5f);
    float originY = pos.y + (boxH - contentH) * 0.5f;
    draw(dl, root, ImVec2(originX, originY), fontSize, ImGui::GetColorU32(pal.textPrimary));
}

}
