#include "query_panel.h"
#include "query_engine.h"
#include "app.h"
#include "imgui.h"
#include <sstream>

namespace nodepanda {

void QueryPanel::render(App& app) {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Motor de Consultas", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {

        ImGui::TextWrapped("Ingrese una consulta Dataview:");
        ImGui::InputTextMultiline("##query", m_queryBuffer, sizeof(m_queryBuffer),
            ImVec2(-1.0f, 100.0f), ImGuiInputTextFlags_AllowTabInput);

        if (ImGui::Button("Ejecutar")) {
            executeQuery(app);
        }

        ImGui::SameLine();
        if (ImGui::Button("Limpiar")) {
            m_queryBuffer[0] = '\0';
            m_hasResults = false;
            m_results.clear();
            m_errorMsg.clear();
            m_resultCount = 0;
            m_selectedResultIndex = -1;
        }

        if (!m_errorMsg.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", m_errorMsg.c_str());
        }

        if (ImGui::CollapsingHeader("Ejemplos", ImGuiTreeNodeFlags_DefaultOpen)) {
            displayExamples();
        }

        if (m_hasResults) {
            ImGui::Separator();
            ImGui::Text("Resultados (%d):", m_resultCount);

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
                    ImGui::Text("%s", m_results[i].second.c_str());
                }

                ImGui::EndTable();
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
        m_errorMsg = "Query cannot be empty";
        return;
    }

    try {
        QueryEngine engine;
        QueryDefinition query = engine.parse(queryStr);
        std::vector<QueryResult> results = engine.execute(query, app.getNoteManager(), app.getGraph());

        m_resultCount = static_cast<int>(results.size());
        for (const auto& result : results) {
            std::string displayLine = result.title;
            if (displayLine.empty()) {
                displayLine = "(untitled)";
            }
            m_results.emplace_back(result.noteId, displayLine);
        }

        m_hasResults = true;
    } catch (const std::exception& e) {
        m_errorMsg = std::string(e.what());
    }
}

void QueryPanel::displayExamples() const {
    ImGui::Text("Ejemplos de consultas:");

    const char* examples[] = {
        "QUERY notas WHERE tipo = \"proyecto\"",
        "QUERY notas WHERE enlaces > 3 SORT fecha DESC",
        "QUERY notas WHERE titulo CONTIENE \"API\" AND tipo = \"tecnico\"",
        "QUERY notas WHERE palabras >= 100 SORT palabras DESC LIMIT 10",
        "QUERY notas WHERE conexiones >= 5 AND tags CONTIENE \"importante\"",
        "QUERY notas SORT fecha DESC LIMIT 20",
    };

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 1.0f, 1.0f));
    for (const auto& example : examples) {
        ImGui::BulletText("%s", example);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::TextWrapped("Campos disponibles: titulo, id, tipo, tags, palabras, caracteres, enlaces, conexiones, fecha");
    ImGui::TextWrapped("Operadores: =, !=, >, <, >=, <=, CONTIENE, NO_CONTIENE");
    ImGui::TextWrapped("Modificadores: WHERE [conditions AND ...], SORT [field ASC|DESC], LIMIT [n]");
}

std::string QueryPanel::formatResultLine(const std::string& id, const std::string& title) const {
    std::ostringstream oss;
    if (title.empty()) {
        oss << id;
    } else {
        oss << title << " (" << id << ")";
    }
    return oss.str();
}

}
