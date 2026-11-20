#include "query_panel.h"
#include "query_engine.h"
#include "app.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "imgui.h"
#include <cstring>
#include <sstream>

namespace nodepanda {

void QueryPanel::render(App& app) {
    const auto& pal = UITheme::palette();
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Motor de Consultas", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {
        ui::segmentedControl("##query_mode", &m_modeIdx, {"Dataview SQL", "Datalog"});
        ImGui::Spacing();

        if (m_modeIdx == 0) {
            ImGui::InputTextMultiline("##query", m_queryBuffer, sizeof(m_queryBuffer),
                ImVec2(-1.0f, 92.0f), ImGuiInputTextFlags_AllowTabInput);

            if (ui::primaryButton("Ejecutar")) {
                executeQuery(app);
            }
            ImGui::SameLine();
            if (ui::subtleButton("Limpiar")) {
                m_queryBuffer[0] = '\0';
                m_hasResults = false;
                m_results.clear();
                m_errorMsg.clear();
                m_resultCount = 0;
                m_selectedResultIndex = -1;
            }

            if (!m_errorMsg.empty()) {
                ImGui::TextColored(pal.danger, "Error: %s", m_errorMsg.c_str());
            }

            if (ImGui::CollapsingHeader("Ejemplos")) {
                displayExamples();
            }

            if (m_hasResults) {
                ImGui::Separator();
                ImGui::TextColored(pal.textSecondary, "Resultados (%d)", m_resultCount);

                if (ImGui::BeginTable("##results", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Titulo", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < static_cast<int>(m_results.size()); ++i) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushID(i);
                        if (ImGui::Selectable(m_results[i].first.c_str(), m_selectedResultIndex == i,
                            ImGuiSelectableFlags_SpanAllColumns)) {
                            m_selectedResultIndex = i;
                            app.selectedNoteId = m_results[i].first;
                        }
                        ImGui::PopID();
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(m_results[i].second.c_str());
                    }
                    ImGui::EndTable();
                }
            }
        } else {
            auto& dl = app.getDatalogEngine();
            ImGui::TextColored(pal.textMuted, "Datoms: %zu | Tx: %lld",
                               dl.datomCount(), static_cast<long long>(dl.currentTx()));
            ImGui::InputTextMultiline("##datalog", m_datalogBuffer, sizeof(m_datalogBuffer),
                ImVec2(-1.0f, 92.0f), ImGuiInputTextFlags_AllowTabInput);

            if (ui::primaryButton("Ejecutar Datalog")) {
                executeDatalog(app);
            }
            ImGui::SameLine();
            if (ui::subtleButton("Reindexar")) {
                dl.rebuildIndex(app.getNoteManager());
                ui::pushToast("Indice Datalog reconstruido", "info");
            }

            ImGui::Spacing();
            for (const auto& nq : dl.getNamedQueries()) {
                if (ui::subtleButton(nq.name.c_str())) {
                    std::strncpy(m_datalogBuffer, nq.query.c_str(), sizeof(m_datalogBuffer) - 1);
                    m_datalogBuffer[sizeof(m_datalogBuffer) - 1] = '\0';
                    executeDatalog(app);
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();

            if (!m_errorMsg.empty()) {
                ImGui::TextColored(pal.danger, "Error: %s", m_errorMsg.c_str());
            }

            if (!m_datalogResults.empty()) {
                ImGui::Separator();
                ImGui::TextColored(pal.textSecondary, "Filas (%zu)", m_datalogResults.size());
                std::vector<std::string> cols;
                for (const auto& [k, _] : m_datalogResults[0].bindings) cols.push_back(k);

                if (!cols.empty() && ImGui::BeginTable("##dl_res", static_cast<int>(cols.size()),
                                                       ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {
                    for (const auto& c : cols) ImGui::TableSetupColumn(c.c_str());
                    ImGui::TableHeadersRow();
                    for (size_t r = 0; r < m_datalogResults.size(); ++r) {
                        ImGui::TableNextRow();
                        for (size_t c = 0; c < cols.size(); ++c) {
                            ImGui::TableSetColumnIndex(static_cast<int>(c));
                            auto it = m_datalogResults[r].bindings.find(cols[c]);
                            std::string val = (it != m_datalogResults[r].bindings.end()) ? it->second : "";
                            if (c == 0 && app.getNoteManager().getNoteById(val) != nullptr) {
                                ImGui::PushID(static_cast<int>(r));
                                if (ImGui::Selectable(val.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                    app.selectedNoteId = val;
                                }
                                ImGui::PopID();
                            } else {
                                ImGui::TextUnformatted(val.c_str());
                            }
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
    }
    ImGui::End();
}

void QueryPanel::executeQuery(App& app) {
    m_hasResults = false;
    m_results.clear();
    m_errorMsg.clear();
    m_resultCount = 0;
    m_selectedResultIndex = -1;

    std::string queryStr(m_queryBuffer);
    if (queryStr.empty()) {
        m_errorMsg = "La consulta no puede estar vacia.";
        return;
    }

    try {
        QueryEngine engine;
        QueryDefinition query = engine.parse(queryStr);
        std::vector<QueryResult> results = engine.execute(query, app.getNoteManager(), app.getGraph());
        m_resultCount = static_cast<int>(results.size());
        for (const auto& result : results) {
            std::string displayLine = result.title.empty() ? result.noteId : result.title;
            m_results.emplace_back(result.noteId, displayLine);
        }
        m_hasResults = true;
    } catch (const std::exception& e) {
        m_errorMsg = e.what();
    }
}

void QueryPanel::executeDatalog(App& app) {
    m_errorMsg.clear();
    m_datalogResults = app.getDatalogEngine().executeQuery(m_datalogBuffer, m_errorMsg);
}

void QueryPanel::displayExamples() const {
    const auto& pal = UITheme::palette();
    const char* examples[] = {
        "QUERY notas WHERE tipo = \"proyecto\"",
        "QUERY notas WHERE enlaces > 3 SORT fecha DESC",
        "QUERY notas WHERE palabras >= 100 SORT palabras DESC LIMIT 10",
        "QUERY notas SORT fecha DESC LIMIT 20",
    };

    ImGui::PushStyleColor(ImGuiCol_Text, pal.accent);
    for (const auto& example : examples) {
        ImGui::BulletText("%s", example);
    }
    ImGui::PopStyleColor();
}

std::string QueryPanel::formatResultLine(const std::string& id, const std::string& title) const {
    std::ostringstream oss;
    if (title.empty()) oss << id;
    else oss << title << " (" << id << ")";
    return oss.str();
}

}
