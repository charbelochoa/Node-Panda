#include "table_view_panel.h"
#include "table_view_engine.h"
#include "app.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace nodepanda {

namespace {

std::string trimPanelStr(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string formatCompactNumber(double val) {
    double rounded = std::round(val);
    char buf[64];
    if (std::fabs(val - rounded) < 1e-6 && std::fabs(val) < 1e12) {
        std::snprintf(buf, sizeof(buf), "%.0f", rounded);
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f", val);
    }
    return std::string(buf);
}

std::string capitalizeFirst(const std::string& s) {
    if (s.empty()) return s;
    std::string out = s;
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

std::string writeExportFile(const NoteManager& nm, const std::string& viewId, const std::string& ext, const std::string& content) {
    std::filesystem::path baseDir = nm.getNotesDirectory();
    std::filesystem::path exportDir;
    if (!baseDir.empty() && baseDir.has_parent_path()) {
        exportDir = baseDir.parent_path() / "exports";
    } else {
        exportDir = std::filesystem::path("data") / "exports";
    }

    std::error_code ec;
    std::filesystem::create_directories(exportDir, ec);

    std::string safeId = viewId.empty() ? "tabla" : viewId;
    std::filesystem::path outPath = exportDir / (safeId + "." + ext);
    std::ofstream out(outPath, std::ios::trunc);
    if (!out.is_open()) return "";
    out << content;
    out.close();
    return outPath.string();
}

int columnTypeToIndex(const std::string& t) {
    if (t == "number") return 1;
    if (t == "date") return 2;
    if (t == "checkbox") return 3;
    return 0;
}

const char* indexToColumnType(int idx) {
    switch (idx) {
        case 1: return "number";
        case 2: return "date";
        case 3: return "checkbox";
        default: return "text";
    }
}

}

void TableViewPanel::clearInlineEdit() {
    m_editNoteId.clear();
    m_editColKey.clear();
    m_editCellBuf[0] = '\0';
    m_editFocusRequested = false;
}

void TableViewPanel::syncFilterBuffers(const std::string& viewId,
                                       const std::string& tag,
                                       const std::string& type,
                                       const std::string& ns,
                                       const std::string& search) {
    if (m_syncedViewId == viewId) return;
    m_syncedViewId = viewId;
    std::snprintf(m_tagFilterBuf, sizeof(m_tagFilterBuf), "%s", tag.c_str());
    std::snprintf(m_typeFilterBuf, sizeof(m_typeFilterBuf), "%s", type.c_str());
    std::snprintf(m_nsFilterBuf, sizeof(m_nsFilterBuf), "%s", ns.c_str());
    std::snprintf(m_searchBuf, sizeof(m_searchBuf), "%s", search.c_str());
    clearInlineEdit();
}

void TableViewPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();
    TableViewEngine& engine = app.getTableViewEngine();
    NoteManager& nm = app.getNoteManager();

    ImGui::SetNextWindowSize(ImVec2(1040.0f, 660.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Vista de Tabla")) {
        ImGui::End();
        return;
    }

    TableViewConfig* cfg = engine.activeView();
    const auto& allViews = engine.views();

    if (ui::beginCard("##table_views_bar", ImVec2(0.0f, 0.0f), false)) {
        float availRight = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        bool firstPill = true;

        for (size_t i = 0; i < allViews.size(); ++i) {
            const auto& v = allViews[i];
            bool isActive = (cfg && cfg->id == v.id);

            ImVec2 textSize = ImGui::CalcTextSize(v.name.c_str());
            float pillW = textSize.x + 22.0f;

            if (!firstPill) {
                float lastX = ImGui::GetItemRectMax().x;
                if (lastX + 6.0f + pillW < availRight - 160.0f) {
                    ImGui::SameLine(0.0f, 6.0f);
                }
            }
            firstPill = false;

            ImGui::PushID(static_cast<int>(i));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(11.0f, 5.0f));
            if (isActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, pal.accentSoft);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal.accentHover);
                ImGui::PushStyleColor(ImGuiCol_Text, pal.textPrimary);
                ImGui::PushStyleColor(ImGuiCol_Border, pal.accent);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, pal.bgSurface2);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.bgSurface3);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal.bgSurface3);
                ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
                ImGui::PushStyleColor(ImGuiCol_Border, pal.borderLine);
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

            if (ImGui::Button(v.name.c_str())) {
                engine.setActiveView(v.id);
                cfg = engine.activeView();
                clearInlineEdit();
            }

            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(5);
            ImGui::PopID();
        }

        ImGui::SameLine(0.0f, 10.0f);
        if (ui::primaryButton("+ Nueva Vista")) {
            m_newViewNameBuf[0] = '\0';
            m_newViewTagBuf[0] = '\0';
            m_newViewTypeBuf[0] = '\0';
            m_newViewNsBuf[0] = '\0';
            m_openNewViewModal = true;
        }

        if (cfg && allViews.size() > 1) {
            ImGui::SameLine(0.0f, 6.0f);
            if (ui::dangerButton("Eliminar Vista")) {
                std::string doomedId = cfg->id;
                if (engine.deleteView(doomedId)) {
                    m_syncedViewId.clear();
                    clearInlineEdit();
                    cfg = engine.activeView();
                    ui::pushToast("Vista eliminada", "info");
                }
            }
        }

        if (allViews.size() > 4 && cfg) {
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::SetNextItemWidth(170.0f);
            if (ImGui::BeginCombo("##view_dropdown", cfg->name.c_str())) {
                for (const auto& v : allViews) {
                    bool selected = (v.id == cfg->id);
                    if (ImGui::Selectable(v.name.c_str(), selected)) {
                        engine.setActiveView(v.id);
                        cfg = engine.activeView();
                        clearInlineEdit();
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        ui::endCard();
    }

    renderNewViewModal(app);
    cfg = engine.activeView();

    if (!cfg) {
        ImGui::TextColored(pal.textMuted, "No hay vistas configuradas.");
        ImGui::End();
        return;
    }

    syncFilterBuffers(cfg->id, cfg->filterTag, cfg->filterType, cfg->filterNamespace, cfg->searchText);

    std::vector<TableRowData> rows = engine.queryRows(nm, *cfg);
    std::vector<TableColumnAggregate> aggs = engine.computeAggregates(rows, *cfg);

    ImGui::Spacing();
    if (ui::beginCard("##table_filters_bar", ImVec2(0.0f, 0.0f), false)) {
        bool filtersChanged = false;

        ImGui::SetNextItemWidth(135.0f);
        if (ImGui::InputTextWithHint("##flt_tag", "#tag", m_tagFilterBuf, sizeof(m_tagFilterBuf))) {
            cfg->filterTag = trimPanelStr(m_tagFilterBuf);
            filtersChanged = true;
        }
        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetNextItemWidth(135.0f);
        if (ImGui::InputTextWithHint("##flt_type", "tipo", m_typeFilterBuf, sizeof(m_typeFilterBuf))) {
            cfg->filterType = trimPanelStr(m_typeFilterBuf);
            filtersChanged = true;
        }
        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetNextItemWidth(145.0f);
        if (ImGui::InputTextWithHint("##flt_ns", "namespace", m_nsFilterBuf, sizeof(m_nsFilterBuf))) {
            cfg->filterNamespace = trimPanelStr(m_nsFilterBuf);
            filtersChanged = true;
        }
        ImGui::SameLine(0.0f, 6.0f);

        float remSearchW = std::max(140.0f, ImGui::GetContentRegionAvail().x - 170.0f);
        ImGui::SetNextItemWidth(remSearchW);
        if (ImGui::InputTextWithHint("##flt_search", "Buscar en notas...", m_searchBuf, sizeof(m_searchBuf))) {
            cfg->searchText = trimPanelStr(m_searchBuf);
            filtersChanged = true;
        }
        ImGui::SameLine(0.0f, 6.0f);

        if (ui::subtleButton("Limpiar")) {
            m_tagFilterBuf[0] = '\0';
            m_typeFilterBuf[0] = '\0';
            m_nsFilterBuf[0] = '\0';
            m_searchBuf[0] = '\0';
            cfg->filterTag.clear();
            cfg->filterType.clear();
            cfg->filterNamespace.clear();
            cfg->searchText.clear();
            filtersChanged = true;
        }

        ImGui::SameLine(0.0f, 8.0f);
        char countBuf[48];
        std::snprintf(countBuf, sizeof(countBuf), "%zu filas", rows.size());
        ui::badge(countBuf, pal.bgSurface3, pal.textSecondary);

        if (filtersChanged) {
            engine.saveViews();
            rows = engine.queryRows(nm, *cfg);
            aggs = engine.computeAggregates(rows, *cfg);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetNextItemWidth(130.0f);
        bool enterAddCol = ImGui::InputTextWithHint("##new_col_key", "clave...", m_newColKeyBuf, sizeof(m_newColKeyBuf),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine(0.0f, 6.0f);

        static const char* kColTypes[] = {"text", "number", "date", "checkbox"};
        ImGui::SetNextItemWidth(96.0f);
        ImGui::Combo("##new_col_type", &m_newColTypeIdx, kColTypes, IM_ARRAYSIZE(kColTypes));
        ImGui::SameLine(0.0f, 6.0f);

        if (ui::subtleButton("+ Columna") || enterAddCol) {
            std::string key = trimPanelStr(m_newColKeyBuf);
            if (!key.empty()) {
                bool exists = false;
                for (auto& existing : cfg->columns) {
                    if (existing.key == key) {
                        existing.visible = true;
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    TableColumnSpec spec;
                    spec.key = key;
                    spec.label = capitalizeFirst(key);
                    spec.type = indexToColumnType(m_newColTypeIdx);
                    spec.visible = true;
                    cfg->columns.push_back(spec);
                }
                m_newColKeyBuf[0] = '\0';
                engine.saveViews();
                rows = engine.queryRows(nm, *cfg);
                aggs = engine.computeAggregates(rows, *cfg);
                ui::pushToast("Columna agregada: " + key, "success");
            }
        }

        ImGui::SameLine(0.0f, 8.0f);
        if (ui::subtleButton("Columnas")) {
            ImGui::OpenPopup("##table_cols_popup");
        }

        if (ImGui::BeginPopup("##table_cols_popup")) {
            bool colsModified = false;
            int removeIdx = -1;
            for (size_t ci = 0; ci < cfg->columns.size(); ++ci) {
                auto& col = cfg->columns[ci];
                ImGui::PushID(static_cast<int>(ci));
                if (ImGui::Checkbox("##vis", &col.visible)) {
                    colsModified = true;
                }
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::Text("%-14s", col.label.empty() ? col.key.c_str() : col.label.c_str());
                ImGui::SameLine(0.0f, 8.0f);
                int tIdx = columnTypeToIndex(col.type);
                ImGui::SetNextItemWidth(88.0f);
                if (ImGui::Combo("##ctype", &tIdx, kColTypes, IM_ARRAYSIZE(kColTypes))) {
                    col.type = indexToColumnType(tIdx);
                    colsModified = true;
                }
                if (col.key != "titulo" && cfg->columns.size() > 1) {
                    ImGui::SameLine(0.0f, 6.0f);
                    if (ImGui::SmallButton("x")) {
                        removeIdx = static_cast<int>(ci);
                    }
                }
                ImGui::PopID();
            }
            if (removeIdx >= 0 && removeIdx < static_cast<int>(cfg->columns.size())) {
                cfg->columns.erase(cfg->columns.begin() + removeIdx);
                colsModified = true;
            }
            if (colsModified) {
                engine.saveViews();
                rows = engine.queryRows(nm, *cfg);
                aggs = engine.computeAggregates(rows, *cfg);
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine(0.0f, 16.0f);
        if (ui::subtleButton("Exportar CSV")) {
            std::string csv = engine.exportCsv(rows, *cfg);
            ImGui::SetClipboardText(csv.c_str());
            std::string savedPath = writeExportFile(nm, cfg->id, "csv", csv);
            m_exportTitle = "Exportacion CSV - " + cfg->name;
            m_exportContent = csv;
            m_exportSavedPath = savedPath;
            m_openExportModal = true;
            ui::pushToast("CSV copiado al portapapeles", "success");
        }

        ImGui::SameLine(0.0f, 6.0f);
        if (ui::subtleButton("Exportar Markdown")) {
            std::string md = engine.exportMarkdown(rows, *cfg);
            ImGui::SetClipboardText(md.c_str());
            std::string savedPath = writeExportFile(nm, cfg->id, "md", md);
            m_exportTitle = "Exportacion Markdown - " + cfg->name;
            m_exportContent = md;
            m_exportSavedPath = savedPath;
            m_openExportModal = true;
            ui::pushToast("Markdown copiado al portapapeles", "success");
        }

        ui::endCard();
    }

    ImGui::Spacing();

    std::vector<size_t> visibleColIndices;
    for (size_t ci = 0; ci < cfg->columns.size(); ++ci) {
        if (cfg->columns[ci].visible) {
            visibleColIndices.push_back(ci);
        }
    }

    if (visibleColIndices.empty()) {
        ImGui::TextColored(pal.textMuted, "Activa al menos una columna desde el menu Columnas.");
        renderExportModal();
        ImGui::End();
        return;
    }

    int totalCols = static_cast<int>(visibleColIndices.size()) + 1;
    std::string tableId = "##db_table_" + cfg->id;
    bool viewJustSwitched = (m_lastTableViewId != cfg->id);
    m_lastTableViewId = cfg->id;

    ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Sortable |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollX |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;

    float tableHeight = std::max(180.0f, ImGui::GetContentRegionAvail().y - 4.0f);
    if (ImGui::BeginTable(tableId.c_str(), totalCols, tableFlags, ImVec2(0.0f, tableHeight))) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("##open_col",
                                ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed,
                                30.0f,
                                static_cast<ImGuiID>(999999));

        for (size_t ci : visibleColIndices) {
            const auto& col = cfg->columns[ci];
            const char* headerLabel = col.label.empty() ? col.key.c_str() : col.label.c_str();
            ImGuiTableColumnFlags colFlags = ImGuiTableColumnFlags_None;
            if (col.key == cfg->sortColumn) {
                colFlags |= ImGuiTableColumnFlags_DefaultSort;
                colFlags |= cfg->sortAscending ? ImGuiTableColumnFlags_PreferSortAscending
                                               : ImGuiTableColumnFlags_PreferSortDescending;
            }
            float initWeight = (col.key == "titulo") ? 1.8f : (col.type == "checkbox" ? 0.65f : 1.0f);
            ImGui::TableSetupColumn(headerLabel, colFlags, initWeight, static_cast<ImGuiID>(ci));
        }

        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsDirty) {
                if (!viewJustSwitched && sortSpecs->SpecsCount > 0) {
                    const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
                    size_t colIdx = static_cast<size_t>(spec.ColumnUserID);
                    if (colIdx < cfg->columns.size()) {
                        cfg->sortColumn = cfg->columns[colIdx].key;
                        cfg->sortAscending = (spec.SortDirection == ImGuiSortDirection_Ascending);
                        engine.saveViews();
                        rows = engine.queryRows(nm, *cfg);
                        aggs = engine.computeAggregates(rows, *cfg);
                    }
                }
                sortSpecs->SpecsDirty = false;
            }
        }

        for (size_t r = 0; r < rows.size(); ++r) {
            const auto& row = rows[r];
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(r));

            bool isSelectedNote = (app.selectedNoteId == row.noteId);
            if (isSelectedNote) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       ImGui::ColorConvertFloat4ToU32(ImVec4(pal.accent.x, pal.accent.y, pal.accent.z, 0.12f)));
            }

            ImGui::TableSetColumnIndex(0);
            if (isSelectedNote) {
                ImGui::PushStyleColor(ImGuiCol_Text, pal.accent);
            }
            if (ImGui::SmallButton(">")) {
                app.selectedNoteId = row.noteId;
            }
            if (isSelectedNote) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Abrir nota: %s", row.noteId.c_str());
            }

            for (size_t vi = 0; vi < visibleColIndices.size(); ++vi) {
                size_t ci = visibleColIndices[vi];
                const auto& col = cfg->columns[ci];
                ImGui::TableSetColumnIndex(static_cast<int>(vi + 1));
                ImGui::PushID(static_cast<int>(vi));

                auto cellIt = row.cells.find(col.key);
                std::string cellVal = (cellIt != row.cells.end()) ? cellIt->second : "";
                bool isReadOnly = TableViewEngine::isComputedColumn(col.key);

                if (col.type == "checkbox" && !isReadOnly) {
                    bool checked = TableViewEngine::parseCheckboxValue(cellVal);
                    if (ImGui::Checkbox("##chk", &checked)) {
                        if (engine.updateCell(nm, row.noteId, col.key, checked ? "true" : "false")) {
                            app.graphNeedsRebuild = true;
                            ui::pushToast("Guardado en frontmatter", "success", 2.0f);
                        }
                    }
                } else if (isReadOnly) {
                    ImGui::TextColored(pal.textSecondary, "%s", cellVal.empty() ? "-" : cellVal.c_str());
                } else {
                    bool isEditing = (m_editNoteId == row.noteId && m_editColKey == col.key);
                    if (isEditing) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (m_editFocusRequested) {
                            ImGui::SetKeyboardFocusHere();
                            m_editFocusRequested = false;
                        }
                        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
                        if (col.type == "number") {
                            inputFlags |= ImGuiInputTextFlags_CharsDecimal;
                        }
                        const char* hint = "";
                        if (col.type == "date") hint = "YYYY-MM-DD";
                        else if (col.type == "number") hint = "0";

                        bool enterPressed = ImGui::InputTextWithHint("##inline_edit", hint, m_editCellBuf, sizeof(m_editCellBuf), inputFlags);
                        bool escPressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
                        bool deactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();
                        bool deactivatedWithoutEdit = ImGui::IsItemDeactivated() && !deactivatedAfterEdit;

                        if (escPressed) {
                            clearInlineEdit();
                        } else if (enterPressed || deactivatedAfterEdit) {
                            std::string updatedVal(m_editCellBuf);
                            if (engine.updateCell(nm, row.noteId, col.key, updatedVal)) {
                                app.graphNeedsRebuild = true;
                                ui::pushToast("Guardado en frontmatter", "success", 2.0f);
                            }
                            clearInlineEdit();
                        } else if (deactivatedWithoutEdit) {
                            clearInlineEdit();
                        }
                    } else {
                        std::string displayText = cellVal.empty() ? "-" : cellVal;
                        if (cellVal.empty()) {
                            ImGui::PushStyleColor(ImGuiCol_Text, pal.textMuted);
                        } else if (col.key == "titulo") {
                            ImGui::PushStyleColor(ImGuiCol_Text, pal.textPrimary);
                        } else if (col.type == "number") {
                            ImGui::PushStyleColor(ImGuiCol_Text, pal.aqua);
                        } else if (col.type == "date") {
                            ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
                        } else {
                            ImGui::PushStyleColor(ImGuiCol_Text, pal.textPrimary);
                        }

                        std::string selLabel = displayText + "##cell_sel";
                        if (ImGui::Selectable(selLabel.c_str(), false, ImGuiSelectableFlags_None)) {
                            m_editNoteId = row.noteId;
                            m_editColKey = col.key;
                            std::snprintf(m_editCellBuf, sizeof(m_editCellBuf), "%s", cellVal.c_str());
                            m_editFocusRequested = true;
                        }
                        ImGui::PopStyleColor();
                    }
                }

                ImGui::PopID();
            }

            ImGui::PopID();
        }

        ImGui::TableNextRow();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(pal.bgSurface2));
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(pal.accent, "=");

        for (size_t vi = 0; vi < visibleColIndices.size(); ++vi) {
            size_t ci = visibleColIndices[vi];
            const auto& col = cfg->columns[ci];
            ImGui::TableSetColumnIndex(static_cast<int>(vi + 1));

            if (ci < aggs.size()) {
                const auto& agg = aggs[ci];
                if (col.type == "checkbox") {
                    int checkedCount = 0;
                    for (const auto& row : rows) {
                        auto it = row.cells.find(col.key);
                        if (it != row.cells.end() && TableViewEngine::parseCheckboxValue(it->second)) {
                            checkedCount++;
                        }
                    }
                    ImGui::TextColored(pal.success, "Si: %d/%d", checkedCount, agg.count);
                } else if (agg.numeric) {
                    std::string sumStr = formatCompactNumber(agg.sum);
                    std::string avgStr = formatCompactNumber(agg.avg);
                    ImGui::TextColored(pal.accent, "N:%d | Sum:%s | Avg:%s", agg.nonEmpty, sumStr.c_str(), avgStr.c_str());
                } else {
                    ImGui::TextColored(pal.textSecondary, "N: %d/%d", agg.nonEmpty, agg.count);
                }
            }
        }

        ImGui::EndTable();
    }

    renderExportModal();
    ImGui::End();
}

void TableViewPanel::renderNewViewModal(App& app) {
    if (m_openNewViewModal) {
        ImGui::OpenPopup("Nueva Vista de Tabla");
        m_openNewViewModal = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Nueva Vista de Tabla", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Nombre de la vista");
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool enterCreate = ImGui::InputTextWithHint("##nv_name", "Ej: Lecturas 2026", m_newViewNameBuf, sizeof(m_newViewNameBuf),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();
        ImGui::TextUnformatted("Filtro #tag (opcional)");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##nv_tag", "Ej: investigacion", m_newViewTagBuf, sizeof(m_newViewTagBuf));

        ImGui::Spacing();
        ImGui::TextUnformatted("Filtro tipo (opcional)");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##nv_type", "Ej: proyecto, concepto", m_newViewTypeBuf, sizeof(m_newViewTypeBuf));

        ImGui::Spacing();
        ImGui::TextUnformatted("Filtro namespace (opcional)");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##nv_ns", "Ej: trabajo/q1", m_newViewNsBuf, sizeof(m_newViewNsBuf));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ui::primaryButton("Crear Vista") || enterCreate) {
            std::string name = trimPanelStr(m_newViewNameBuf);
            if (name.empty()) name = "Nueva Vista";
            auto& created = app.getTableViewEngine().createView(
                name,
                trimPanelStr(m_newViewTagBuf),
                trimPanelStr(m_newViewTypeBuf));
            std::string ns = trimPanelStr(m_newViewNsBuf);
            if (!ns.empty()) {
                created.filterNamespace = ns;
                app.getTableViewEngine().saveViews();
            }
            m_syncedViewId.clear();
            clearInlineEdit();
            ui::pushToast("Vista creada: " + created.name, "success");
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine(0.0f, 8.0f);
        if (ui::subtleButton("Cancelar")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void TableViewPanel::renderExportModal() {
    if (m_openExportModal) {
        ImGui::OpenPopup("Resultado de Exportacion");
        m_openExportModal = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(640.0f, 420.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Resultado de Exportacion", nullptr, ImGuiWindowFlags_None)) {
        const ThemePalette& pal = UITheme::palette();
        ImGui::TextUnformatted(m_exportTitle.c_str());
        if (!m_exportSavedPath.empty()) {
            ImGui::TextColored(pal.textSecondary, "Archivo: %s", m_exportSavedPath.c_str());
        }
        ImGui::Spacing();

        float availH = std::max(140.0f, ImGui::GetContentRegionAvail().y - 42.0f);
        ImGui::InputTextMultiline("##export_preview",
                                  const_cast<char*>(m_exportContent.c_str()),
                                  m_exportContent.size() + 1,
                                  ImVec2(-FLT_MIN, availH),
                                  ImGuiInputTextFlags_ReadOnly);

        ImGui::Spacing();
        if (ui::primaryButton("Copiar")) {
            ImGui::SetClipboardText(m_exportContent.c_str());
            ui::pushToast("Contenido copiado", "success");
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (ui::subtleButton("Cerrar")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}
