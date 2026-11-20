#include "backlinks_panel.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace nodepanda {

namespace {

std::string toLowerText(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool isInsideWikiLink(const std::string& content, size_t pos) {
    size_t openBracket = content.rfind("[[", pos);
    if (openBracket == std::string::npos) return false;
    size_t closeBracket = content.rfind("]]", pos);
    if (closeBracket != std::string::npos && closeBracket > openBracket) return false;
    size_t nextClose = content.find("]]", pos);
    return nextClose != std::string::npos;
}

bool findUnlinkedOccurrence(const std::string& content,
                            const std::vector<std::string>& targets,
                            size_t& outPos,
                            size_t& outLen) {
    std::string lowerContent = toLowerText(content);
    for (const auto& target : targets) {
        if (target.size() < 2) continue;
        std::string lowerTarget = toLowerText(target);
        size_t searchPos = 0;
        while (searchPos < lowerContent.size()) {
            size_t found = lowerContent.find(lowerTarget, searchPos);
            if (found == std::string::npos) break;
            if (!isInsideWikiLink(content, found)) {
                outPos = found;
                outLen = target.size();
                return true;
            }
            searchPos = found + lowerTarget.size();
        }
    }
    return false;
}

std::vector<std::string> buildMentionTargets(const std::string& currentId, const Note* currentNote) {
    std::vector<std::string> targets;
    if (currentNote && !currentNote->title.empty() && currentNote->title != currentId) {
        targets.push_back(currentNote->title);
    }
    targets.push_back(currentId);
    return targets;
}

}

void BacklinksPanel::recalculate(App& app) {
    m_backlinks.clear();
    m_unlinkedMentions.clear();

    if (app.selectedNoteId.empty()) return;

    const auto& allNotes = app.getNoteManager().getAllNotes();
    const std::string& currentId = app.selectedNoteId;
    const Note* currentNote = app.getNoteManager().getNoteById(currentId);

    std::vector<std::string> aliases;
    if (currentNote) {
        std::string aliasStr = currentNote->getFrontmatter("aliases");
        if (!aliasStr.empty()) {
            std::istringstream aliasStream(aliasStr);
            std::string alias;
            while (std::getline(aliasStream, alias, ',')) {
                size_t s = alias.find_first_not_of(" \t");
                size_t e = alias.find_last_not_of(" \t");
                if (s != std::string::npos) {
                    aliases.push_back(alias.substr(s, e - s + 1));
                }
            }
        }
    }

    std::vector<std::string> mentionTargets = buildMentionTargets(currentId, currentNote);

    for (const auto& [noteId, note] : allNotes) {
        if (noteId == currentId) continue;

        bool hasDirectLink = false;
        for (const auto& link : note.links) {
            if (link == currentId) {
                m_backlinks.push_back(noteId);
                hasDirectLink = true;
                break;
            }
            for (const auto& alias : aliases) {
                if (link == alias) {
                    m_backlinks.push_back(noteId);
                    hasDirectLink = true;
                    break;
                }
            }
            if (hasDirectLink) break;
        }

        if (!hasDirectLink) {
            size_t matchPos = 0;
            size_t matchLen = 0;
            if (findUnlinkedOccurrence(note.content, mentionTargets, matchPos, matchLen)) {
                size_t ctxStart = (matchPos > 30) ? matchPos - 30 : 0;
                size_t ctxLen = std::min<size_t>(90, note.content.size() - ctxStart);
                std::string context = "..." + note.content.substr(ctxStart, ctxLen) + "...";
                std::replace(context.begin(), context.end(), '\n', ' ');
                std::replace(context.begin(), context.end(), '\r', ' ');
                m_unlinkedMentions.push_back({noteId, context});
            }
        }
    }

    std::sort(m_backlinks.begin(), m_backlinks.end());
    std::sort(m_unlinkedMentions.begin(), m_unlinkedMentions.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
}

void BacklinksPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();

    ImGui::Begin("Backlinks", nullptr, ImGuiWindowFlags_NoCollapse);

    if (app.selectedNoteId.empty()) {
        ImGui::TextColored(pal.textMuted, "Selecciona una nota");
        ImGui::End();
        return;
    }

    if (m_cachedNoteId != app.selectedNoteId) {
        m_cachedNoteId = app.selectedNoteId;
        recalculate(app);
    }

    Note* activeNote = app.getNoteManager().getNoteById(app.selectedNoteId);
    ImGui::TextColored(pal.textPrimary, "%s", activeNote ? activeNote->title.c_str() : app.selectedNoteId.c_str());
    ImGui::SameLine(0.0f, 8.0f);
    ui::badge(app.selectedNoteId.c_str(), pal.accentSoft, pal.accent);

    ImGui::Spacing();
    std::string blCountStr = std::to_string(m_backlinks.size()) + " enlaces";
    std::string umCountStr = std::to_string(m_unlinkedMentions.size()) + " menciones";
    ui::badge(blCountStr.c_str(), ImVec4(pal.aqua.x, pal.aqua.y, pal.aqua.z, 0.18f), pal.aqua);
    ImGui::SameLine(0.0f, 6.0f);
    ui::badge(umCountStr.c_str(), ImVec4(pal.warning.x, pal.warning.y, pal.warning.z, 0.18f), pal.warning);
    ImGui::SameLine(0.0f, 8.0f);
    if (ui::subtleButton("Refrescar")) {
        m_cachedNoteId.clear();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Enlaces entrantes", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_backlinks.empty()) {
            ImGui::TextColored(pal.textMuted, "  Ninguna nota enlaza aqui.");
        } else {
            for (const auto& bl : m_backlinks) {
                ImGui::PushID(bl.c_str());
                ImGui::PushStyleColor(ImGuiCol_Text, pal.aqua);
                if (ImGui::Selectable(("  [[" + bl + "]]").c_str())) {
                    app.selectedNoteId = bl;
                    m_cachedNoteId.clear();
                }
                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered()) {
                    auto* note = app.getNoteManager().getNoteById(bl);
                    if (note) {
                        ImGui::BeginTooltip();
                        ImGui::TextColored(pal.accent, "%s", note->title.c_str());
                        std::string preview = note->content.substr(0, 140);
                        if (note->content.size() > 140) preview += "...";
                        ImGui::TextWrapped("%s", preview.c_str());
                        ImGui::EndTooltip();
                    }
                }
                ImGui::PopID();
            }
        }
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Menciones sin enlazar", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_unlinkedMentions.empty()) {
            ImGui::TextColored(pal.textMuted, "  No hay menciones sin enlazar.");
        } else {
            std::vector<std::string> targets = buildMentionTargets(app.selectedNoteId, activeNote);
            for (size_t i = 0; i < m_unlinkedMentions.size(); ++i) {
                const auto& [noteId, context] = m_unlinkedMentions[i];
                ImGui::PushID(static_cast<int>(i));

                if (ui::beginCard("##unlinked_card", ImVec2(0, 0), false)) {
                    ImGui::PushStyleColor(ImGuiCol_Text, pal.warning);
                    if (ImGui::Selectable(noteId.c_str(), false, 0, ImVec2(ImGui::GetContentRegionAvail().x - 84.0f, 0.0f))) {
                        app.selectedNoteId = noteId;
                        m_cachedNoteId.clear();
                    }
                    ImGui::PopStyleColor();

                    ImGui::SameLine();
                    if (ui::subtleButton("Enlazar", ImVec2(76.0f, 0.0f))) {
                        Note* srcNote = app.getNoteManager().getNoteById(noteId);
                        if (srcNote) {
                            size_t pos = 0;
                            size_t len = 0;
                            if (findUnlinkedOccurrence(srcNote->content, targets, pos, len)) {
                                std::string linked = "[[" + app.selectedNoteId + "]]";
                                srcNote->content.replace(pos, len, linked);
                                srcNote->dirty = true;
                                srcNote->saveToFile();
                                srcNote->parseLinks();
                                app.graphNeedsRebuild = true;
                                app.rebuildKnowledgeIndices();
                                m_cachedNoteId.clear();
                                ui::pushToast("Mencion enlazada", "success");
                            }
                        }
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
                    ImGui::TextWrapped("%s", context.c_str());
                    ImGui::PopStyleColor();
                }
                ui::endCard();
                ImGui::Spacing();
                ImGui::PopID();
            }
        }
    }

    ImGui::End();
}

}
