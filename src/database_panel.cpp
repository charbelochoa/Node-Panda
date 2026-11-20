#include "database_panel.h"
#include "app.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "imgui.h"

#include <cstring>

namespace nodepanda {

void DatabasePanel::render(App& app) {
    auto& db = app.getDatabaseManager();
    const auto& pal = UITheme::palette();

    if (!m_initialized) {
        m_initialized = true;
        const auto& cfg = db.config();
        m_engineIdx = (cfg.engine == DbEngineType::LocalOnly) ? 0
                    : (cfg.engine == DbEngineType::PostgreSQL) ? 1 : 2;
        m_modeIdx = (cfg.deployMode == DbDeployMode::Server) ? 1 : 0;
        std::strncpy(m_host, cfg.host.c_str(), sizeof(m_host) - 1);
        m_port = cfg.port;
        std::strncpy(m_database, cfg.database.c_str(), sizeof(m_database) - 1);
        std::strncpy(m_user, cfg.username.c_str(), sizeof(m_user) - 1);
        std::strncpy(m_password, cfg.password.c_str(), sizeof(m_password) - 1);
        m_autoSync = cfg.autoSync;
    }

    ImGui::Begin("Base de Datos", nullptr, ImGuiWindowFlags_NoCollapse);

    auto status = db.getStatus();
    ui::statusDot(status.connected ? "Conectado" : "Sin conexion",
                  status.connected ? pal.success : pal.warning,
                  status.connected);
    ImGui::SameLine(0.0f, 12.0f);
    ui::badge(status.engineName.c_str(), pal.accentSoft, pal.accent);
    ImGui::SameLine(0.0f, 6.0f);
    ui::badge(status.modeName.c_str(), pal.bgSurface3, pal.textSecondary);

    ImGui::Spacing();

    int prevEngine = m_engineIdx;
    if (ui::segmentedControl("##db_engine", &m_engineIdx, {"Local", "PostgreSQL", "MySQL"})) {
        if (prevEngine != m_engineIdx) {
            if (m_engineIdx == 1) m_port = 5432;
            else if (m_engineIdx == 2) m_port = 3306;
        }
    }
    ImGui::Spacing();
    ui::segmentedControl("##db_mode", &m_modeIdx, {"Modo Local", "Modo Servidor"});

    ImGui::Spacing();
    if (ui::beginCard("##db_cfg_card", ImVec2(0, 0))) {
        ui::sectionHeader("Conexion");
        ImGui::InputText("Host", m_host, sizeof(m_host));
        ImGui::InputInt("Puerto", &m_port);
        ImGui::InputText("Base de datos", m_database, sizeof(m_database));
        ImGui::InputText("Usuario", m_user, sizeof(m_user));
        ImGui::InputText("Contrasena", m_password, sizeof(m_password), ImGuiInputTextFlags_Password);
        ImGui::Checkbox("Sincronizacion automatica", &m_autoSync);

        ImGui::Spacing();
        if (ui::primaryButton("Guardar y Probar")) {
            DbConfig cfg;
            cfg.engine = (m_engineIdx == 0) ? DbEngineType::LocalOnly
                       : (m_engineIdx == 1) ? DbEngineType::PostgreSQL
                       : DbEngineType::MySQL;
            cfg.deployMode = (m_modeIdx == 1) ? DbDeployMode::Server : DbDeployMode::Local;
            cfg.host = m_host;
            cfg.port = m_port;
            cfg.database = m_database;
            cfg.username = m_user;
            cfg.password = m_password;
            cfg.autoSync = m_autoSync;
            db.configure(cfg);
            std::string details;
            bool ok = db.testConnection(details);
            m_statusMessage = details;
            ui::pushToast(details, ok ? "success" : "warn");
        }
        ImGui::SameLine();
        if (ui::subtleButton("Inicializar Tablas")) {
            std::string err;
            if (db.initializeSchema(err)) {
                m_statusMessage = "Esquema SQL inicializado correctamente.";
                ui::pushToast(m_statusMessage, "success");
            } else {
                m_statusMessage = err;
                ui::pushToast(err, "error");
            }
        }
        ImGui::SameLine();
        if (ui::subtleButton("Sincronizar Notas")) {
            std::string summary;
            bool ok = db.syncNotes(app.getNoteManager(), summary);
            m_statusMessage = summary;
            ui::pushToast(summary, ok ? "success" : "warn");
        }
    }
    ui::endCard();

    if (!m_statusMessage.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(pal.textSecondary, "%s", m_statusMessage.c_str());
    }

    ImGui::Spacing();
    if (ui::beginCard("##db_sql_card", ImVec2(0, 0))) {
        ui::sectionHeader("Consola SQL");
        ImGui::InputTextMultiline("##sql_query", m_sqlInput, sizeof(m_sqlInput), ImVec2(-1.0f, 84.0f));
        if (ui::primaryButton("Ejecutar SQL")) {
            m_lastQueryResult = db.executeSql(m_sqlInput);
            if (!m_lastQueryResult.ok) {
                ui::pushToast(m_lastQueryResult.error, "error");
            }
        }
        if (m_lastQueryResult.ok && !m_lastQueryResult.columns.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(pal.textMuted, "%.1f ms | %d filas",
                               m_lastQueryResult.latencyMs,
                               static_cast<int>(m_lastQueryResult.rows.size()));
            if (ImGui::BeginTable("##db_res_tbl", static_cast<int>(m_lastQueryResult.columns.size()),
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {
                for (const auto& col : m_lastQueryResult.columns) {
                    ImGui::TableSetupColumn(col.c_str());
                }
                ImGui::TableHeadersRow();
                for (const auto& row : m_lastQueryResult.rows) {
                    ImGui::TableNextRow();
                    for (size_t i = 0; i < row.size() && i < m_lastQueryResult.columns.size(); ++i) {
                        ImGui::TableSetColumnIndex(static_cast<int>(i));
                        ImGui::TextUnformatted(row[i].c_str());
                    }
                }
                ImGui::EndTable();
            }
        } else if (!m_lastQueryResult.error.empty()) {
            ImGui::TextColored(pal.danger, "%s", m_lastQueryResult.error.c_str());
        }
    }
    ui::endCard();

    ImGui::End();
}

}
