#pragma once
#include <string>
#include <vector>

namespace nodepanda {

class TextureManager;

class MarkdownParser {
public:
    static std::vector<std::string> extractLinks(const std::string& content);

   
    static void renderToImGui(const std::string& content, std::string& clickedLink,
                               TextureManager* texManager = nullptr,
                               const std::string& notesDir = "");

private:
    static constexpr float HEADER_COLOR[4]  = {0.4f, 0.8f, 1.0f, 1.0f};
    static constexpr float LINK_COLOR[4]    = {0.3f, 0.9f, 0.5f, 1.0f};
    static constexpr float BOLD_COLOR[4]    = {1.0f, 1.0f, 1.0f, 1.0f};
    static constexpr float NORMAL_COLOR[4]  = {0.85f, 0.85f, 0.85f, 1.0f};
    static constexpr float BULLET_COLOR[4]  = {0.6f, 0.6f, 0.9f, 1.0f};
};

} 
