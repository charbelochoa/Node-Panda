#include "http_panel.h"
#include "app.h"
#include "imgui.h"

namespace nodepanda {

void HttpPanel::render(App& app) {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Servidor API")) {
        ImGui::End();
        return;
    }

    auto& httpServer = app.getHttpServer();
    bool isRunning = httpServer.isRunning();

    ImVec4 statusColor = isRunning ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImGui::TextColored(statusColor, isRunning ? "Estado: ACTIVO" : "Estado: INACTIVO");
    ImGui::SameLine();
    ImGui::TextDisabled("(servidor HTTP)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Puerto: %d", httpServer.getPort());
    ImGui::SameLine();
    ImGui::TextDisabled("(localhost:%d)", httpServer.getPort());

    ImGui::Spacing();

    ImGui::Text("URL Base:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "http://localhost:%d", httpServer.getPort());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!isRunning) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
        if (ImGui::Button("Iniciar Servidor", ImVec2(200, 0))) {
            httpServer.start(&app, httpServer.getPort());
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Button("Detener Servidor", ImVec2(200, 0))) {
            httpServer.stop();
        }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Endpoints Disponibles:");
    ImGui::Spacing();

    if (ImGui::BeginTable("##endpoints", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Endpoint", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Descripción", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "GET /api/notas");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Lista todas las notas");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "GET /api/notas/{id}");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Obtiene una nota por ID");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "GET /api/grafo");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Obtiene nodos y aristas");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "GET /api/memoria/buscar");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Busca notas por query");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "GET /api/stats");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Estadísticas generales");

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Ejemplos de Uso:");
    ImGui::TextDisabled("http://localhost:%d/api/notas", httpServer.getPort());
    ImGui::TextDisabled("http://localhost:%d/api/memoria/buscar?q=python&n=5", httpServer.getPort());

    ImGui::End();
}

}
