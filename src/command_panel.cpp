// ============================================================================
// CommandPanel — Centro de Mando (Dashboard)
// ============================================================================

#include "command_panel.h"
#include "app.h"
#include "memory_engine.h"
#include "imgui.h"

#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <cstring>

namespace nodepanda {

// ============================================================================
// recalcStats — Recalcular todas las estadísticas del workspace
// ============================================================================
void CommandPanel::recalcStats(App& app) {
    auto& nm = app.getNoteManager();
    const auto& allNotes = nm.getAllNotes();

    m_stats = Stats{};
    m_stats.totalNotes = (int)allNotes.size();

    // Conjuntos para análisis
    std::unordered_set<std::string> allNoteIds;
    std::unordered_map<std::string, int> connectionCount; // total de enlaces (in + out)
    std::unordered_map<std::string, int> typeCounts;
    std::unordered_set<std::string> hasIncoming;   // notas que reciben al menos un enlace
    std::unordered_set<std::string> hasOutgoing;   // notas que envían al menos un enlace

    for (const auto& [id, note] : allNotes) {
        allNoteIds.insert(id);
        connectionCount[id] += 0; // asegurar entrada

        if (!note.links.empty()) {
            hasOutgoing.insert(id);
            connectionCount[id] += (int)note.links.size();
        }

        // Tipo de nota
        std::string tipo = note.getFrontmatter("tipo");
        if (!tipo.empty()) typeCounts[tipo]++;

        // Contar enlaces y detectar rotos
        for (const auto& link : note.links) {
            m_stats.totalLinks++;
            std::string resolved = nm.resolveAlias(link);
            if (nm.getNoteById(resolved)) {
                hasIncoming.insert(resolved);
                connectionCount[resolved]++;
            } else {
                m_stats.brokenLinks++;
                m_stats.brokenLinkList.push_back(id + " -> [[" + link + "]]");
            }
        }
    }

    // Notas huérfanas (sin enlaces entrantes NI salientes)
    for (const auto& id : allNoteIds) {
        if (hasIncoming.find(id) == hasIncoming.end() &&
            hasOutgoing.find(id) == hasOutgoing.end()) {
            m_stats.orphanNotes++;
            m_stats.orphanList.push_back(id);
        }
    }

    // Promedios y densidad
    if (m_stats.totalNotes > 0) {
        m_stats.avgLinksPerNote = (float)m_stats.totalLinks / (float)m_stats.totalNotes;
    }
    if (m_stats.totalNotes > 1) {
        float maxEdges = (float)m_stats.totalNotes * ((float)m_stats.totalNotes - 1.0f) / 2.0f;
        m_stats.graphDensity = (maxEdges > 0) ? (float)m_stats.totalLinks / maxEdges : 0.0f;
    }

    // Top 5 más conectadas
    std::vector<std::pair<std::string, int>> sorted(connectionCount.begin(), connectionCount.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    m_stats.topConnected.clear();
    for (int i = 0; i < std::min(5, (int)sorted.size()); ++i) {
        if (sorted[i].second > 0)
            m_stats.topConnected.push_back(sorted[i]);
    }

    // Distribución de tipos
    m_stats.typeDistribution.assign(typeCounts.begin(), typeCounts.end());
    std::sort(m_stats.typeDistribution.begin(), m_stats.typeDistribution.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    m_stats.typeCount = (int)typeCounts.size();

    // Notas recientes (por fecha de modificación)
    std::vector<std::pair<std::chrono::system_clock::time_point, std::pair<std::string, std::string>>> recent;
    for (const auto& [id, note] : allNotes) {
        recent.push_back({ note.lastModified, { id, note.title } });
    }
    std::sort(recent.begin(), recent.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    m_stats.recentNotes.clear();
    for (int i = 0; i < std::min(8, (int)recent.size()); ++i) {
        m_stats.recentNotes.push_back(recent[i].second);
    }

    m_cachedNoteCount = m_stats.totalNotes;
}

// ============================================================================
// Helper: barra horizontal proporcional
// ============================================================================
static void drawBar(float fraction, ImVec4 color, float width = 120.0f) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float h = 6.0f;
    ImU32 col = IM_COL32((int)(color.x * 255), (int)(color.y * 255),
                          (int)(color.z * 255), (int)(color.w * 255));
    ImGui::GetWindowDrawList()->AddRectFilled(
        pos, ImVec2(pos.x + width, pos.y + h), IM_COL32(30, 32, 45, 150), 3.0f);
    ImGui::GetWindowDrawList()->AddRectFilled(
        pos, ImVec2(pos.x + width * fraction, pos.y + h), col, 3.0f);
    ImGui::Dummy(ImVec2(width, h + 2.0f));
}

// ============================================================================
// render — Renderizar el panel completo
// ============================================================================
void CommandPanel::render(App& app) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
    ImGui::Begin("Centro de Mando", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();

    // Re-calcular si cambió el número de notas (heurística simple)
    int currentCount = (int)app.getNoteManager().getAllNotes().size();
    if (currentCount != m_cachedNoteCount || m_cachedNoteCount < 0) {
        recalcStats(app);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // ENCABEZADO
    // ═══════════════════════════════════════════════════════════════════════════
    ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "Centro de Mando");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.35f, 0.37f, 0.48f, 1.0f), "  Node Panda");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════════
    // MÉTRICAS PRINCIPALES
    // ═══════════════════════════════════════════════════════════════════════════
    ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f), "Metricas");
    ImGui::Spacing();

    // Grid de 2x3 métricas
    auto metric = [](const char* label, int value, ImVec4 color) {
        ImGui::TextColored(color, "%d", value);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f), "%s", label);
    };

    metric("notas", m_stats.totalNotes, ImVec4(0.28f, 0.88f, 0.82f, 1.0f));
    ImGui::SameLine(0.0f, 20.0f);
    metric("enlaces", m_stats.totalLinks, ImVec4(0.40f, 0.82f, 0.55f, 1.0f));

    metric("huerfanas", m_stats.orphanNotes,
           m_stats.orphanNotes > 0 ? ImVec4(0.95f, 0.65f, 0.25f, 1.0f) : ImVec4(0.40f, 0.82f, 0.55f, 1.0f));
    ImGui::SameLine(0.0f, 20.0f);
    metric("rotos", m_stats.brokenLinks,
           m_stats.brokenLinks > 0 ? ImVec4(0.95f, 0.40f, 0.30f, 1.0f) : ImVec4(0.40f, 0.82f, 0.55f, 1.0f));

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f),
                       "Promedio: %.1f enlaces/nota  |  Densidad: %.1f%%  |  %d tipos",
                       m_stats.avgLinksPerNote, m_stats.graphDensity * 100.0f, m_stats.typeCount);

    // ── Performance ─────────────────────────────────────────────────────────
    ImGui::Spacing();
    auto& perf = app.getPerformanceMetrics();
    float fps = perf.fps();
    ImVec4 fpsColor = (fps >= 60) ? ImVec4(0.40f, 0.90f, 0.55f, 1.0f) :
                      (fps >= 30) ? ImVec4(0.90f, 0.80f, 0.25f, 1.0f) :
                                    ImVec4(0.95f, 0.40f, 0.30f, 1.0f);
    ImGui::TextColored(fpsColor, "%.0f FPS", fps);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f),
                       "| %.1f ms | %.1f MB RAM",
                       perf.frameTimeMs(), perf.memoryMB());

    // ── Memory Engine stats ─────────────────────────────────────────────────
    auto& mem = app.getMemoryEngine();
    ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f),
                       "Motor Memoria: %d notas indexadas | %d terminos",
                       mem.indexedNotes(), mem.vocabularySize());

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════════
    // TOP NOTAS CONECTADAS
    // ═══════════════════════════════════════════════════════════════════════════
    if (!m_stats.topConnected.empty()) {
        ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f), "Nodos Hub");
        ImGui::Spacing();

        int maxConn = m_stats.topConnected.empty() ? 1 : m_stats.topConnected[0].second;
        for (int i = 0; i < (int)m_stats.topConnected.size(); ++i) {
            const auto& [id, count] = m_stats.topConnected[i];
            ImGui::PushID(i);

            float fraction = (maxConn > 0) ? (float)count / (float)maxConn : 0.0f;
            drawBar(fraction, ImVec4(0.28f, 0.88f, 0.82f, 0.8f));
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.80f, 0.55f, 1.0f));
            if (ImGui::Selectable(id.c_str(), false)) {
                app.selectedNoteId = id;
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f), "(%d)", count);

            ImGui::PopID();
        }
        ImGui::Spacing();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // DISTRIBUCIÓN DE TIPOS
    // ═══════════════════════════════════════════════════════════════════════════
    if (!m_stats.typeDistribution.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f), "Tipos de Nota");
        ImGui::Spacing();

        int maxType = m_stats.typeDistribution[0].second;
        for (const auto& [tipo, count] : m_stats.typeDistribution) {
            float fraction = (maxType > 0) ? (float)count / (float)maxType : 0.0f;

            // Color basado en el tipo
            ImVec4 barColor(0.45f, 0.47f, 0.60f, 0.8f);
            if (tipo == "proyecto")   barColor = ImVec4(0.15f, 0.85f, 0.90f, 0.8f);
            if (tipo == "concepto")   barColor = ImVec4(0.62f, 0.38f, 0.88f, 0.8f);
            if (tipo == "referencia") barColor = ImVec4(0.90f, 0.72f, 0.20f, 0.8f);
            if (tipo == "diario")     barColor = ImVec4(0.40f, 0.82f, 0.45f, 0.8f);
            if (tipo == "tarea")      barColor = ImVec4(0.95f, 0.45f, 0.35f, 0.8f);

            drawBar(fraction, barColor, 80.0f);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.60f, 0.62f, 0.72f, 1.0f), "%s (%d)", tipo.c_str(), count);
        }
        ImGui::Spacing();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // SALUD DEL GRAFO
    // ═══════════════════════════════════════════════════════════════════════════
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f), "Salud del Grafo");
    ImGui::Spacing();

    // Indicador de salud general
    float healthScore = 1.0f;
    if (m_stats.totalNotes > 0) {
        float orphanPenalty = (float)m_stats.orphanNotes / (float)m_stats.totalNotes;
        float brokenPenalty = (float)m_stats.brokenLinks / std::max(1.0f, (float)m_stats.totalLinks);
        healthScore = std::max(0.0f, 1.0f - orphanPenalty * 0.5f - brokenPenalty * 0.5f);
    }

    ImVec4 healthColor = (healthScore >= 0.8f) ? ImVec4(0.40f, 0.90f, 0.55f, 1.0f) :
                         (healthScore >= 0.5f) ? ImVec4(0.90f, 0.80f, 0.25f, 1.0f) :
                                                 ImVec4(0.95f, 0.40f, 0.30f, 1.0f);
    ImGui::TextColored(healthColor, "%.0f%%", healthScore * 100.0f);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f), "salud general");
    drawBar(healthScore, healthColor, 160.0f);

    // Notas huérfanas (expandible)
    if (m_stats.orphanNotes > 0) {
        ImGui::Spacing();
        ImVec4 orphanCol = ImVec4(0.95f, 0.65f, 0.25f, 1.0f);
        char orphanHeader[64];
        std::snprintf(orphanHeader, sizeof(orphanHeader),
                      "Notas huerfanas (%d)###orphans", m_stats.orphanNotes);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.18f, 0.10f, 0.5f));
        if (ImGui::CollapsingHeader(orphanHeader)) {
            for (const auto& id : m_stats.orphanList) {
                ImGui::PushStyleColor(ImGuiCol_Text, orphanCol);
                if (ImGui::Selectable(("  " + id).c_str(), false)) {
                    app.selectedNoteId = id;
                }
                ImGui::PopStyleColor();
            }
        }
        ImGui::PopStyleColor();
    }

    // Enlaces rotos (expandible)
    if (m_stats.brokenLinks > 0) {
        ImGui::Spacing();
        ImVec4 brokenCol = ImVec4(0.95f, 0.40f, 0.30f, 1.0f);
        char brokenHeader[64];
        std::snprintf(brokenHeader, sizeof(brokenHeader),
                      "Enlaces rotos (%d)###broken", m_stats.brokenLinks);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.12f, 0.10f, 0.5f));
        if (ImGui::CollapsingHeader(brokenHeader)) {
            for (const auto& entry : m_stats.brokenLinkList) {
                ImGui::TextColored(brokenCol, "  %s", entry.c_str());
            }
        }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════════
    // ACTIVIDAD RECIENTE
    // ═══════════════════════════════════════════════════════════════════════════
    ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f), "Actividad Reciente");
    ImGui::Spacing();

    if (m_stats.recentNotes.empty()) {
        ImGui::TextColored(ImVec4(0.40f, 0.42f, 0.52f, 1.0f), "Sin actividad reciente");
    } else {
        for (int i = 0; i < (int)m_stats.recentNotes.size(); ++i) {
            ImGui::PushID(5000 + i);
            const auto& [id, title] = m_stats.recentNotes[i];

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.80f, 0.55f, 1.0f));
            if (ImGui::Selectable(("  " + title).c_str(), false)) {
                app.selectedNoteId = id;
            }
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "%s", title.c_str());
                ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.58f, 1.0f), "ID: %s", id.c_str());
                ImGui::EndTooltip();
            }

            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════════
    // ACCIONES RÁPIDAS
    // ═══════════════════════════════════════════════════════════════════════════
    ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f), "Acciones Rapidas");
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.45f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.18f, 0.58f, 0.52f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.08f, 0.35f, 0.30f, 1.0f));

    if (ImGui::Button("  Recalcular Stats  ")) {
        recalcStats(app);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Forzar recalculo de todas las estadisticas");

    ImGui::SameLine();
    if (ImGui::Button("  Re-indexar Memoria  ")) {
        app.getMemoryEngine().reindex(app.getNoteManager());
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reconstruir el indice TF-IDF del Motor de Memoria");

    ImGui::PopStyleColor(3);

    ImGui::End();
}

} // namespace nodepanda
