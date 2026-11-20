#include "agent_panel.h"
#include "agent_engine.h"
#include "ai_catalog.h"
#include "app.h"
#include "imgui.h"
#include "skill_manager.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "usage_ledger.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace nodepanda {

namespace {

ImVec4 statusColorFor(const std::string& status, const ThemePalette& pal) {
    if (status == "completed") return pal.success;
    if (status == "running") return pal.accent;
    if (status == "stopped") return pal.warning;
    if (status == "failed") return pal.danger;
    return pal.textMuted;
}

}

void AgentPanel::render(App& app) {
    ImGui::SetNextWindowSize(ImVec2(980, 680), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Centro de Agentes IA")) {
        ImGui::End();
        return;
    }

    static const std::vector<std::string> tabs = {
        "Misiones", "Salas", "Contexto", "Skills", "Proveedores", "Consumo"
    };
    ui::segmentedControl("##agent_tabs", &m_activeTab, tabs, ImGui::GetContentRegionAvail().x);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    switch (m_activeTab) {
        case 0: renderMissionsTab(app); break;
        case 1: renderRoomsTab(app); break;
        case 2: renderContextTab(app); break;
        case 3: renderSkillsTab(app); break;
        case 4: renderProvidersTab(app); break;
        case 5: renderUsageTab(app); break;
        default: break;
    }

    ImGui::End();
}

void AgentPanel::renderMissionsTab(App& app) {
    auto& engine = app.getAgentEngine();
    auto& catalog = app.getAiCatalog();
    const auto& pal = UITheme::palette();

    std::vector<std::string> modelChoices;
    modelChoices.push_back("auto");
    for (const auto& m : catalog.models()) {
        modelChoices.push_back(m.id);
    }
    if (m_modelIdx >= static_cast<int>(modelChoices.size())) m_modelIdx = 0;
    if (m_reviewerIdx >= static_cast<int>(modelChoices.size())) m_reviewerIdx = 0;

    if (ui::beginCard("mission_composer", ImVec2(0, 178))) {
        ImGui::InputTextMultiline("##mission_prompt", m_promptBuf, sizeof(m_promptBuf), ImVec2(-1.0f, 76.0f));

        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("Modelo", modelChoices[m_modelIdx].c_str())) {
            for (int i = 0; i < static_cast<int>(modelChoices.size()); ++i) {
                bool sel = (i == m_modelIdx);
                if (ImGui::Selectable(modelChoices[i].c_str(), sel)) m_modelIdx = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::BeginCombo("Revisor", modelChoices[m_reviewerIdx].c_str())) {
            for (int i = 0; i < static_cast<int>(modelChoices.size()); ++i) {
                bool sel = (i == m_reviewerIdx);
                if (ImGui::Selectable(modelChoices[i].c_str(), sel)) m_reviewerIdx = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputText("Sala", m_roomBuf, sizeof(m_roomBuf));

        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputFloat("Presupuesto USD", &m_budgetUsd, 0.05f, 0.25f, "%.2f");
        if (m_budgetUsd < 0.01f) m_budgetUsd = 0.01f;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputInt("Iteraciones", &m_maxIters);
        m_maxIters = std::clamp(m_maxIters, 1, 16);
        ImGui::SameLine();
        if (ui::primaryButton("Iniciar Mision", ImVec2(140.0f, 0.0f))) {
            std::string prompt(m_promptBuf);
            if (!prompt.empty()) {
                AgentRun launched = engine.createAndStartRun(
                    "operator",
                    prompt,
                    modelChoices[m_modelIdx],
                    modelChoices[m_reviewerIdx],
                    m_roomBuf,
                    static_cast<double>(m_budgetUsd),
                    m_maxIters);
                m_selectedRunId = launched.id;
                m_promptBuf[0] = '\0';
                ui::pushToast("Mision iniciada: " + launched.id, "info");
            }
        }
    }
    ui::endCard();

    ImGui::Spacing();
    auto runs = engine.listRuns();
    float availH = std::max(220.0f, ImGui::GetContentRegionAvail().y);

    if (ImGui::BeginTable("##missions_split", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Lista", ImGuiTableColumnFlags_WidthFixed, 270.0f);
        ImGui::TableSetupColumn("Detalle", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("##runs_list", ImVec2(0, availH), true);
        for (int i = static_cast<int>(runs.size()) - 1; i >= 0; --i) {
            const auto& r = runs[i];
            bool selected = (r.id == m_selectedRunId);
            ImGui::PushID(r.id.c_str());
            if (ImGui::Selectable(r.title.c_str(), selected)) {
                m_selectedRunId = r.id;
            }
            ui::badge(r.status.c_str(), statusColorFor(r.status, pal), ImVec4(0.05f, 0.06f, 0.08f, 1.0f));
            ImGui::SameLine();
            ImGui::TextDisabled("%s | %s", r.model.c_str(), AiCatalog::formatUsdFromNano(r.costNano, 4).c_str());
            ImGui::Separator();
            ImGui::PopID();
        }
        if (runs.empty()) {
            ImGui::TextDisabled("Sin misiones ejecutadas.");
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("##run_detail", ImVec2(0, availH), true);
        const AgentRun* selectedRun = nullptr;
        for (const auto& r : runs) {
            if (r.id == m_selectedRunId) {
                selectedRun = &r;
                break;
            }
        }
        if (!selectedRun && !runs.empty()) {
            selectedRun = &runs.back();
            m_selectedRunId = selectedRun->id;
        }

        if (selectedRun) {
            ui::statusDot(selectedRun->status.c_str(), statusColorFor(selectedRun->status, pal), selectedRun->status == "running");
            ImGui::SameLine();
            ImGui::Text("ID: %s | Modelo: %s | Sala: %s", selectedRun->id.c_str(), selectedRun->model.c_str(), selectedRun->room.c_str());
            if (selectedRun->status == "running") {
                ImGui::SameLine();
                if (ui::dangerButton("Detener")) {
                    engine.stopRun(selectedRun->id);
                }
            }
            ImGui::TextDisabled("Tokens in/out: %lld / %lld | Costo: %s | Motivo: %s",
                                static_cast<long long>(selectedRun->usage.input),
                                static_cast<long long>(selectedRun->usage.output),
                                AiCatalog::formatUsdFromNano(selectedRun->costNano, 5).c_str(),
                                selectedRun->stopReason.c_str());

            if (!selectedRun->error.empty()) {
                ImGui::TextColored(pal.danger, "Error: %s", selectedRun->error.c_str());
            }

            if (!selectedRun->iterations.empty()) {
                ui::sectionHeader("Iteraciones");
                if (ImGui::BeginTable("##iter_tbl", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0f);
                    ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 56.0f);
                    ImGui::TableSetupColumn("Tokens", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                    ImGui::TableSetupColumn("Costo", ImGuiTableColumnFlags_WidthFixed, 84.0f);
                    ImGui::TableSetupColumn("Latencia", ImGuiTableColumnFlags_WidthFixed, 74.0f);
                    ImGui::TableSetupColumn("Veredicto", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();
                    for (const auto& it : selectedRun->iterations) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("%d", it.index);
                        ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", it.score);
                        ImGui::TableSetColumnIndex(2); ImGui::Text("%lld", static_cast<long long>(it.tokens));
                        ImGui::TableSetColumnIndex(3); ImGui::Text("%s", AiCatalog::formatUsdFromNano(it.costNano, 5).c_str());
                        ImGui::TableSetColumnIndex(4); ImGui::Text("%lld ms", static_cast<long long>(it.latencyMs));
                        ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(it.verdict.c_str());
                    }
                    ImGui::EndTable();
                }
            }

            if (!selectedRun->steps.empty()) {
                ui::sectionHeader("Pasos");
                for (const auto& st : selectedRun->steps) {
                    ImGui::BulletText("[%s | %s] %s", st.role.c_str(), st.model.c_str(), st.detail.c_str());
                }
            }

            if (!selectedRun->result.empty()) {
                ui::sectionHeader("Resultado");
                ImGui::TextWrapped("%s", selectedRun->result.c_str());
            }
        } else {
            ImGui::TextDisabled("Selecciona o inicia una mision.");
        }

        auto events = engine.listEvents(12);
        if (!events.empty()) {
            ImGui::Spacing();
            ui::sectionHeader("Eventos Recientes");
            for (int i = static_cast<int>(events.size()) - 1; i >= 0; --i) {
                ImGui::TextDisabled("[%s] %s: %s", events[i].createdAt.c_str(), events[i].type.c_str(), events[i].text.c_str());
            }
        }
        ImGui::EndChild();

        ImGui::EndTable();
    }
}

void AgentPanel::renderRoomsTab(App& app) {
    auto& engine = app.getAgentEngine();

    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("Sala Activa", m_chatRoomBuf, sizeof(m_chatRoomBuf));
    ImGui::SameLine();
    if (ui::subtleButton("general")) std::strncpy(m_chatRoomBuf, "general", sizeof(m_chatRoomBuf) - 1);
    ImGui::SameLine();
    if (ui::subtleButton("arquitectura")) std::strncpy(m_chatRoomBuf, "arquitectura", sizeof(m_chatRoomBuf) - 1);
    ImGui::SameLine();
    if (ui::subtleButton("datos")) std::strncpy(m_chatRoomBuf, "datos", sizeof(m_chatRoomBuf) - 1);

    ImGui::Spacing();
    float feedH = std::max(180.0f, ImGui::GetContentRegionAvail().y - 96.0f);
    ImGui::BeginChild("##room_feed", ImVec2(0, feedH), true);
    auto msgs = engine.listMessages(m_chatRoomBuf);
    for (const auto& m : msgs) {
        ImGui::TextColored(UITheme::palette().accent, "%s -> %s", m.from.c_str(), m.to.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", m.createdAt.c_str());
        ImGui::TextWrapped("%s", m.text.c_str());
        ImGui::Separator();
    }
    if (msgs.empty()) {
        ImGui::TextDisabled("Sin mensajes en esta sala.");
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("De", m_chatFromBuf, sizeof(m_chatFromBuf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("Para", m_chatToBuf, sizeof(m_chatToBuf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 95.0f);
    bool submit = ImGui::InputText("##chat_text", m_chatTextBuf, sizeof(m_chatTextBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ui::primaryButton("Enviar", ImVec2(85.0f, 0.0f)) || submit) {
        if (m_chatTextBuf[0] != '\0') {
            engine.postMessage(m_chatRoomBuf, m_chatFromBuf, m_chatToBuf, m_chatTextBuf);
            m_chatTextBuf[0] = '\0';
        }
    }
}

void AgentPanel::renderContextTab(App& app) {
    auto& brain = app.getContextBrain();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 280.0f);
    bool enter = ImGui::InputText("Consulta", m_ctxQueryBuf, sizeof(m_ctxQueryBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Max Tokens", &m_ctxMaxTokens);
    m_ctxMaxTokens = std::clamp(m_ctxMaxTokens, 256, 8192);
    ImGui::SameLine();
    if (ui::primaryButton("Recuperar", ImVec2(110.0f, 0.0f)) || enter) {
        m_lastRetrieval = brain.retrieve(app.getNoteManager(), m_ctxQueryBuf, m_ctxMaxTokens);
        m_hasRetrieval = true;
    }

    if (!m_hasRetrieval) {
        ImGui::Spacing();
        ImGui::TextDisabled("Ejecuta una consulta para recuperar contexto del vault.");
        return;
    }

    ImGui::Spacing();
    ImGui::Text("Metodo: %s | Coincidencias: %zu | Tokens estimados: %d / %d",
                m_lastRetrieval.method.c_str(),
                m_lastRetrieval.hits.size(),
                m_lastRetrieval.estimatedTokens,
                m_lastRetrieval.maxTokens);

    if (ImGui::BeginTable("##ctx_hits", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 210.0f))) {
        ImGui::TableSetupColumn("Nota", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableSetupColumn("Relevancia", ImGuiTableColumnFlags_WidthFixed, 82.0f);
        ImGui::TableSetupColumn("Tokens", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Extracto", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (const auto& h : m_lastRetrieval.hits) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(h.title.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                app.selectedNoteId = h.noteId;
            }
            ImGui::TableSetColumnIndex(1); ImGui::Text("%d", h.relevance);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%d", h.estimatedTokens);
            ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(h.excerpt.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ui::sectionHeader("XML Generado");
    ImGui::BeginChild("##ctx_xml", ImVec2(0, 0), true);
    ImGui::TextWrapped("%s", m_lastRetrieval.formattedXml.c_str());
    ImGui::EndChild();
}

void AgentPanel::renderSkillsTab(App& app) {
    auto& skills = app.getSkillManager();
    const auto& list = skills.getSkills();

    if (ui::subtleButton("Recargar Skills")) {
        skills.scanSkills();
    }
    ImGui::Spacing();

    if (ImGui::BeginTable("##skills_tbl", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 260.0f))) {
        ImGui::TableSetupColumn("Activo", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("Nombre", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableSetupColumn("Descripcion", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Accion", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        std::string toggleId;
        bool toggleVal = false;
        std::string removeId;

        for (const auto& s : list) {
            ImGui::TableNextRow();
            ImGui::PushID(s.id.c_str());
            ImGui::TableSetColumnIndex(0);
            bool en = s.enabled;
            if (ImGui::Checkbox("##en", &en)) {
                toggleId = s.id;
                toggleVal = en;
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(s.name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s", s.description.c_str());
            ImGui::TableSetColumnIndex(3);
            if (!s.builtin) {
                if (ui::dangerButton("Eliminar")) {
                    removeId = s.id;
                }
            } else {
                ImGui::TextDisabled("base");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();

        if (!toggleId.empty()) skills.setEnabled(toggleId, toggleVal);
        if (!removeId.empty()) skills.removeSkill(removeId);
    }

    ImGui::Spacing();
    ui::sectionHeader("Nuevo Skill");
    ImGui::InputText("Nombre##sk", m_newSkillName, sizeof(m_newSkillName));
    ImGui::InputText("Descripcion##sk", m_newSkillDesc, sizeof(m_newSkillDesc));
    ImGui::InputTextMultiline("Instrucciones##sk", m_newSkillContent, sizeof(m_newSkillContent), ImVec2(-1.0f, 96.0f));
    if (ui::primaryButton("Agregar Skill")) {
        if (skills.addSkill(m_newSkillName, m_newSkillDesc, m_newSkillContent)) {
            m_newSkillName[0] = '\0';
            m_newSkillDesc[0] = '\0';
            m_newSkillContent[0] = '\0';
            ui::pushToast("Skill guardado", "success");
        }
    }
}

void AgentPanel::renderProvidersTab(App& app) {
    auto& catalog = app.getAiCatalog();
    auto& cfg = catalog.settings();
    const auto& pal = UITheme::palette();

    if (!m_localEndpointInit) {
        std::strncpy(m_localEndpointBuf, cfg.localEndpoint.c_str(), sizeof(m_localEndpointBuf) - 1);
        m_localEndpointInit = true;
    }

    if (ImGui::BeginTable("##providers_tbl", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Proveedor", ImGuiTableColumnFlags_WidthFixed, 175.0f);
        ImGui::TableSetupColumn("Estado", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Activo", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("Free", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn("API Key / Endpoint", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (auto& p : catalog.providersMutable()) {
            ImGui::TableNextRow();
            ImGui::PushID(p.idStr.c_str());

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s %s", p.asciiBadge.c_str(), p.name.c_str());

            ImGui::TableSetColumnIndex(1);
            bool ready = catalog.hasApiKey(p.id) && p.enabled;
            ui::statusDot(ready ? "Listo" : "Sin Key", ready ? pal.success : pal.warning, false);

            ImGui::TableSetColumnIndex(2);
            ImGui::Checkbox("##en", &p.enabled);

            ImGui::TableSetColumnIndex(3);
            ImGui::Checkbox("##ft", &p.freeTier);

            ImGui::TableSetColumnIndex(4);
            if (p.id == ProviderId::Local) {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 86.0f);
                ImGui::InputText("##local_ep", m_localEndpointBuf, sizeof(m_localEndpointBuf));
                ImGui::SameLine();
                if (ui::subtleButton("Aplicar")) {
                    cfg.localEndpoint = m_localEndpointBuf;
                    p.endpoint = cfg.localEndpoint;
                    catalog.saveConfig();
                    ui::pushToast("Endpoint local actualizado", "success");
                }
            } else {
                std::string& keyBuf = m_keyInputs[p.idStr];
                char tmp[256] = {};
                std::strncpy(tmp, keyBuf.c_str(), sizeof(tmp) - 1);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 86.0f);
                if (ImGui::InputText("##apikey", tmp, sizeof(tmp), ImGuiInputTextFlags_Password)) {
                    keyBuf = tmp;
                }
                ImGui::SameLine();
                if (ui::subtleButton("Guardar")) {
                    catalog.setApiKey(p.id, keyBuf);
                    catalog.saveConfig();
                    keyBuf.clear();
                    ui::pushToast("Clave guardada para " + p.name, "success");
                }
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ui::sectionHeader("Politica del Agente");
    ImGui::Checkbox("Router Automatico", &cfg.router);
    ImGui::SameLine();
    ImGui::Checkbox("Optimizador Iterativo", &cfg.optimizer);
    ImGui::SameLine();
    ImGui::Checkbox("Permitir Escrituras", &cfg.allowWrites);
    ImGui::SameLine();
    ImGui::Checkbox("Permitir Comandos", &cfg.allowCommands);

    float b = static_cast<float>(cfg.budgetUsd);
    float t = static_cast<float>(cfg.targetScore);
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::InputFloat("Presupuesto Default", &b, 0.05f, 0.25f, "%.2f")) cfg.budgetUsd = std::max(0.01, static_cast<double>(b));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::InputFloat("Score Objetivo", &t, 1.0f, 5.0f, "%.1f")) cfg.targetScore = std::clamp(static_cast<double>(t), 10.0, 100.0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::InputInt("Max Iteraciones", &cfg.maxIterations);
    cfg.maxIterations = std::clamp(cfg.maxIterations, 1, 16);

    if (ui::primaryButton("Guardar Configuracion")) {
        catalog.saveConfig();
        ui::pushToast("Configuracion persistida", "success");
    }
}

void AgentPanel::renderUsageTab(App& app) {
    auto& ledger = app.getUsageLedger();
    const auto& pal = UITheme::palette();

    float cardW = (ImGui::GetContentRegionAvail().x - 24.0f) / 3.0f;
    ui::metricCard("cost_card", "Costo Total", AiCatalog::formatUsdFromNano(ledger.totalCostNano(), 5), pal.accent, cardW);
    ImGui::SameLine();
    ui::metricCard("tok_card", "Tokens Totales", std::to_string(ledger.totalTokens()), pal.aqua, cardW);
    ImGui::SameLine();
    ui::metricCard("calls_card", "Llamadas", std::to_string(ledger.entries().size()), pal.proViolet, cardW);

    ImGui::Spacing();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
    ImGui::InputText("Archivo CSV", m_exportCsvPath, sizeof(m_exportCsvPath));
    ImGui::SameLine();
    if (ui::primaryButton("Exportar CSV", ImVec2(125.0f, 0.0f))) {
        if (ledger.exportCsv(m_exportCsvPath)) {
            ui::pushToast("CSV exportado: " + std::string(m_exportCsvPath), "success");
        } else {
            ui::pushToast("Error al exportar CSV", "error");
        }
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("##usage_tbl", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Fecha", ImGuiTableColumnFlags_WidthFixed, 155.0f);
        ImGui::TableSetupColumn("Proveedor", ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn("Modelo", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("In", ImGuiTableColumnFlags_WidthFixed, 68.0f);
        ImGui::TableSetupColumn("Out", ImGuiTableColumnFlags_WidthFixed, 68.0f);
        ImGui::TableSetupColumn("Costo", ImGuiTableColumnFlags_WidthFixed, 86.0f);
        ImGui::TableSetupColumn("Latencia", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableHeadersRow();

        const auto& items = ledger.entries();
        for (int i = static_cast<int>(items.size()) - 1; i >= 0; --i) {
            const auto& e = items[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(e.createdAt.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(e.provider.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(e.actualModel.c_str());
            ImGui::TableSetColumnIndex(3); ImGui::Text("%lld", static_cast<long long>(e.usage.input));
            ImGui::TableSetColumnIndex(4); ImGui::Text("%lld", static_cast<long long>(e.usage.output));
            ImGui::TableSetColumnIndex(5); ImGui::Text("%s", AiCatalog::formatUsdFromNano(e.costNano, 5).c_str());
            ImGui::TableSetColumnIndex(6); ImGui::Text("%lld ms", static_cast<long long>(e.latencyMs));
        }
        ImGui::EndTable();
    }
}

}
