#include "markdown_parser.h"
#include "texture_manager.h"
#include <regex>
#include <sstream>
#include <filesystem>

#include "imgui.h"

namespace nodepanda {

std::vector<std::string> MarkdownParser::extractLinks(const std::string& content) {
    std::vector<std::string> links;
    std::regex linkRegex(R"(\[\[(.+?)\]\])");
    auto begin = std::sregex_iterator(content.begin(), content.end(), linkRegex);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::smatch match = *it;
        links.push_back(match[1].str());
    }
    return links;
}


static bool tryRenderImage(const std::string& imagePath,
                           TextureManager* texManager) {
    if (!texManager) return false;
    if (!std::filesystem::exists(imagePath)) return false;

    GLuint texId = 0;
    int w = 0, h = 0;
    if (!texManager->LoadTextureFromFile(imagePath.c_str(), &texId, &w, &h)) {
        return false;
    }

    float maxWidth = ImGui::GetContentRegionAvail().x;
    float scale = 1.0f;
    if (w > 0 && (float)w > maxWidth) {
        scale = maxWidth / (float)w;
    }
    float displayW = (float)w * scale;
    float displayH = (float)h * scale;

    ImGui::Image((ImTextureID)(intptr_t)texId, ImVec2(displayW, displayH));
    return true;
}

void MarkdownParser::renderToImGui(const std::string& content, std::string& clickedLink,
                                     TextureManager* texManager, const std::string& notesDir) {
    clickedLink.clear();
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.size() >= 4 && line[0] == '#' && line[1] == '#' && line[2] == '#' && line[3] == ' ') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(HEADER_COLOR[0], HEADER_COLOR[1], HEADER_COLOR[2], HEADER_COLOR[3]));
            ImGui::TextWrapped("%s", line.substr(4).c_str());
            ImGui::PopStyleColor();
            continue;
        }
        if (line.size() >= 3 && line[0] == '#' && line[1] == '#' && line[2] == ' ') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(HEADER_COLOR[0], HEADER_COLOR[1], HEADER_COLOR[2], HEADER_COLOR[3]));
            ImGui::TextWrapped("%s", line.substr(3).c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            continue;
        }
        if (line.size() >= 2 && line[0] == '#' && line[1] == ' ') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(HEADER_COLOR[0], HEADER_COLOR[1], HEADER_COLOR[2], HEADER_COLOR[3]));
            float oldSize = ImGui::GetFont()->Scale;
            ImGui::GetFont()->Scale *= 1.4f;
            ImGui::PushFont(ImGui::GetFont());
            ImGui::TextWrapped("%s", line.substr(2).c_str());
            ImGui::GetFont()->Scale = oldSize;
            ImGui::PopFont();
            ImGui::PopStyleColor();
            ImGui::Separator();
            continue;
        }

        
        {
            std::regex imgObsidian(R"(!\[\[(.+?)\]\])");
            std::smatch m;
            if (std::regex_search(line, m, imgObsidian) && texManager) {
                std::string imageName = m[1].str();
                std::filesystem::path imgPath = std::filesystem::path(notesDir) / imageName;
                if (tryRenderImage(imgPath.string(), texManager)) {
                    continue;
                }
                if (tryRenderImage(imageName, texManager)) {
                    continue;
                }
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                    "[Imagen no encontrada: %s]", imageName.c_str());
                continue;
            }
        }

        
        {
            std::regex imgMarkdown(R"(!\[([^\]]*)\]\(([^)]+)\))");
            std::smatch m;
            if (std::regex_search(line, m, imgMarkdown) && texManager) {
                std::string altText = m[1].str();
                std::string imgSrc  = m[2].str();

                std::filesystem::path imgPath = std::filesystem::path(notesDir) / imgSrc;
                if (tryRenderImage(imgPath.string(), texManager)) {
                    if (!altText.empty()) {
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f),
                            "%s", altText.c_str());
                    }
                    continue;
                }
                if (tryRenderImage(imgSrc, texManager)) {
                    if (!altText.empty()) {
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f),
                            "%s", altText.c_str());
                    }
                    continue;
                }
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                    "[Imagen no encontrada: %s (%s)]", altText.c_str(), imgSrc.c_str());
                continue;
            }
        }

        if (line.size() >= 2 && line[0] == '-' && line[1] == ' ') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(BULLET_COLOR[0], BULLET_COLOR[1], BULLET_COLOR[2], BULLET_COLOR[3]));
            ImGui::BulletText("%s", line.substr(2).c_str());
            ImGui::PopStyleColor();
            continue;
        }

        if (line.empty()) {
            ImGui::Spacing();
            continue;
        }

        if (line == "---") {
            ImGui::Separator();
            continue;
        }

        std::regex linkRegex(R"(\[\[(.+?)\]\])");
        std::sregex_iterator begin(line.begin(), line.end(), linkRegex);
        std::sregex_iterator endIter;

        if (begin != endIter) {
            size_t lastPos = 0;
            for (auto it = begin; it != endIter; ++it) {
                std::smatch match = *it;
                size_t matchPos = match.position();

                if (matchPos > lastPos) {
                    ImGui::TextWrapped("%s", line.substr(lastPos, matchPos - lastPos).c_str());
                    ImGui::SameLine(0, 0);
                }

                std::string linkName = match[1].str();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.3f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.5f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(LINK_COLOR[0], LINK_COLOR[1], LINK_COLOR[2], LINK_COLOR[3]));
                std::string btnLabel = "[[" + linkName + "]]##" + std::to_string(matchPos);
                if (ImGui::SmallButton(btnLabel.c_str())) {
                    clickedLink = linkName;
                }
                ImGui::PopStyleColor(3);
                ImGui::SameLine(0, 0);

                lastPos = matchPos + match.length();
            }
            if (lastPos < line.size()) {
                ImGui::TextWrapped("%s", line.substr(lastPos).c_str());
            } else {
                ImGui::NewLine();
            }
            continue;
        }

        if (line.size() >= 1 && line[0] == '`') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", line.c_str());
            ImGui::PopStyleColor();
            continue;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(NORMAL_COLOR[0], NORMAL_COLOR[1], NORMAL_COLOR[2], NORMAL_COLOR[3]));
        ImGui::TextWrapped("%s", line.c_str());
        ImGui::PopStyleColor();
    }
}

} 
