// ============================================================================
// MemoryPanel — Panel de Memoria Semántica (UI)
// ============================================================================

#include "memory_panel.h"
#include "app.h"
#include "memory_engine.h"
#include "imgui.h"

#include <cstring>
#include <algorithm>

namespace nodepanda {

// ============================================================================
// Helper: renderizar una barra de relevancia visual
// ============================================================================
static void renderScoreBar(float score) {
    // Barra horizontal proporcional al score
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float barW = 60.0f * score;
    float barH = 4.0f;
    ImU32 col;
    if (score > 0.6f)
        col = IM_COL32(72, 224, 210, 255);  // cyan fuerte
    else if (score > 0.3f)
        col = IM_COL32(200, 200, 80, 255);  // amarillo
    else
        col = IM_COL32(100, 110, 140, 200);  // gris suave

    ImGui::GetWindowDrawList()->AddRectFilled(
        pos, ImVec2(pos.x + barW, pos.y + barH), col, 2.0f);
    // Fondo de la barra
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(pos.x + barW, pos.y),
        ImVec2(pos.x + 60.0f, pos.y + barH),
        IM_COL32(40, 42, 55, 150), 2.0f);
    ImGui::Dummy(ImVec2(60.0f, barH + 2.0f));
}

// ============================================================================
// Helper: renderizar un resultado clickeable
// ============================================================================
static bool renderResult(const MemoryResult& r, int idx) {
    ImGui::PushID(idx);
    bool clicked = false;

    // Score como porcentaje
    char scoreBuf[16];
    std::snprintf(scoreBuf, sizeof(scoreBuf), "%.0f%%", r.score * 100.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.80f, 0.55f, 1.0f));
    if (ImGui::Selectable(("  " + r.title).c_str(), false)) {
        clicked = true;
    }
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "%s", r.title.c_str());
        ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.66f, 1.0f),
                           "Similitud: %s  |  ID: %s", scoreBuf, r.noteId.c_str());
        ImGui::EndTooltip();
    }

    // Barra de score en la misma línea
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 64.0f);
    ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f), "%s", scoreBuf);

    ImGui::PopID();
    return clicked;
}

// ============================================================================
// render — Renderizar el panel completo
// ============================================================================
void MemoryPanel::render(App& app) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::Begin("Motor de Memoria", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();

    auto& memory = app.getMemoryEngine();

    // ── Re-indexar si es necesario ──────────────────────────────────────────
    if (memory.needsReindex()) {
        memory.reindex(app.getNoteManager());
    }

    // ── Encabezado con estadísticas ─────────────────────────────────────────
    ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "Motor de Memoria");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.35f, 0.37f, 0.48f, 1.0f),
                       "  %d notas  |  %d terminos",
                       memory.indexedNotes(), memory.vocabularySize());

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════════
    // SECCIÓN 1: Búsqueda Semántica
    // ═══════════════════════════════════════════════════════════════════════════
    ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f), "Busqueda Semantica");
    ImGui::Spacing();

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
    bool enterPressed = ImGui::InputTextWithHint("##memsearch", "Buscar por contenido...",
                                                  m_searchBuffer, SEARCH_SIZE,
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.45f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.18f, 0.58f, 0.52f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.08f, 0.35f, 0.30f, 1.0f));
    bool searchClicked = ImGui::Button("Buscar");
    ImGui::PopStyleColor(3);

    if (enterPressed || searchClicked) {
        m_searchTriggered = true;
    }

    if (m_searchTriggered && std::strlen(m_searchBuffer) > 0) {
        auto results = memory.search(m_searchBuffer, 10);
        if (results.empty()) {
            ImGui::TextColored(ImVec4(0.40f, 0.42f, 0.52f, 1.0f),
                               "Sin resultados para \"%s\"", m_searchBuffer);
        } else {
            ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.62f, 1.0f),
                               "%d resultado(s):", (int)results.size());
            for (int i = 0; i < (int)results.size(); ++i) {
                if (renderResult(results[i], 1000 + i)) {
                    app.selectedNoteId = results[i].noteId;
                }
            }
        }
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════════
    // SECCIÓN 2: Notas Relacionadas (a la nota seleccionada)
    // ═══════════════════════════════════════════════════════════════════════════
    if (app.selectedNoteId.empty()) {
        ImGui::TextColored(ImVec4(0.35f, 0.36f, 0.46f, 1.0f),
                           "Selecciona una nota para ver relaciones");
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f), "Notas Relacionadas");
    ImGui::Spacing();

    auto similar = memory.findSimilar(app.selectedNoteId, 8);
    if (similar.empty()) {
        ImGui::TextColored(ImVec4(0.40f, 0.42f, 0.52f, 1.0f),
                           "No se encontraron notas similares");
    } else {
        for (int i = 0; i < (int)similar.size(); ++i) {
            if (renderResult(similar[i], 2000 + i)) {
                app.selectedNoteId = similar[i].noteId;
            }
        }
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════════
    // SECCIÓN 3: Sugerencias de Enlace
    // ═══════════════════════════════════════════════════════════════════════════
    ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f), "Sugerencias de Enlace");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.40f, 0.42f, 0.52f, 1.0f), "(no enlazadas aun)");
    ImGui::Spacing();

    Note* currentNote = app.getNoteManager().getNoteById(app.selectedNoteId);
    if (currentNote) {
        auto suggestions = memory.suggestLinks(
            app.selectedNoteId, currentNote->links, 5);

        if (suggestions.empty()) {
            ImGui::TextColored(ImVec4(0.40f, 0.42f, 0.52f, 1.0f),
                               "Sin sugerencias adicionales");
        } else {
            for (int i = 0; i < (int)suggestions.size(); ++i) {
                ImGui::PushID(3000 + i);
                const auto& s = suggestions[i];

                // Botón para insertar enlace
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.25f, 0.40f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.20f, 0.35f, 0.55f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.12f, 0.20f, 0.32f, 1.0f));
                char linkBtn[32];
                std::snprintf(linkBtn, sizeof(linkBtn), " + ");
                if (ImGui::SmallButton(linkBtn)) {
                    // Insertar [[enlace]] al final del contenido
                    if (currentNote) {
                        currentNote->content += "\n[[" + s.noteId + "]]";
                        currentNote->dirty = true;
                        currentNote->parseLinks();
                        app.graphNeedsRebuild = true;
                        memory.markDirty();
                    }
                }
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Insertar [[%s]] al final de la nota", s.noteId.c_str());

                ImGui::SameLine();
                if (renderResult(s, 3000 + i)) {
                    app.selectedNoteId = s.noteId;
                }

                ImGui::PopID();
            }
        }
    }

    // ── Botón de re-indexar manual ──────────────────────────────────────────
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.20f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.25f, 0.28f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.14f, 0.16f, 0.24f, 1.0f));
    if (ImGui::Button("  Re-indexar Memoria  ")) {
        memory.reindex(app.getNoteManager());
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reconstruir el indice TF-IDF manualmente");

    ImGui::End();
}

} // namespace nodepanda
