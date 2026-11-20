#include "whiteboard_panel.h"
#include "whiteboard_engine.h"
#include "app.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace nodepanda {

namespace {

ImVec4 nodeAccentColor(int colorIndex, const ThemePalette& pal) {
    switch (colorIndex % 6) {
        case 0: return pal.accent;
        case 1: return pal.aqua;
        case 2: return pal.warning;
        case 3: return pal.success;
        case 4: return pal.proViolet;
        case 5: return pal.orange;
    }
    return pal.accent;
}

ImVec4 nodeFillColor(WhiteboardNodeKind kind, int colorIndex, const ThemePalette& pal) {
    ImVec4 acc = nodeAccentColor(colorIndex, pal);
    if (kind == WhiteboardNodeKind::Frame) {
        return ImVec4(acc.x, acc.y, acc.z, 0.08f);
    }
    if (kind == WhiteboardNodeKind::StickyNote) {
        return ImVec4(
            pal.bgSurface2.x * 0.65f + acc.x * 0.35f,
            pal.bgSurface2.y * 0.65f + acc.y * 0.35f,
            pal.bgSurface2.z * 0.65f + acc.z * 0.35f,
            0.96f);
    }
    return pal.bgSurface1;
}

ImVec2 rectEdgeAnchor(const WhiteboardNode& node, const ImVec2& targetCenter) {
    float cx = node.x + node.w * 0.5f;
    float cy = node.y + node.h * 0.5f;
    float dx = targetCenter.x - cx;
    float dy = targetCenter.y - cy;
    if (std::fabs(dx) < 0.001f && std::fabs(dy) < 0.001f) {
        return ImVec2(cx, cy);
    }
    float hw = node.w * 0.5f;
    float hh = node.h * 0.5f;
    float tx = (std::fabs(dx) > 0.001f) ? (hw / std::fabs(dx)) : 1e9f;
    float ty = (std::fabs(dy) > 0.001f) ? (hh / std::fabs(dy)) : 1e9f;
    float t = std::min(tx, ty);
    return ImVec2(cx + dx * t, cy + dy * t);
}

void drawArrowLine(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, ImU32 color, float thickness) {
    dl->AddLine(p1, p2, color, thickness);
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 6.0f) return;
    float angle = std::atan2(dy, dx);
    float arrowLen = 11.0f;
    float spread = 0.45f;
    ImVec2 left(p2.x - arrowLen * std::cos(angle - spread), p2.y - arrowLen * std::sin(angle - spread));
    ImVec2 right(p2.x - arrowLen * std::cos(angle + spread), p2.y - arrowLen * std::sin(angle + spread));
    dl->AddTriangleFilled(p2, left, right, color);
}

const WhiteboardNode* findNodeConst(const Whiteboard& wb, const std::string& id) {
    for (const auto& n : wb.nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

WhiteboardNode* findNodeMutable(Whiteboard& wb, const std::string& id) {
    for (auto& n : wb.nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

}

ImVec2 WhiteboardPanel::worldToScreen(const ImVec2& canvasOrigin, float wx, float wy) const {
    return ImVec2(canvasOrigin.x + (wx + m_panX) * m_zoom,
                  canvasOrigin.y + (wy + m_panY) * m_zoom);
}

ImVec2 WhiteboardPanel::screenToWorld(const ImVec2& canvasOrigin, const ImVec2& sp) const {
    return ImVec2((sp.x - canvasOrigin.x) / m_zoom - m_panX,
                  (sp.y - canvasOrigin.y) / m_zoom - m_panY);
}

void WhiteboardPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();
    WhiteboardEngine& engine = app.getWhiteboardEngine();
    NoteManager& nm = app.getNoteManager();

    ImGui::SetNextWindowSize(ImVec2(980, 680), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Pizarra")) {
        ImGui::End();
        return;
    }

    Whiteboard* board = engine.activeBoard();
    if (!board) {
        board = &engine.createBoard("Principal");
    }

    std::vector<std::string> boardIds = engine.listBoards();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::BeginCombo("##wb_select", board->name.c_str())) {
        for (const auto& bid : boardIds) {
            Whiteboard* candidate = engine.getBoard(bid);
            if (!candidate) continue;
            bool sel = (candidate->id == board->id);
            if (ImGui::Selectable(candidate->name.c_str(), sel)) {
                engine.setActiveBoard(bid);
                board = engine.activeBoard();
                m_selectedNodeId.clear();
                m_connectFromId.clear();
                m_inspectorNodeId.clear();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (ui::subtleButton("+ Pizarra")) {
        ImGui::OpenPopup("##new_board_popup");
    }
    if (ImGui::BeginPopup("##new_board_popup")) {
        ImGui::SetNextItemWidth(180.0f);
        bool enter = ImGui::InputTextWithHint("##nb_name", "Nombre...", m_newBoardName, sizeof(m_newBoardName),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ui::primaryButton("Crear") || enter) {
            if (m_newBoardName[0] != '\0') {
                Whiteboard& created = engine.createBoard(std::string(m_newBoardName));
                engine.saveBoard(created.id);
                board = engine.activeBoard();
                m_newBoardName[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, 10.0f);
    float spawnX = -m_panX + 120.0f + static_cast<float>((board->nodes.size() % 5) * 32);
    float spawnY = -m_panY + 90.0f + static_cast<float>((board->nodes.size() % 4) * 32);

    if (ui::subtleButton("+ Nota")) {
        std::string linked = app.selectedNoteId;
        std::string title = linked.empty() ? "Nota" : linked;
        std::string body;
        if (!linked.empty()) {
            Note* n = nm.getNoteById(linked);
            if (n) {
                body = n->content.substr(0, std::min<size_t>(160, n->content.size()));
            }
        }
        m_selectedNodeId = engine.addNode(WhiteboardNodeKind::NoteCard, title, body, spawnX, spawnY, linked);
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (ui::subtleButton("+ Sticky")) {
        m_selectedNodeId = engine.addNode(WhiteboardNodeKind::StickyNote, "Idea", "Escribe aqui...", spawnX, spawnY);
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (ui::subtleButton("+ Texto")) {
        m_selectedNodeId = engine.addNode(WhiteboardNodeKind::TextBox, "Bloque", "Contenido...", spawnX, spawnY);
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (ui::subtleButton("+ Marco")) {
        m_selectedNodeId = engine.addNode(WhiteboardNodeKind::Frame, "Grupo", "", spawnX - 40.0f, spawnY - 30.0f);
    }

    ImGui::SameLine(0.0f, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, m_connectMode ? pal.accent : pal.bgSurface2);
    ImGui::PushStyleColor(ImGuiCol_Text, m_connectMode ? ImVec4(1, 1, 1, 1) : pal.textPrimary);
    if (ImGui::Button(m_connectMode ? "Conectar: ON" : "Conectar")) {
        m_connectMode = !m_connectMode;
        m_connectFromId.clear();
    }
    ImGui::PopStyleColor(2);

    if (m_connectMode) {
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputTextWithHint("##edge_lbl", "Etiqueta...", m_edgeLabelBuffer, sizeof(m_edgeLabelBuffer));
    }

    ImGui::SameLine(0.0f, 8.0f);
    if (ui::subtleButton("Centrar")) {
        m_panX = 40.0f;
        m_panY = 40.0f;
        m_zoom = 1.0f;
    }

    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextColored(pal.textMuted, "%d%%", static_cast<int>(m_zoom * 100.0f + 0.5f));

    ImGui::SameLine(0.0f, 8.0f);
    if (ui::primaryButton("Guardar")) {
        if (engine.saveBoard(board->id)) {
            ui::pushToast("Pizarra guardada", "success");
        } else {
            ui::pushToast("Error al guardar pizarra", "error");
        }
    }

    ImGui::Separator();

    bool hasSelection = (!m_selectedNodeId.empty() && findNodeMutable(*board, m_selectedNodeId) != nullptr);
    float inspectorHeight = hasSelection ? 120.0f : 0.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 canvasSize(std::max(200.0f, avail.x), std::max(200.0f, avail.y - inspectorHeight - (hasSelection ? 8.0f : 0.0f)));
    ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    ImVec2 canvasEnd(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y);

    ImGui::InvisibleButton("##wb_canvas", canvasSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool canvasHovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    if (canvasHovered && io.MouseWheel != 0.0f) {
        ImVec2 worldBefore = screenToWorld(canvasOrigin, mousePos);
        float factor = (io.MouseWheel > 0.0f) ? 1.12f : (1.0f / 1.12f);
        m_zoom = std::clamp(m_zoom * factor, 0.25f, 3.0f);
        m_panX = (mousePos.x - canvasOrigin.x) / m_zoom - worldBefore.x;
        m_panY = (mousePos.y - canvasOrigin.y) / m_zoom - worldBefore.y;
    }

    if (canvasHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        m_isPanning = true;
        m_lastMousePos = mousePos;
    }
    if (m_isPanning) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            m_panX += (mousePos.x - m_lastMousePos.x) / m_zoom;
            m_panY += (mousePos.y - m_lastMousePos.y) / m_zoom;
            m_lastMousePos = mousePos;
        } else {
            m_isPanning = false;
        }
    }

    auto hitTestNode = [&](const ImVec2& sp, bool& hitResizeHandle) -> WhiteboardNode* {
        hitResizeHandle = false;
        for (int pass = 0; pass < 2; ++pass) {
            bool wantFrame = (pass == 1);
            for (int i = static_cast<int>(board->nodes.size()) - 1; i >= 0; --i) {
                WhiteboardNode& n = board->nodes[static_cast<size_t>(i)];
                bool isFrame = (n.kind == WhiteboardNodeKind::Frame);
                if (isFrame != wantFrame) continue;
                ImVec2 pMin = worldToScreen(canvasOrigin, n.x, n.y);
                ImVec2 pMax = worldToScreen(canvasOrigin, n.x + n.w, n.y + n.h);
                if (sp.x >= pMin.x && sp.x <= pMax.x && sp.y >= pMin.y && sp.y <= pMax.y) {
                    if (sp.x >= pMax.x - 16.0f && sp.y >= pMax.y - 16.0f) {
                        hitResizeHandle = true;
                    }
                    return &n;
                }
            }
        }
        return nullptr;
    };

    if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        bool hitResize = false;
        WhiteboardNode* hit = hitTestNode(mousePos, hitResize);
        if (hit) {
            if (m_connectMode) {
                if (m_connectFromId.empty()) {
                    m_connectFromId = hit->id;
                } else if (m_connectFromId != hit->id) {
                    engine.addEdge(m_connectFromId, hit->id, std::string(m_edgeLabelBuffer));
                    m_connectFromId.clear();
                } else {
                    m_connectFromId.clear();
                }
                m_selectedNodeId = hit->id;
            } else {
                m_selectedNodeId = hit->id;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                    hit->kind == WhiteboardNodeKind::NoteCard && !hit->linkedNoteId.empty()) {
                    if (nm.getNoteById(hit->linkedNoteId)) {
                        app.selectedNoteId = hit->linkedNoteId;
                        ui::pushToast("Nota abierta: " + hit->linkedNoteId, "info");
                    }
                } else if (hitResize) {
                    m_resizingNodeId = hit->id;
                } else {
                    m_draggingNodeId = hit->id;
                    ImVec2 worldMouse = screenToWorld(canvasOrigin, mousePos);
                    m_dragOffsetWorld = ImVec2(worldMouse.x - hit->x, worldMouse.y - hit->y);
                }
            }
        } else {
            m_selectedNodeId.clear();
            m_connectFromId.clear();
        }
    }

    if (!m_draggingNodeId.empty()) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            WhiteboardNode* dn = findNodeMutable(*board, m_draggingNodeId);
            if (dn) {
                ImVec2 wm = screenToWorld(canvasOrigin, mousePos);
                dn->x = wm.x - m_dragOffsetWorld.x;
                dn->y = wm.y - m_dragOffsetWorld.y;
            }
        } else {
            m_draggingNodeId.clear();
        }
    }

    if (!m_resizingNodeId.empty()) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            WhiteboardNode* rn = findNodeMutable(*board, m_resizingNodeId);
            if (rn) {
                ImVec2 wm = screenToWorld(canvasOrigin, mousePos);
                rn->w = std::max(120.0f, wm.x - rn->x);
                rn->h = std::max(70.0f, wm.y - rn->y);
            }
        } else {
            m_resizingNodeId.clear();
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvasOrigin, canvasEnd, true);

    dl->AddRectFilled(canvasOrigin, canvasEnd, ImGui::ColorConvertFloat4ToU32(pal.bgBase));

    float gridStep = 28.0f * m_zoom;
    if (gridStep >= 8.0f) {
        ImU32 dotCol = ImGui::ColorConvertFloat4ToU32(pal.border);
        float offsetX = std::fmod(m_panX * m_zoom, gridStep);
        float offsetY = std::fmod(m_panY * m_zoom, gridStep);
        if (offsetX < 0.0f) offsetX += gridStep;
        if (offsetY < 0.0f) offsetY += gridStep;
        for (float x = canvasOrigin.x + offsetX; x < canvasEnd.x; x += gridStep) {
            for (float y = canvasOrigin.y + offsetY; y < canvasEnd.y; y += gridStep) {
                dl->AddCircleFilled(ImVec2(x, y), 1.3f, dotCol);
            }
        }
    }

    auto drawNode = [&](const WhiteboardNode& n) {
        ImVec2 pMin = worldToScreen(canvasOrigin, n.x, n.y);
        ImVec2 pMax = worldToScreen(canvasOrigin, n.x + n.w, n.y + n.h);
        if (pMax.x < canvasOrigin.x || pMin.x > canvasEnd.x ||
            pMax.y < canvasOrigin.y || pMin.y > canvasEnd.y) {
            return;
        }

        ImVec4 accent = nodeAccentColor(n.colorIndex, pal);
        ImVec4 fill = nodeFillColor(n.kind, n.colorIndex, pal);
        bool selected = (n.id == m_selectedNodeId);
        bool connectSrc = (n.id == m_connectFromId);
        float rounding = (n.kind == WhiteboardNodeKind::Frame) ? 10.0f : 8.0f;

        dl->AddRectFilled(pMin, pMax, ImGui::ColorConvertFloat4ToU32(fill), rounding);

        ImVec4 borderCol = connectSrc ? pal.success : (selected ? accent : pal.borderLine);
        float borderThick = (selected || connectSrc) ? 2.2f : 1.1f;
        dl->AddRect(pMin, pMax, ImGui::ColorConvertFloat4ToU32(borderCol), rounding, 0, borderThick);

        float headerH = std::max(22.0f, 26.0f * m_zoom);
        if (n.kind != WhiteboardNodeKind::Frame) {
            ImVec2 hdrMax(pMax.x, std::min(pMax.y, pMin.y + headerH));
            ImVec4 hdrCol(accent.x, accent.y, accent.z, 0.22f);
            dl->AddRectFilled(pMin, hdrMax, ImGui::ColorConvertFloat4ToU32(hdrCol), rounding, ImDrawFlags_RoundCornersTop);
        }

        dl->PushClipRect(ImVec2(pMin.x + 4.0f, pMin.y + 2.0f), ImVec2(pMax.x - 4.0f, pMax.y - 4.0f), true);

        ImU32 titleCol = ImGui::ColorConvertFloat4ToU32(pal.textPrimary);
        ImU32 bodyCol = ImGui::ColorConvertFloat4ToU32(pal.textSecondary);

        std::string headerTitle = n.title.empty() ? "(Sin titulo)" : n.title;
        if (n.kind == WhiteboardNodeKind::NoteCard && !n.linkedNoteId.empty()) {
            headerTitle = "[[" + n.linkedNoteId + "]]";
        }
        dl->AddText(ImVec2(pMin.x + 8.0f, pMin.y + 5.0f), titleCol, headerTitle.c_str());

        if (!n.body.empty() && n.kind != WhiteboardNodeKind::Frame && (pMax.y - pMin.y) > headerH + 14.0f) {
            float wrapW = std::max(40.0f, (pMax.x - pMin.x) - 16.0f);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(pMin.x + 8.0f, pMin.y + headerH + 6.0f),
                        bodyCol, n.body.c_str(), nullptr, wrapW);
        }

        dl->PopClipRect();

        if (selected) {
            ImVec2 r1(pMax.x - 12.0f, pMax.y - 3.0f);
            ImVec2 r2(pMax.x - 3.0f, pMax.y - 12.0f);
            ImVec2 r3(pMax.x - 3.0f, pMax.y - 3.0f);
            dl->AddTriangleFilled(r1, r2, r3, ImGui::ColorConvertFloat4ToU32(accent));
        }
    };

    for (const auto& n : board->nodes) {
        if (n.kind == WhiteboardNodeKind::Frame) {
            drawNode(n);
        }
    }

    ImU32 edgeCol = ImGui::ColorConvertFloat4ToU32(pal.textSecondary);
    for (const auto& e : board->edges) {
        const WhiteboardNode* fromN = findNodeConst(*board, e.fromId);
        const WhiteboardNode* toN = findNodeConst(*board, e.toId);
        if (!fromN || !toN) continue;

        ImVec2 toCenter(toN->x + toN->w * 0.5f, toN->y + toN->h * 0.5f);
        ImVec2 fromCenter(fromN->x + fromN->w * 0.5f, fromN->y + fromN->h * 0.5f);
        ImVec2 wStart = rectEdgeAnchor(*fromN, toCenter);
        ImVec2 wEnd = rectEdgeAnchor(*toN, fromCenter);

        ImVec2 sStart = worldToScreen(canvasOrigin, wStart.x, wStart.y);
        ImVec2 sEnd = worldToScreen(canvasOrigin, wEnd.x, wEnd.y);
        drawArrowLine(dl, sStart, sEnd, edgeCol, 2.0f);

        if (!e.label.empty()) {
            ImVec2 mid((sStart.x + sEnd.x) * 0.5f, (sStart.y + sEnd.y) * 0.5f);
            ImVec2 ts = ImGui::CalcTextSize(e.label.c_str());
            ImVec2 boxMin(mid.x - ts.x * 0.5f - 6.0f, mid.y - ts.y * 0.5f - 3.0f);
            ImVec2 boxMax(mid.x + ts.x * 0.5f + 6.0f, mid.y + ts.y * 0.5f + 3.0f);
            dl->AddRectFilled(boxMin, boxMax, ImGui::ColorConvertFloat4ToU32(pal.bgPopover), 4.0f);
            dl->AddRect(boxMin, boxMax, ImGui::ColorConvertFloat4ToU32(pal.borderLine), 4.0f);
            dl->AddText(ImVec2(mid.x - ts.x * 0.5f, mid.y - ts.y * 0.5f),
                        ImGui::ColorConvertFloat4ToU32(pal.textPrimary), e.label.c_str());
        }
    }

    if (m_connectMode && !m_connectFromId.empty()) {
        const WhiteboardNode* src = findNodeConst(*board, m_connectFromId);
        if (src) {
            ImVec2 sStart = worldToScreen(canvasOrigin, src->x + src->w * 0.5f, src->y + src->h * 0.5f);
            drawArrowLine(dl, sStart, mousePos, ImGui::ColorConvertFloat4ToU32(pal.accent), 2.0f);
        }
    }

    for (const auto& n : board->nodes) {
        if (n.kind != WhiteboardNodeKind::Frame) {
            drawNode(n);
        }
    }

    dl->AddRect(canvasOrigin, canvasEnd, ImGui::ColorConvertFloat4ToU32(pal.borderLine));
    dl->PopClipRect();

    if (hasSelection) {
        WhiteboardNode* sel = findNodeMutable(*board, m_selectedNodeId);
        if (sel) {
            if (m_inspectorNodeId != sel->id) {
                m_inspectorNodeId = sel->id;
                std::strncpy(m_editTitleBuffer, sel->title.c_str(), sizeof(m_editTitleBuffer) - 1);
                m_editTitleBuffer[sizeof(m_editTitleBuffer) - 1] = '\0';
                std::strncpy(m_editBodyBuffer, sel->body.c_str(), sizeof(m_editBodyBuffer) - 1);
                m_editBodyBuffer[sizeof(m_editBodyBuffer) - 1] = '\0';
                std::strncpy(m_editLinkBuffer, sel->linkedNoteId.c_str(), sizeof(m_editLinkBuffer) - 1);
                m_editLinkBuffer[sizeof(m_editLinkBuffer) - 1] = '\0';
            }

            ImGui::Spacing();
            if (ui::beginCard("##wb_inspector", ImVec2(0, inspectorHeight - 4.0f), false)) {
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::InputTextWithHint("##wb_ed_title", "Titulo", m_editTitleBuffer, sizeof(m_editTitleBuffer))) {
                    sel->title = std::string(m_editTitleBuffer);
                }

                ImGui::SameLine(0.0f, 8.0f);
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::InputTextWithHint("##wb_ed_link", "Nota enlazada", m_editLinkBuffer, sizeof(m_editLinkBuffer))) {
                    sel->linkedNoteId = std::string(m_editLinkBuffer);
                }

                ImGui::SameLine(0.0f, 10.0f);
                for (int ci = 0; ci < 6; ++ci) {
                    if (ci > 0) ImGui::SameLine(0.0f, 4.0f);
                    ImVec4 c = nodeAccentColor(ci, pal);
                    ImGui::PushID(ci);
                    ImGui::PushStyleColor(ImGuiCol_Button, c);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
                    if (ImGui::Button(sel->colorIndex == ci ? "*" : " ", ImVec2(22.0f, 22.0f))) {
                        sel->colorIndex = ci;
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::PopID();
                }

                ImGui::SameLine(0.0f, 12.0f);
                if (ui::dangerButton("Eliminar nodo")) {
                    std::string toRemove = sel->id;
                    m_selectedNodeId.clear();
                    m_inspectorNodeId.clear();
                    engine.removeNode(toRemove);
                    sel = nullptr;
                }

                if (sel) {
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::InputTextMultiline("##wb_ed_body", m_editBodyBuffer, sizeof(m_editBodyBuffer),
                            ImVec2(-1.0f, 52.0f))) {
                        sel->body = std::string(m_editBodyBuffer);
                    }
                }
            }
            ui::endCard();
        }
    }

    ImGui::End();
}

}
