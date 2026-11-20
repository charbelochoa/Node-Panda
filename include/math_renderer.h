#pragma once

#include "imgui.h"
#include <string>
#include <vector>

namespace nodepanda {

enum class MathNodeKind {
    Sequence,
    Text,
    Fraction,
    Script,
    Sqrt,
    Group
};

struct MathNode {
    MathNodeKind kind = MathNodeKind::Text;
    std::string text;
    std::vector<MathNode> children;
    std::vector<MathNode> num;
    std::vector<MathNode> den;
    std::vector<MathNode> sup;
    std::vector<MathNode> sub;
};

struct MathBlock {
    std::string rawLatex;
    bool displayMode = false;
    int lineNumber = 0;
};

class MathRenderer {
public:
    static MathNode parseLatex(const std::string& latex);
    static std::string replaceSymbols(const std::string& latex);
    static std::vector<MathBlock> extractMathBlocks(const std::string& markdown);
    static ImVec2 measure(const MathNode& node, float fontSize);
    static void draw(ImDrawList* dl, const MathNode& node, ImVec2 origin, float fontSize, ImU32 color);
    static void renderInlineWidget(const std::string& latex, float fontSize = 18.0f);
    static void renderDisplayBox(const std::string& latex, float fontSize = 22.0f);
};

}
