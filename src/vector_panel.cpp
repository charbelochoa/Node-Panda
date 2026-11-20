#include "vector_panel.h"
#include "app.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace nodepanda {

namespace {

void copyToBuffer(char* dst, size_t dstSize, const std::string& src) {
    if (dstSize == 0) return;
    std::strncpy(dst, src.c_str(), dstSize - 1);
    dst[dstSize - 1] = '\0';
}

}

void VectorPanel::syncConfigBuffers(const VectorConfig& cfg) {
    copyToBuffer(m_modelBuf, sizeof(m_modelBuf), cfg.modelId);
    copyToBuffer(m_endpointBuf, sizeof(m_endpointBuf), cfg.apiEndpoint);
    copyToBuffer(m_apiKeyBuf, sizeof(m_apiKeyBuf), cfg.apiKey);
    m_modeIdx = cfg.useRemoteApi ? 1 : 0;
    m_minScore = cfg.minScore;
    m_topK = cfg.topK;
    m_configSynced = true;
}

void VectorPanel::applyConfig(VectorEngine& engine) {
    VectorConfig cfg = engine.config();
    cfg.useRemoteApi = (m_modeIdx == 1);
    cfg.modelId = m_modelBuf;
    cfg.apiEndpoint = m_endpointBuf;
    cfg.apiKey = m_apiKeyBuf;
    cfg.minScore = m_minScore;
    cfg.topK = std::max(1, std::min(100, m_topK));
    engine.configure(cfg);
}

void VectorPanel::renderResultCard(const VectorHit& hit, int index, App& app) {
    const auto& pal = UITheme::palette();

    ImGui::PushID(index);
    char cardId[64];
    std::snprintf(cardId, sizeof(cardId), "##vec_hit_%d", index);

    if (ui::beginCard(cardId, ImVec2(0.0f, 0.0f), true)) {
        const bool isSelected = (!hit.pageId.empty() && app.selectedNoteId == hit.pageId);
        const std::string displayTitle = hit.title.empty() ? hit.pageId : hit.title;

        ImGui::PushStyleColor(ImGuiCol_Text, isSelected ? pal.accent : pal.textPrimary);
        if (ImGui::Selectable(displayTitle.c_str(), isSelected, ImGuiSelectableFlags_None)) {
            if (!hit.pageId.empty()) {
                app.selectedNoteId = hit.pageId;
            }
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (hit.kind == "block") {
            ui::badge("Bloque", pal.bgSurface3, pal.aqua);
        } else {
            ui::badge("Nota", pal.accentSoft, pal.accent);
        }

        char pctBuf[24];
        std::snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", hit.hybridScore * 100.0f);
        float pctW = ImGui::CalcTextSize(pctBuf).x;
        float rightX = ImGui::GetContentRegionMax().x - pctW;
        if (rightX > ImGui::GetCursorPosX()) {
            ImGui::SameLine(rightX);
        } else {
            ImGui::SameLine();
        }

        ImVec4 barColor = pal.warning;
        if (hit.hybridScore >= 0.65f) {
            barColor = pal.success;
        } else if (hit.hybridScore >= 0.35f) {
            barColor = pal.accent;
        }
        ImGui::TextColored(barColor, "%s", pctBuf);

        ui::progressBarModern(hit.hybridScore, barColor, 0.0f, 5.0f);

        ImGui::TextColored(pal.textMuted,
                           "Hibrido: %.2f  |  Vector: %.2f  |  Palabras: %.2f  |  %s",
                           hit.hybridScore, hit.vectorScore, hit.keywordScore, hit.pageId.c_str());

        if (!hit.snippet.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
            ImGui::TextWrapped("%s", hit.snippet.c_str());
            ImGui::PopStyleColor();
        }
    }
    ui::endCard();
    ImGui::Spacing();
    ImGui::PopID();
}

void VectorPanel::render(App& app) {
    ImGui::SetNextWindowSize(ImVec2(820, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Busqueda Vectorial")) {
        ImGui::End();
        return;
    }

    auto& engine = app.getVectorEngine();
    const auto& pal = UITheme::palette();

    if (!m_configSynced) {
        syncConfigBuffers(engine.config());
    }

    const VectorConfig& cfg = engine.config();
    float availW = ImGui::GetContentRegionAvail().x;
    float gap = ImGui::GetStyle().ItemSpacing.x;
    float cardW = std::max(140.0f, (availW - gap * 2.0f) / 3.0f);

    std::string dimLabel = std::to_string(engine.dimension()) + "D";
    std::string activeModelLabel = cfg.useRemoteApi
        ? (cfg.modelId.empty() ? "API Remota" : cfg.modelId)
        : "Local 384D";

    ui::metricCard("##vec_m_docs", "Documentos vectorizados", std::to_string(engine.documentCount()), pal.accent, cardW);
    ImGui::SameLine();
    ui::metricCard("##vec_m_dim", "Dimension", dimLabel, pal.aqua, cardW);
    ImGui::SameLine();
    ui::metricCard("##vec_m_model", "Modelo activo", activeModelLabel, pal.proViolet, cardW);

    ImGui::Spacing();

    if (ui::beginCard("##vec_mode_card", ImVec2(0.0f, 0.0f))) {
        static const std::vector<std::string> modes = {
            "Local 384D",
            "API Embeddings Remota"
        };
        if (ui::segmentedControl("##vec_mode_seg", &m_modeIdx, modes, ImGui::GetContentRegionAvail().x)) {
            applyConfig(engine);
            ui::pushToast(m_modeIdx == 1 ? "Modo API Embeddings activado" : "Modo Local 384D activado", "info");
        }

        if (m_modeIdx == 1) {
            ImGui::Spacing();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.0f);
            ImGui::InputText("Endpoint", m_endpointBuf, sizeof(m_endpointBuf));

            float halfW = std::max(120.0f, (ImGui::GetContentRegionAvail().x - 240.0f) * 0.5f);
            ImGui::SetNextItemWidth(halfW);
            ImGui::InputText("Modelo", m_modelBuf, sizeof(m_modelBuf));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(halfW);
            ImGui::InputText("API Key", m_apiKeyBuf, sizeof(m_apiKeyBuf), ImGuiInputTextFlags_Password);
            ImGui::SameLine();
            if (ui::primaryButton("Aplicar", ImVec2(96.0f, 0.0f))) {
                applyConfig(engine);
                ui::pushToast("Configuracion de embeddings guardada", "success");
            }
        }

        ImGui::Spacing();
        ImGui::SetNextItemWidth(160.0f);
        bool filterChanged = ImGui::SliderFloat("Umbral", &m_minScore, 0.0f, 0.90f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(96.0f);
        filterChanged |= ImGui::InputInt("Top K", &m_topK);
        if (filterChanged) {
            m_topK = std::max(1, std::min(100, m_topK));
            applyConfig(engine);
        }
    }
    ui::endCard();

    ImGui::Spacing();

    float searchBtnW = 104.0f;
    ImGui::SetNextItemWidth(std::max(140.0f, ImGui::GetContentRegionAvail().x - searchBtnW - gap));
    bool enterSearch = ImGui::InputTextWithHint("##vec_query", "Consulta semantica e hibrida...",
                                                m_queryBuf, sizeof(m_queryBuf),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    bool clickSearch = ui::primaryButton("Buscar", ImVec2(searchBtnW, 0.0f));

    if (enterSearch || clickSearch) {
        std::string q(m_queryBuf);
        if (!q.empty()) {
            m_results = engine.searchHybrid(q, m_topK);
            m_lastQuery = q;
            m_hasSearched = true;
            m_showingSimilar = false;
        }
    }

    ImGui::Spacing();

    if (ui::subtleButton("Reconstruir Indice")) {
        engine.rebuildIndex(app.getNoteManager(), app.getOutlinerEngine());
        if (m_showingSimilar && !m_similarSourceId.empty()) {
            m_results = engine.findSimilarToNote(m_similarSourceId, m_topK);
        } else if (m_hasSearched && !m_lastQuery.empty()) {
            m_results = engine.searchHybrid(m_lastQuery, m_topK);
        }
        ui::pushToast("Indice vectorial reconstruido (" + std::to_string(engine.documentCount()) + " docs)", "success");
    }

    ImGui::SameLine();
    if (ui::subtleButton("Similares a Nota Actual")) {
        if (app.selectedNoteId.empty()) {
            ui::pushToast("Selecciona una nota primero", "warn");
        } else {
            if (engine.documentCount() == 0) {
                engine.rebuildIndex(app.getNoteManager(), app.getOutlinerEngine());
            }
            m_results = engine.findSimilarToNote(app.selectedNoteId, m_topK);
            m_similarSourceId = app.selectedNoteId;
            m_showingSimilar = true;
            m_hasSearched = true;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (m_hasSearched) {
        if (m_showingSimilar) {
            ImGui::TextColored(pal.textSecondary, "Similares a %s (%zu)",
                               m_similarSourceId.c_str(), m_results.size());
        } else {
            ImGui::TextColored(pal.textSecondary, "Resultados (%zu)", m_results.size());
        }
        ImGui::Spacing();
    }

    if (ImGui::BeginChild("##vec_results_scroll", ImVec2(0.0f, 0.0f), false)) {
        if (m_results.empty()) {
            if (m_hasSearched) {
                ImGui::TextColored(pal.textMuted, "Sin coincidencias sobre el umbral actual.");
            } else {
                ImGui::TextColored(pal.textMuted, "Escribe una consulta o busca notas similares.");
            }
        } else {
            for (int i = 0; i < static_cast<int>(m_results.size()); ++i) {
                renderResultCard(m_results[static_cast<size_t>(i)], i, app);
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

}
