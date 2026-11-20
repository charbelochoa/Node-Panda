#include "analytics_panel.h"
#include "app.h"
#include "graph.h"
#include "imgui.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace nodepanda {

void AnalyticsPanel::render(App& app) {
    const auto& graph = app.getGraph();
    int currentNodeCount = graph.getNodes().size();

    if (currentNodeCount != m_cachedNodeCount) {
        m_analytics.analyze(graph);
        m_cachedNodeCount = currentNodeCount;
    }

    if (!ImGui::Begin("Analisis de Grafo")) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Recalcular", ImVec2(120, 0))) {
        m_analytics.analyze(graph);
        m_cachedNodeCount = currentNodeCount;
    }

    if (!m_analytics.isReady()) {
        ImGui::TextWrapped("Calculando metricas...");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("AnalyticsTabBar")) {

        if (ImGui::BeginTabItem("PageRank")) {
            ImGui::Text("Top 15 Nodos por PageRank");
            ImGui::Separator();

            auto topNodes = m_analytics.topByPageRank(15);
            int rank = 1;

            for (const auto& metric : topNodes) {
                ImGui::PushID(rank);

                std::ostringstream oss;
                oss << std::fixed << std::setprecision(4) << metric.pageRank;
                std::string scoreStr = oss.str();

                if (ImGui::Selectable(
                    (std::to_string(rank) + ". " + metric.noteId).c_str(),
                    app.selectedNoteId == metric.noteId)) {
                    app.selectedNoteId = metric.noteId;
                }

                ImGui::SameLine();
                ImGui::ProgressBar(metric.pageRank, ImVec2(100, 0), scoreStr.c_str());

                ImGui::PopID();
                rank++;
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Centralidad")) {
            ImGui::Text("Top 15 Nodos por Centralidad");
            ImGui::Separator();

            auto topNodes = m_analytics.topByBetweenness(15);
            int rank = 1;

            for (const auto& metric : topNodes) {
                ImGui::PushID(rank);

                std::ostringstream oss;
                oss << std::fixed << std::setprecision(4) << metric.betweenness;
                std::string scoreStr = oss.str();

                if (ImGui::Selectable(
                    (std::to_string(rank) + ". " + metric.noteId).c_str(),
                    app.selectedNoteId == metric.noteId)) {
                    app.selectedNoteId = metric.noteId;
                }

                ImGui::SameLine();
                ImGui::ProgressBar(metric.betweenness, ImVec2(100, 0), scoreStr.c_str());

                ImGui::PopID();
                rank++;
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Comunidades")) {
            int communityCount = m_analytics.communityCount();
            ImGui::Text("Total de Comunidades: %d", communityCount);
            ImGui::Separator();

            for (int comId = 0; comId < communityCount; ++comId) {
                auto members = m_analytics.getCommunityMembers(comId);

                std::string headerLabel = "Comunidad " + std::to_string(comId) +
                                         " (" + std::to_string(members.size()) + " miembros)";

                if (ImGui::CollapsingHeader(headerLabel.c_str())) {
                    for (const auto& noteId : members) {
                        ImGui::PushID(noteId.c_str());

                        if (ImGui::Selectable(noteId.c_str(),
                            app.selectedNoteId == noteId)) {
                            app.selectedNoteId = noteId;
                        }

                        ImGui::PopID();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

}
