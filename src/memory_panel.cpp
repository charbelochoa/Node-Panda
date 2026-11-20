#include "memory_panel.h"
#include "app.h"
#include "memory_engine.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>

namespace nodepanda {

namespace {

ImVec4 scoreAccentColor(float score, const ThemePalette& pal) {
    if (score > 0.6f) return pal.aqua;
    if (score > 0.3f) return pal.warning;
    return pal.textMuted;
}

bool renderResultRow(const MemoryResult& r, int idx, const ThemePalette& pal) {
    ImGui::PushID(idx);
    bool clicked = false;

    char scoreBuf[16];
    std::snprintf(scoreBuf, sizeof(scoreBuf), "%.0f%%", r.score * 100.0f);
    ImVec4 barCol = scoreAccentColor(r.score, pal);

    ImGui::PushStyleColor(ImGuiCol_Text, pal.textPrimary);
    float selW = std::max(80.0f, ImGui::GetContentRegionAvail().x - 62.0f);
    if (ImGui::Selectable(r.title.c_str(), false, 0, ImVec2(selW, 0.0f))) {
        clicked = true;
    }
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(pal.accent, "%s", r.title.c_str());
        ImGui::TextColored(pal.textMuted, "Similitud: %s  |  ID: %s", scoreBuf, r.noteId.c_str());
        ImGui::EndTooltip();
    }

    ImGui::SameLine();
    ui::badge(scoreBuf, ImVec4(barCol.x, barCol.y, barCol.z, 0.18f), barCol);
    ui::progressBarModern(r.score, barCol, ImGui::GetContentRegionAvail().x, 5.0f);
    ImGui::Spacing();

    ImGui::PopID();
    return clicked;
}

}

void MemoryPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::Begin("Motor de Memoria", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();

    auto& memory = app.getMemoryEngine();

    if (memory.needsReindex()) {
        memory.reindex(app.getNoteManager());
    }

    float availW = ImGui::GetContentRegionAvail().x;
    float cardW = std::max(100.0f, (availW - 8.0f) * 0.5f);
    ui::metricCard("##mem_notes", "Notas indexadas", std::to_string(memory.indexedNotes()), pal.accent, cardW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##mem_vocab", "Vocabulario TF-IDF", std::to_string(memory.vocabularySize()), pal.aqua, cardW);

    ImGui::Spacing();
    if (ui::subtleButton("Re-indexar Memoria", ImVec2(-1.0f, 0.0f))) {
        memory.reindex(app.getNoteManager());
        ui::pushToast("Indice TF-IDF actualizado", "success");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reconstruir el indice TF-IDF manualmente");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ui::sectionHeader("Busqueda Semantica");

    ImGui::SetNextItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x - 86.0f));
    bool enterPressed = ImGui::InputTextWithHint("##memsearch", "Buscar por contenido...",
                                                 m_searchBuffer, SEARCH_SIZE,
                                                 ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0.0f, 6.0f);
    bool searchClicked = ui::primaryButton("Buscar", ImVec2(78.0f, 0.0f));

    if (enterPressed || searchClicked) {
        m_searchTriggered = true;
    }

    if (m_searchTriggered && std::strlen(m_searchBuffer) > 0) {
        ImGui::Spacing();
        auto results = memory.search(m_searchBuffer, 10);
        if (results.empty()) {
            ImGui::TextColored(pal.textMuted, "Sin resultados para \"%s\"", m_searchBuffer);
        } else {
            char countBuf[48];
            std::snprintf(countBuf, sizeof(countBuf), "%d resultado(s)", static_cast<int>(results.size()));
            ui::badge(countBuf, pal.accentSoft, pal.accent);
            ImGui::Spacing();
            for (int i = 0; i < static_cast<int>(results.size()); ++i) {
                if (renderResultRow(results[i], 1000 + i, pal)) {
                    app.selectedNoteId = results[i].noteId;
                }
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (app.selectedNoteId.empty()) {
        ImGui::TextColored(pal.textMuted, "Selecciona una nota para ver relaciones");
        ImGui::End();
        return;
    }

    ui::sectionHeader("Notas Relacionadas");

    auto similar = memory.findSimilar(app.selectedNoteId, 8);
    if (similar.empty()) {
        ImGui::TextColored(pal.textMuted, "No se encontraron notas similares");
    } else {
        for (int i = 0; i < static_cast<int>(similar.size()); ++i) {
            if (renderResultRow(similar[i], 2000 + i, pal)) {
                app.selectedNoteId = similar[i].noteId;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ui::sectionHeader("Sugerencias de Enlace");

    Note* currentNote = app.getNoteManager().getNoteById(app.selectedNoteId);
    if (currentNote) {
        auto suggestions = memory.suggestLinks(app.selectedNoteId, currentNote->links, 5);

        if (suggestions.empty()) {
            ImGui::TextColored(pal.textMuted, "Sin sugerencias adicionales");
        } else {
            for (int i = 0; i < static_cast<int>(suggestions.size()); ++i) {
                ImGui::PushID(3000 + i);
                const auto& s = suggestions[i];
                ImVec4 barCol = scoreAccentColor(s.score, pal);

                if (ui::beginCard("##sug_card", ImVec2(0, 0), false)) {
                    char scoreBuf[16];
                    std::snprintf(scoreBuf, sizeof(scoreBuf), "%.0f%%", s.score * 100.0f);

                    float titleW = std::max(80.0f, ImGui::GetContentRegionAvail().x - 130.0f);
                    if (ImGui::Selectable(s.title.c_str(), false, 0, ImVec2(titleW, 0.0f))) {
                        app.selectedNoteId = s.noteId;
                    }
                    ImGui::SameLine();
                    ui::badge(scoreBuf, ImVec4(barCol.x, barCol.y, barCol.z, 0.18f), barCol);
                    ImGui::SameLine(0.0f, 6.0f);
                    if (ui::subtleButton("+ Enlazar")) {
                        currentNote->content += "\n[[" + s.noteId + "]]";
                        currentNote->dirty = true;
                        currentNote->saveToFile();
                        currentNote->parseLinks();
                        app.graphNeedsRebuild = true;
                        app.rebuildKnowledgeIndices();
                        memory.markDirty();
                        ui::pushToast("Enlace insertado", "success");
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Insertar [[%s]] al final de la nota", s.noteId.c_str());

                    ui::progressBarModern(s.score, barCol, ImGui::GetContentRegionAvail().x, 5.0f);
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
