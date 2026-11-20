#include "outliner_panel.h"
#include "outliner_engine.h"
#include "app.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace nodepanda {

void OutlinerPanel::zoomToBlock(const std::string& uuid) {
    m_zoomedBlockUuid = uuid;
    m_selectedBlockUuid = uuid;
}

void OutlinerPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();
    OutlinerEngine& engine = app.getOutlinerEngine();
    NoteManager& nm = app.getNoteManager();

    ImGui::SetNextWindowSize(ImVec2(760, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Outliner")) {
        ImGui::End();
        return;
    }

    if (!m_initialized) {
        engine.rebuildFromNotes(nm);
        m_initialized = true;
    }

    if (app.selectedNoteId != m_lastPageId) {
        m_lastPageId = app.selectedNoteId;
        m_zoomedBlockUuid.clear();
        m_editingBlockUuid.clear();
        m_selectedBlockUuid.clear();
        engine.rebuildFromNotes(nm);
    }

    if (app.selectedNoteId.empty()) {
        ImGui::TextColored(pal.textMuted, "Selecciona una nota para abrir el outliner.");
        ImGui::End();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, pal.textPrimary);
    ImGui::Text("%s", app.selectedNoteId.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##outliner_search", "Buscar bloques...", m_searchBuffer, sizeof(m_searchBuffer));

    ImGui::SameLine();
    if (ui::primaryButton("+ Bloque")) {
        std::string created = engine.createBlock(nm, app.selectedNoteId, "Nuevo bloque", m_zoomedBlockUuid, false);
        if (!created.empty()) {
            m_editingBlockUuid = created;
            m_selectedBlockUuid = created;
            std::strncpy(m_editBuffer, "Nuevo bloque", sizeof(m_editBuffer) - 1);
            m_editBuffer[sizeof(m_editBuffer) - 1] = '\0';
            m_focusEditInput = true;
            app.graphNeedsRebuild = true;
        }
    }

    ImGui::SameLine();
    if (ui::subtleButton("Recargar")) {
        engine.rebuildFromNotes(nm);
        if (!m_zoomedBlockUuid.empty() && !engine.getBlock(m_zoomedBlockUuid)) {
            m_zoomedBlockUuid.clear();
        }
    }

    if (!m_zoomedBlockUuid.empty()) {
        ImGui::Spacing();
        if (ui::subtleButton(app.selectedNoteId.c_str())) {
            m_zoomedBlockUuid.clear();
        }
        std::vector<std::string> crumbs = engine.getBreadcrumbs(m_zoomedBlockUuid);
        for (size_t i = 0; i < crumbs.size(); ++i) {
            const BlockNode* cb = engine.getBlock(crumbs[i]);
            if (!cb) continue;
            ImGui::SameLine();
            ImGui::TextColored(pal.textMuted, ">");
            ImGui::SameLine();
            std::string label = cb->content.empty() ? "(vacio)" : cb->content;
            if (label.size() > 28) label = label.substr(0, 25) + "...";
            ImGui::PushID(static_cast<int>(i));
            if (ui::subtleButton(label.c_str())) {
                m_zoomedBlockUuid = crumbs[i];
            }
            ImGui::PopID();
        }
    }

    ImGui::Separator();

    std::string searchQuery(m_searchBuffer);
    if (!searchQuery.empty()) {
        std::vector<const BlockNode*> matches = engine.searchBlocks(searchQuery);
        ImGui::TextColored(pal.textSecondary, "Coincidencias: %d", static_cast<int>(matches.size()));
        ImGui::BeginChild("##outliner_search_results", ImVec2(0, 0), false);
        for (size_t i = 0; i < matches.size(); ++i) {
            const BlockNode* b = matches[i];
            if (!b) continue;
            ImGui::PushID(b->uuid.c_str());
            if (ui::beginCard("##match_card", ImVec2(0, 0), true)) {
                ui::badge(b->pageId.c_str(), pal.accentSoft, pal.accent);
                ImGui::SameLine();
                std::string resolved = engine.resolveBlockRefs(b->content);
                if (ImGui::Selectable(resolved.empty() ? "(vacio)" : resolved.c_str())) {
                    app.selectedNoteId = b->pageId;
                    m_lastPageId = b->pageId;
                    m_zoomedBlockUuid = b->uuid;
                    m_selectedBlockUuid = b->uuid;
                    m_searchBuffer[0] = '\0';
                }
            }
            ui::endCard();
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    ImGui::BeginChild("##outliner_tree", ImVec2(0, -38.0f), false);

    if (!m_zoomedBlockUuid.empty()) {
        const BlockNode* zoomed = engine.getBlock(m_zoomedBlockUuid);
        if (zoomed) {
            renderBlockTree(app, m_zoomedBlockUuid, 0);
        } else {
            m_zoomedBlockUuid.clear();
        }
    } else {
        std::vector<std::string> roots = engine.getRootBlocksForPage(app.selectedNoteId);
        if (roots.empty()) {
            ImGui::TextColored(pal.textMuted, "Sin bloques en esta pagina.");
        } else {
            for (const auto& rootUuid : roots) {
                renderBlockTree(app, rootUuid, 0);
            }
        }
    }

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 105.0f);
    bool addSubmitted = ImGui::InputTextWithHint("##quick_add_block", "Agregar bloque...",
        m_newBlockBuffer, sizeof(m_newBlockBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ui::primaryButton("Agregar", ImVec2(95.0f, 0.0f)) || addSubmitted) {
        std::string txt(m_newBlockBuffer);
        if (!txt.empty()) {
            engine.createBlock(nm, app.selectedNoteId, txt, m_zoomedBlockUuid, false);
            m_newBlockBuffer[0] = '\0';
            app.graphNeedsRebuild = true;
        }
    }

    ImGui::End();
}

void OutlinerPanel::renderBlockTree(App& app, const std::string& uuid, int visualDepth) {
    OutlinerEngine& engine = app.getOutlinerEngine();
    NoteManager& nm = app.getNoteManager();
    const ThemePalette& pal = UITheme::palette();

    BlockNode* block = engine.getBlockMutable(uuid);
    if (!block) return;

    std::vector<std::string> childrenCopy = block->childrenUuids;
    std::map<std::string, std::string> propsCopy = block->properties;
    std::vector<std::string> tagsCopy = block->tags;
    std::vector<std::string> pageRefsCopy = block->pageRefs;
    std::vector<std::string> blockRefsCopy = block->blockRefs;
    std::string contentCopy = block->content;
    bool hasChildren = !childrenCopy.empty();
    bool collapsed = block->collapsed;

    ImGui::PushID(uuid.c_str());

    float indentPx = static_cast<float>(visualDepth) * 22.0f;
    ImVec2 rowStart = ImGui::GetCursorScreenPos();
    if (visualDepth > 0) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 guideCol = ImGui::ColorConvertFloat4ToU32(pal.borderLine);
        for (int g = 0; g < visualDepth; ++g) {
            float gx = rowStart.x + static_cast<float>(g) * 22.0f + 8.0f;
            dl->AddLine(ImVec2(gx, rowStart.y), ImVec2(gx, rowStart.y + 24.0f), guideCol, 1.0f);
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentPx);
    }

    if (hasChildren) {
        if (ImGui::SmallButton(collapsed ? ">" : "v")) {
            block->collapsed = !block->collapsed;
            collapsed = block->collapsed;
        }
        ImGui::SameLine(0.0f, 4.0f);
    } else {
        ImGui::Dummy(ImVec2(18.0f, 1.0f));
        ImGui::SameLine(0.0f, 4.0f);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, m_selectedBlockUuid == uuid ? pal.accentSoft : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, m_selectedBlockUuid == uuid ? pal.accent : pal.textSecondary);
    if (ImGui::SmallButton("o")) {
        zoomToBlock(uuid);
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zoom al bloque (%s)", uuid.substr(0, std::min<size_t>(8, uuid.size())).c_str());
    }

    ImGui::SameLine(0.0f, 6.0f);

    bool actionDeleted = false;
    if (m_editingBlockUuid == uuid) {
        ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x - 190.0f));
        if (m_focusEditInput) {
            ImGui::SetKeyboardFocusHere();
            m_focusEditInput = false;
        }
        bool commit = ImGui::InputText("##edit_block", m_editBuffer, sizeof(m_editBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (commit || ImGui::IsItemDeactivatedAfterEdit()) {
            engine.updateBlockContent(nm, uuid, std::string(m_editBuffer));
            m_editingBlockUuid.clear();
            app.graphNeedsRebuild = true;
        }
    } else {
        std::string displayText = contentCopy.empty() ? "(vacio)" : contentCopy;
        ImGui::PushStyleColor(ImGuiCol_Text, contentCopy.empty() ? pal.textMuted : pal.textPrimary);
        bool clicked = ImGui::Selectable(displayText.c_str(), m_selectedBlockUuid == uuid,
            ImGuiSelectableFlags_AllowOverlap, ImVec2(std::max(120.0f, ImGui::GetContentRegionAvail().x - 195.0f), 0.0f));
        ImGui::PopStyleColor();
        if (clicked) {
            m_selectedBlockUuid = uuid;
            m_editingBlockUuid = uuid;
            std::strncpy(m_editBuffer, contentCopy.c_str(), sizeof(m_editBuffer) - 1);
            m_editBuffer[sizeof(m_editBuffer) - 1] = '\0';
            m_focusEditInput = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("+")) {
        std::string ch = engine.createBlock(nm, app.selectedNoteId, "Sub-bloque", uuid, false);
        if (!ch.empty()) {
            m_selectedBlockUuid = ch;
            m_editingBlockUuid = ch;
            std::strncpy(m_editBuffer, "Sub-bloque", sizeof(m_editBuffer) - 1);
            m_editBuffer[sizeof(m_editBuffer) - 1] = '\0';
            m_focusEditInput = true;
            app.graphNeedsRebuild = true;
        }
    }
    ImGui::SameLine(0.0f, 2.0f);
    if (ImGui::SmallButton("<")) {
        if (engine.unindentBlock(nm, uuid)) app.graphNeedsRebuild = true;
    }
    ImGui::SameLine(0.0f, 2.0f);
    if (ImGui::SmallButton(">")) {
        if (engine.indentBlock(nm, uuid)) app.graphNeedsRebuild = true;
    }
    ImGui::SameLine(0.0f, 2.0f);
    if (ImGui::SmallButton("^")) {
        if (engine.moveBlockUp(nm, uuid)) app.graphNeedsRebuild = true;
    }
    ImGui::SameLine(0.0f, 2.0f);
    if (ImGui::SmallButton("v##dn")) {
        if (engine.moveBlockDown(nm, uuid)) app.graphNeedsRebuild = true;
    }
    ImGui::SameLine(0.0f, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, pal.danger);
    if (ImGui::SmallButton("x")) {
        if (m_zoomedBlockUuid == uuid) m_zoomedBlockUuid.clear();
        if (m_editingBlockUuid == uuid) m_editingBlockUuid.clear();
        if (m_selectedBlockUuid == uuid) m_selectedBlockUuid.clear();
        engine.deleteBlock(nm, uuid);
        app.graphNeedsRebuild = true;
        actionDeleted = true;
    }
    ImGui::PopStyleColor();

    if (!actionDeleted) {
        if (!propsCopy.empty() || !tagsCopy.empty() || !pageRefsCopy.empty()) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentPx + 42.0f);
            bool firstBadge = true;
            for (const auto& [k, v] : propsCopy) {
                if (!firstBadge) ImGui::SameLine(0.0f, 6.0f);
                std::string propText = k + ":: " + v;
                ui::badge(propText.c_str(), pal.bgSurface2, pal.aqua);
                firstBadge = false;
            }
            for (const auto& tag : tagsCopy) {
                if (!firstBadge) ImGui::SameLine(0.0f, 6.0f);
                std::string tagText = "#" + tag;
                ui::badge(tagText.c_str(), pal.accentSoft, pal.proViolet);
                firstBadge = false;
            }
            for (const auto& ref : pageRefsCopy) {
                if (!firstBadge) ImGui::SameLine(0.0f, 6.0f);
                std::string refLabel = "[[" + ref + "]]";
                ImGui::PushStyleColor(ImGuiCol_Button, pal.accentSoft);
                ImGui::PushStyleColor(ImGuiCol_Text, pal.accent);
                if (ImGui::SmallButton(refLabel.c_str())) {
                    if (nm.getNoteById(ref)) {
                        app.selectedNoteId = ref;
                    }
                }
                ImGui::PopStyleColor(2);
                firstBadge = false;
            }
        }

        if (!blockRefsCopy.empty()) {
            std::string resolved = engine.resolveBlockRefs(contentCopy);
            if (resolved != contentCopy) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentPx + 42.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
                ImGui::TextWrapped("Ref: %s", resolved.c_str());
                ImGui::PopStyleColor();
            }
        }

        if (!collapsed) {
            for (const auto& childUuid : childrenCopy) {
                renderBlockTree(app, childUuid, visualDepth + 1);
            }
        }
    }

    ImGui::PopID();
}

}
