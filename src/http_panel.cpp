#include "http_panel.h"
#include "app.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "imgui.h"

namespace nodepanda {

void HttpPanel::render(App& app) {
    const auto& pal = UITheme::palette();
    ImGui::SetNextWindowSize(ImVec2(640, 460), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Servidor API")) {
        ImGui::End();
        return;
    }

    auto& httpServer = app.getHttpServer();
    bool isRunning = httpServer.isRunning();

    ui::statusDot(isRunning ? "Servidor activo" : "Servidor detenido",
                  isRunning ? pal.success : pal.danger, isRunning);
    ImGui::SameLine(0.0f, 12.0f);
    char urlBuf[128];
    std::snprintf(urlBuf, sizeof(urlBuf), "http://%s:%d",
                  httpServer.getBindHost().c_str(), httpServer.getPort());
    ui::badge(urlBuf, pal.accentSoft, pal.accent);

    ImGui::Spacing();
    if (!isRunning) {
        if (ui::primaryButton("Iniciar Servidor", ImVec2(180, 0))) {
            httpServer.start(&app, httpServer.getPort());
        }
    } else {
        if (ui::dangerButton("Detener Servidor", ImVec2(180, 0))) {
            httpServer.stop();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* rows[][2] = {
        {"GET /api/health", "Estado del servicio"},
        {"GET /api/snapshot", "Resumen global del sistema"},
        {"GET /api/notas", "Lista todas las notas"},
        {"GET /api/notas/{id}", "Contenido completo de nota"},
        {"GET /api/grafo", "Nodos y aristas del grafo"},
        {"GET /api/memoria/buscar?q=...", "Busqueda semantica TF-IDF"},
        {"GET /api/v1/blocks", "Bloques jerarquicos del outliner"},
        {"POST /api/v1/blocks", "Crear bloque en una pagina"},
        {"GET /api/v1/tasks", "Lista de tareas"},
        {"POST /api/v1/datalog", "Ejecutar consulta Datalog"},
        {"GET /api/v1/sync", "Estado y checksum de sincronizacion"},
        {"GET /api/runs", "Misiones de agentes IA"},
        {"POST /api/runs", "Lanzar mision multi-agente"},
        {"GET /api/messages?room=...", "Mensajes de sala de agentes"},
        {"POST /api/messages", "Enviar mensaje a sala"},
        {"GET /api/skills", "Skills activas"},
        {"POST /api/context/retrieve", "Recuperacion de contexto"},
        {"GET /api/db/status", "Estado de PostgreSQL / MySQL"},
        {"POST /api/auth/login", "Autenticacion con token"}
    };

    if (ImGui::BeginTable("##endpoints", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Endpoint", ImGuiTableColumnFlags_WidthStretch, 0.48f);
        ImGui::TableSetupColumn("Funcion", ImGuiTableColumnFlags_WidthStretch, 0.52f);
        ImGui::TableHeadersRow();
        for (const auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(pal.accent, "%s", r[0]);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(r[1]);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

}
