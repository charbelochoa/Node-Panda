#include "backlinks_panel.h"
#include "imgui.h"
#include <algorithm>
#include <sstream>

namespace nodepanda {

void BacklinksPanel::recalculate(App& app) {
    m_backlinks.clear();
    m_unlinkedMentions.clear();

    if (app.selectedNoteId.empty()) return;

    const auto& allNotes = app.getNoteManager().getAllNotes();
    const std::string& currentId = app.selectedNoteId;

    for (const auto& [noteId, note] : allNotes) {
        if (noteId == currentId) continue;

        bool hasDirectLink = false;
        for (const auto& link : note.links) {
            if (link == currentId) {
                m_backlinks.push_back(noteId);
                hasDirectLink = true;
                break;
            }
            auto* currentNote = app.getNoteManager().getNoteById(currentId);
            if (currentNote) {
                std::string aliasStr = currentNote->getFrontmatter("aliases");
                if (!aliasStr.empty()) {
                    std::istringstream aliasStream(aliasStr);
                    std::string alias;
                    while (std::getline(aliasStream, alias, ',')) {
                        // Trim
                        size_t s = alias.find_first_not_of(" \t");
                        size_t e = alias.find_last_not_of(" \t");
                        if (s != std::string::npos) alias = alias.substr(s, e - s + 1);
                        if (link == alias) {
                            m_backlinks.push_back(noteId);
                            hasDirectLink = true;
                            break;
                        }
                    }
                }
            }
            if (hasDirectLink) break;
        }

        if (!hasDirectLink) {
            std::string lowerContent = note.content;
            std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
            std::string lowerTarget = currentId;
            std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), ::tolower);

            size_t pos = lowerContent.find(lowerTarget);
            if (pos != std::string::npos) {
                bool isLinked = false;
                if (pos >= 2) {
                    std::string before = note.content.substr(pos - 2, 2);
                    if (before == "[[") isLinked = true;
                }
                if (!isLinked) {
                    
                    size_t ctxStart = (pos > 30) ? pos - 30 : 0;
                    size_t ctxLen = std::min<size_t>(80, note.content.size() - ctxStart);
                    std::string context = "..." + note.content.substr(ctxStart, ctxLen) + "...";
                    
                    std::replace(context.begin(), context.end(), '\n', ' ');
                    std::replace(context.begin(), context.end(), '\r', ' ');
                    m_unlinkedMentions.push_back({noteId, context});
                }
            }
        }
    }

    
    std::sort(m_backlinks.begin(), m_backlinks.end());
}

void BacklinksPanel::render(App& app) {
    ImGui::Begin("Backlinks", nullptr, ImGuiWindowFlags_NoCollapse);

    if (app.selectedNoteId.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Selecciona una nota");
        ImGui::End();
        return;
    }

    
    if (m_cachedNoteId != app.selectedNoteId) {
        m_cachedNoteId = app.selectedNoteId;
        recalculate(app);
    }

    
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Nota: %s", app.selectedNoteId.c_str());
    ImGui::Separator();

    
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Enlaces Entrantes", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_backlinks.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), "  Ninguna nota enlaza aqui.");
        } else {
            for (const auto& bl : m_backlinks) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.5f, 1.0f));
                if (ImGui::Selectable(("  [[" + bl + "]]").c_str())) {
                    app.selectedNoteId = bl;
                    m_cachedNoteId.clear(); 
                }
                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered()) {
                    auto* note = app.getNoteManager().getNoteById(bl);
                    if (note) {
                        ImGui::BeginTooltip();
                        std::string preview = note->content.substr(0, 120);
                        if (note->content.size() > 120) preview += "...";
                        ImGui::TextWrapped("%s", preview.c_str());
                        ImGui::EndTooltip();
                    }
                }
            }
        }
        ImGui::Text("  Total: %d", (int)m_backlinks.size());
    }

    
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Menciones Sin Vincular", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_unlinkedMentions.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), "  No hay menciones sin vincular.");
        } else {
            for (const auto& [noteId, context] : m_unlinkedMentions) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
                ImGui::Text("  %s", noteId.c_str());
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.65f, 1.0f));
                ImGui::TextWrapped("    %s", context.c_str());
                ImGui::PopStyleColor();

                
                std::string btnId = "Vincular##" + noteId;
                ImGui::SameLine();
                if (ImGui::SmallButton(btnId.c_str())) {
                    
                    auto* note = app.getNoteManager().getNoteById(noteId);
                    if (note) {
                        
                        size_t pos = note->content.find(app.selectedNoteId);
                        if (pos != std::string::npos) {
                            std::string linked = "[[" + app.selectedNoteId + "]]";
                            note->content.replace(pos, app.selectedNoteId.size(), linked);
                            note->dirty = true;
                            note->parseLinks();
                            app.graphNeedsRebuild = true;
                            m_cachedNoteId.clear(); 
                        }
                    }
                }

                ImGui::Separator();
            }
        }
        ImGui::Text("  Total: %d", (int)m_unlinkedMentions.size());
    }

    
    ImGui::Spacing();
    if (ImGui::Button("Refrescar", ImVec2(-1, 25))) {
        m_cachedNoteId.clear();
    }

    ImGui::End();
}

} 
