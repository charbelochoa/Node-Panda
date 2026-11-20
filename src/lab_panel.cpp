#include "lab_panel.h"
#include "app.h"
#include "evolution_engine.h"
#include "imgui.h"
#include "ui_components.h"
#include "ui_theme.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

namespace nodepanda {

LabPanel::LabPanel() {
    m_cadLogits = {
        0.65f, -0.40f, 1.15f, 0.25f, -1.20f, -0.85f, -0.30f,
        -1.50f, 0.45f, -0.70f, -1.10f, -0.95f, -0.55f, -2.10f
    };
}

void LabPanel::render(App& app) {
    ImGui::SetNextWindowSize(ImVec2(960, 660), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Laboratorio de Datos y Evolucion")) {
        ImGui::End();
        return;
    }

    static const std::vector<std::string> tabs = {
        "Tabular", "Documentos", "Evaluacion CAD", "Evolucion"
    };
    ui::segmentedControl("##lab_tabs", &m_activeTab, tabs, ImGui::GetContentRegionAvail().x);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    switch (m_activeTab) {
        case 0: renderTabularTab(app); break;
        case 1: renderDocumentsTab(app); break;
        case 2: renderCadTab(app); break;
        case 3: renderEvolutionTab(app); break;
        default: break;
    }

    ImGui::End();
}

void LabPanel::renderTabularTab(App& app) {
    auto& lab = app.getDataLab();
    const auto& pal = UITheme::palette();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 135.0f);
    ImGui::InputText("Ruta CSV", m_csvPathBuf, sizeof(m_csvPathBuf));
    ImGui::SameLine();
    if (ui::primaryButton("Perfilar CSV", ImVec2(120.0f, 0.0f))) {
        m_profile = lab.profileCsv(m_csvPathBuf);
        m_hasProfile = true;
        if (!m_profile.ok) {
            ui::pushToast(m_profile.error, "error");
        }
    }

    if (m_hasProfile) {
        if (!m_profile.ok) {
            ImGui::TextColored(pal.danger, "%s", m_profile.error.c_str());
        } else {
            ImGui::Text("Filas: %d | Columnas: %d | SHA-256: %.16s...",
                        m_profile.rowCount, m_profile.colCount, m_profile.sha256.c_str());
            if (ImGui::BeginTable("##csv_profile", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 145.0f))) {
                ImGui::TableSetupColumn("Columna", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Tipo", ImGuiTableColumnFlags_WidthFixed, 76.0f);
                ImGui::TableSetupColumn("No Nulos", ImGuiTableColumnFlags_WidthFixed, 72.0f);
                ImGui::TableSetupColumn("Nulos", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 86.0f);
                ImGui::TableHeadersRow();
                for (const auto& c : m_profile.columns) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(c.name.c_str());
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(c.inferredType.c_str());
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%d", c.nonEmpty);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%d", c.nullCount);
                    ImGui::TableSetColumnIndex(4);
                    if (c.inferredType == "number") ImGui::Text("%.2f", c.minVal);
                    else ImGui::TextDisabled("-");
                    ImGui::TableSetColumnIndex(5);
                    if (c.inferredType == "number") ImGui::Text("%.2f", c.maxVal);
                    else ImGui::TextDisabled("-");
                }
                ImGui::EndTable();
            }
        }
    }

    ImGui::Spacing();
    ui::sectionHeader("Agregacion y Grafico");
    static const char* aggItems[] = { "count", "sum", "avg", "min", "max" };
    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("Funcion", &m_aggIdx, aggItems, 5);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputText("Metrica", m_metricColBuf, sizeof(m_metricColBuf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputText("Agrupar Por", m_groupColBuf, sizeof(m_groupColBuf));
    ImGui::SameLine();
    if (ui::primaryButton("Analizar", ImVec2(105.0f, 0.0f))) {
        AnalysisPlan plan;
        plan.aggregation = aggItems[m_aggIdx];
        plan.metric = m_metricColBuf;
        plan.groupBy = m_groupColBuf;
        m_artifact = lab.analyzeCsv(m_csvPathBuf, plan);
        m_hasArtifact = true;
        if (!m_artifact.ok) ui::pushToast(m_artifact.error, "error");
    }

    if (m_hasArtifact && m_artifact.ok) {
        ImGui::Spacing();
        ImGui::Text("Total: %.4f | Filas procesadas: %d | Grupos: %zu",
                    m_artifact.totalValue, m_artifact.processedRows, m_artifact.points.size());

        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        float canvasW = std::max(260.0f, ImGui::GetContentRegionAvail().x);
        float canvasH = 170.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasW, canvasPos.y + canvasH),
                          ImGui::ColorConvertFloat4ToU32(pal.bgSurface2), 6.0f);
        dl->AddRect(canvasPos, ImVec2(canvasPos.x + canvasW, canvasPos.y + canvasH),
                    ImGui::ColorConvertFloat4ToU32(pal.border), 6.0f);

        size_t n = std::min<size_t>(m_artifact.points.size(), 14);
        double maxV = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (m_artifact.points[i].value > maxV) maxV = m_artifact.points[i].value;
        }
        if (maxV <= 0.0) maxV = 1.0;

        float padX = 16.0f;
        float padTop = 14.0f;
        float padBottom = 26.0f;
        float innerW = canvasW - padX * 2.0f;
        float innerH = canvasH - padTop - padBottom;

        if (n > 0) {
            float slot = innerW / static_cast<float>(n);
            float barW = std::max(10.0f, slot * 0.65f);
            ImU32 barCol = ImGui::ColorConvertFloat4ToU32(pal.accent);
            ImU32 txtCol = ImGui::ColorConvertFloat4ToU32(pal.textSecondary);
            for (size_t i = 0; i < n; ++i) {
                float frac = static_cast<float>(std::max(0.0, m_artifact.points[i].value / maxV));
                float bh = frac * innerH;
                float x0 = canvasPos.x + padX + slot * static_cast<float>(i) + (slot - barW) * 0.5f;
                float y1 = canvasPos.y + padTop + innerH;
                float y0 = y1 - bh;
                dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + barW, y1), barCol, 3.0f);
                std::string lbl = m_artifact.points[i].label;
                if (lbl.size() > 8) lbl = lbl.substr(0, 7) + ".";
                dl->AddText(ImVec2(x0, y1 + 5.0f), txtCol, lbl.c_str());
            }
        }
        ImGui::Dummy(ImVec2(canvasW, canvasH));

        ImGui::Spacing();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
        ImGui::InputText("Salida SVG", m_svgOutPathBuf, sizeof(m_svgOutPathBuf));
        ImGui::SameLine();
        if (ui::subtleButton("Exportar SVG", ImVec2(125.0f, 0.0f))) {
            std::string svg = lab.renderChartSvg(m_artifact, "bar");
            std::ofstream out(m_svgOutPathBuf, std::ios::trunc);
            if (out.is_open()) {
                out << svg;
                ui::pushToast("SVG exportado", "success");
            } else {
                ui::pushToast("No se pudo escribir SVG", "error");
            }
        }
    }
}

void LabPanel::renderDocumentsTab(App& app) {
    auto& lab = app.getDataLab();

    ImGui::InputText("Archivo", m_docPathBuf, sizeof(m_docPathBuf));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 125.0f);
    bool enter = ImGui::InputText("Termino", m_docQueryBuf, sizeof(m_docQueryBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ui::primaryButton("Buscar", ImVec2(110.0f, 0.0f)) || enter) {
        m_docHits = lab.searchDocument(m_docPathBuf, m_docQueryBuf);
    }

    ImGui::Spacing();
    ImGui::Text("Coincidencias: %zu", m_docHits.size());
    if (ImGui::BeginTable("##doc_hits", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Linea", ImGuiTableColumnFlags_WidthFixed, 68.0f);
        ImGui::TableSetupColumn("Extracto", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (const auto& h : m_docHits) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", h.lineNumber);
            ImGui::TableSetColumnIndex(1); ImGui::TextWrapped("%s", h.excerpt.c_str());
        }
        ImGui::EndTable();
    }
}

void LabPanel::renderCadTab(App& app) {
    auto& lab = app.getDataLab();
    const auto& pal = UITheme::palette();

    static const char* pathologyNames[14] = {
        "Atelectasis", "Cardiomegaly", "Effusion", "Infiltration",
        "Mass", "Nodule", "Pneumonia", "Pneumothorax",
        "Consolidation", "Edema", "Emphysema", "Fibrosis",
        "Pleural_Thickening", "Hernia"
    };

    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Estudio", m_cadLabelBuf, sizeof(m_cadLabelBuf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    ImGui::SliderFloat("Umbral", &m_cadThreshold, 0.05f, 0.95f, "%.2f");
    ImGui::SameLine();
    if (ui::primaryButton("Evaluar 14 Clases")) {
        std::vector<double> logits(m_cadLogits.begin(), m_cadLogits.end());
        m_cadReport = lab.evaluateCadMetrics(m_cadLabelBuf, logits, static_cast<double>(m_cadThreshold));
        m_hasCadReport = true;
    }

    if (ImGui::CollapsingHeader("Logits de Entrada (14 Clases)")) {
        if (ImGui::BeginTable("##cad_logits_grid", 2, ImGuiTableFlags_None)) {
            for (int i = 0; i < 14; ++i) {
                if (i % 2 == 0) ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(i % 2);
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(160.0f);
                ImGui::SliderFloat(pathologyNames[i], &m_cadLogits[i], -4.0f, 4.0f, "%.2f");
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    if (!m_hasCadReport) {
        std::vector<double> logits(m_cadLogits.begin(), m_cadLogits.end());
        m_cadReport = lab.evaluateCadMetrics(m_cadLabelBuf, logits, static_cast<double>(m_cadThreshold));
        m_hasCadReport = true;
    }

    int positives = 0;
    for (const auto& f : m_cadReport.findings) {
        if (f.positive) ++positives;
    }

    ImGui::Spacing();
    float cw = (ImGui::GetContentRegionAvail().x - 16.0f) / 2.0f;
    char aucBuf[32];
    std::snprintf(aucBuf, sizeof(aucBuf), "%.4f", m_cadReport.meanAucEstimate);
    ui::metricCard("cad_auc", "AUC Estimado", aucBuf, pal.aqua, cw);
    ImGui::SameLine();
    ui::metricCard("cad_pos", "Hallazgos Positivos", std::to_string(positives) + " / 14", positives > 0 ? pal.warning : pal.success, cw);

    ImGui::Spacing();
    if (ImGui::BeginTable("##cad_findings", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Patologia", ImGuiTableColumnFlags_WidthFixed, 175.0f);
        ImGui::TableSetupColumn("Probabilidad", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Distribucion", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Estado", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableHeadersRow();
        for (const auto& f : m_cadReport.findings) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(f.pathology.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f%%", f.probability * 100.0);
            ImGui::TableSetColumnIndex(2);
            ui::progressBarModern(static_cast<float>(f.probability), f.positive ? pal.warning : pal.accent, -1.0f, 8.0f);
            ImGui::TableSetColumnIndex(3);
            ui::badge(f.positive ? "POSITIVO" : "NEGATIVO", f.positive ? pal.warning : pal.bgSurface3,
                      f.positive ? ImVec4(0.05f, 0.06f, 0.08f, 1.0f) : pal.textSecondary);
        }
        ImGui::EndTable();
    }
}

void LabPanel::renderEvolutionTab(App& app) {
    auto& evo = app.getEvolutionEngine();
    auto& lab = app.getDataLab();
    const auto& pal = UITheme::palette();

    float cw = (ImGui::GetContentRegionAvail().x - 24.0f) / 3.0f;
    ui::metricCard("evo_ver", "Version Activa", evo.activeVersionId(), pal.accent, cw);
    ImGui::SameLine();
    ui::metricCard("evo_cases", "Casos", std::to_string(evo.cases().size()), pal.aqua, cw);
    ImGui::SameLine();
    ui::metricCard("evo_trials", "Pruebas", std::to_string(evo.trials().size()), pal.proViolet, cw);

    ImGui::Spacing();
    static const char* modes[] = { "adaptive", "aggressive", "conservative" };
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("Modo", &m_optModeIdx, modes, 3);
    ImGui::SameLine();
    if (ui::primaryButton("Proponer Optimizacion")) {
        std::string cand = evo.proposeOptimization(modes[m_optModeIdx]);
        m_evoStatus = "Candidata propuesta: " + cand;
    }
    ImGui::SameLine();
    if (ui::subtleButton("Decidir Promocion")) {
        m_evoStatus = "Decision: " + evo.decidePromotion();
    }
    if (!m_evoStatus.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(pal.accent, "%s", m_evoStatus.c_str());
    }

    ImGui::Spacing();
    ui::sectionHeader("Agregar Caso de Benchmark");
    ImGui::SetNextItemWidth(230.0f);
    ImGui::InputText("Prompt##bc", m_casePromptBuf, sizeof(m_casePromptBuf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(170.0f);
    ImGui::InputText("CSV##bc", m_caseFileBuf, sizeof(m_caseFileBuf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(95.0f);
    ImGui::InputText("Split##bc", m_caseSplitBuf, sizeof(m_caseSplitBuf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(95.0f);
    ImGui::InputFloat("Esperado", &m_caseExpected, 1.0f, 10.0f, "%.2f");
    ImGui::SameLine();
    if (ui::primaryButton("Agregar")) {
        evo.addCase(m_casePromptBuf, m_caseFileBuf, m_caseSplitBuf, static_cast<double>(m_caseExpected));
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("##evo_cases_tbl", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 140.0f))) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupColumn("Prompt", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Split", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Esperado", ImGuiTableColumnFlags_WidthFixed, 82.0f);
        ImGui::TableSetupColumn("Ejecutar", ImGuiTableColumnFlags_WidthFixed, 165.0f);
        ImGui::TableHeadersRow();
        for (const auto& c : evo.cases()) {
            ImGui::TableNextRow();
            ImGui::PushID(c.id.c_str());
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(c.id.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(c.prompt.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(c.split.c_str());
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", c.expectedTotal);
            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton("Baseline")) {
                auto tr = evo.runBenchmark(c.id, "baseline", lab);
                m_evoStatus = "Baseline score: " + std::to_string(static_cast<int>(tr.score));
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Adaptive")) {
                auto tr = evo.runBenchmark(c.id, "adaptive", lab);
                m_evoStatus = "Adaptive score: " + std::to_string(static_cast<int>(tr.score));
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ui::sectionHeader("Versiones de Politica");
    if (ImGui::BeginTable("##evo_vers_tbl", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 130.0f))) {
        ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("Reglas", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Accion", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        std::string restoreId;
        for (const auto& v : evo.versions()) {
            ImGui::TableNextRow();
            ImGui::PushID(v.id.c_str());
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(v.id.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", v.validationScore);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%zu", v.procedures.size());
            ImGui::TableSetColumnIndex(3);
            if (v.id == evo.activeVersionId()) {
                ui::badge("ACTIVA", pal.success, ImVec4(0.05f, 0.06f, 0.08f, 1.0f));
            } else if (ImGui::SmallButton("Restaurar")) {
                restoreId = v.id;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (!restoreId.empty()) {
            evo.restoreVersion(restoreId);
            m_evoStatus = "Restaurada: " + restoreId;
        }
    }
}

}
