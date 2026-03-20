// ============================================================================
// GraphPanel — Visualización del grafo de nodos (modos 2D y 3D)
// ============================================================================

#include "graph_panel.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <numeric>

namespace nodepanda {

// ── Utilidades de color ─────────────────────────────────────────────────────

static ImVec4 typeColor(const std::string& tipo) {
    if (tipo == "proyecto")    return ImVec4(0.15f, 0.85f, 0.90f, 0.95f);
    if (tipo == "concepto")    return ImVec4(0.62f, 0.38f, 0.88f, 0.95f);
    if (tipo == "referencia")  return ImVec4(0.90f, 0.72f, 0.20f, 0.95f);
    if (tipo == "diario")      return ImVec4(0.40f, 0.82f, 0.45f, 0.95f);
    if (tipo == "tarea")       return ImVec4(0.95f, 0.45f, 0.35f, 0.95f);
    return ImVec4(-1, -1, -1, -1);
}

static ImVec4 connectivityColor(int count) {
    if (count <= 0) return ImVec4(0.32f, 0.38f, 0.55f, 0.90f);
    if (count == 1) return ImVec4(0.30f, 0.52f, 0.72f, 0.90f);
    if (count <= 3) return ImVec4(0.20f, 0.68f, 0.72f, 0.92f);
    if (count <= 5) return ImVec4(0.15f, 0.80f, 0.82f, 0.95f);
    return              ImVec4(0.45f, 0.90f, 0.38f, 0.95f);
}

static ImVec4 nodeColor(const GraphNode& node) {
    ImVec4 tc = typeColor(node.nodeType);
    if (tc.x >= 0.0f) return tc;
    return connectivityColor(node.connectionCount);
}

static ImU32 toCol32(ImVec4 c, float alphaOverride = -1.0f) {
    return IM_COL32(
        (int)(c.x * 255),
        (int)(c.y * 255),
        (int)(c.z * 255),
        (int)((alphaOverride >= 0.0f ? alphaOverride : c.w) * 255)
    );
}

// ============================================================================
// render — Punto de entrada con toggle 2D/3D
// ============================================================================
void GraphPanel::render(App& app) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Grafo de Nodos", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    auto& graph = app.getGraph();
    auto& nodes = graph.getNodes();
    auto& edges = graph.getEdges();

    // ── Toolbar ─────────────────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::SetCursorPos(ImVec2(8.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.48f, 0.60f, 1.0f));
    ImGui::Text("Nodos: %d  |  Aristas: %d  |  Zoom: %.0f%%  |  %s",
                (int)nodes.size(), (int)edges.size(),
                m_zoom * 100.0f,
                m_mode3D ? "3D" : "2D");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 16.0f);

    if (ImGui::SmallButton("Reset")) {
        m_zoom = 1.0f;
        m_panX = 0.0f;
        m_panY = 0.0f;
        m_autoCenter = true;
        m_camYaw = 0.4f;
        m_camPitch = 0.3f;
        m_camDist = 600.0f;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Restablecer camara y zoom");

    ImGui::SameLine(0.0f, 8.0f);

    // Toggle 2D/3D
    if (m_mode3D) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.25f, 0.60f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.60f, 0.35f, 0.70f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.35f, 1.0f));
    }
    if (ImGui::SmallButton(m_mode3D ? "Modo: 3D" : "Modo: 2D")) {
        m_mode3D = !m_mode3D;
        // Sincronizar con force layout
        app.getForceLayout().mode3D = m_mode3D;
        app.getForceLayout().reset();
        app.getForceLayout().initialize(app.getGraph(), 800.0f, 600.0f);
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(m_mode3D
            ? "Cambiar a vista 2D\nArrastrar = rotar | Scroll = zoom"
            : "Cambiar a vista 3D con perspectiva");

    ImGui::SameLine(0.0f, 8.0f);

    if (!m_mode3D) {
        if (m_autoCenter) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.50f, 0.48f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.60f, 0.55f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.35f, 1.0f));
        }
        if (ImGui::SmallButton(m_autoCenter ? "Auto-Center: ON" : "Auto-Center: OFF")) {
            m_autoCenter = !m_autoCenter;
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Centrar automaticamente en el centroide");
    }

    ImGui::PopStyleVar();
    ImGui::Spacing();

    if (nodes.empty()) {
        ImGui::SetCursorPosX(20.0f);
        ImGui::TextColored(ImVec4(0.30f, 0.32f, 0.42f, 1.0f),
                           "Crea notas con [[enlaces]] para ver el grafo.");
        ImGui::End();
        return;
    }

    if (m_mode3D)
        render3D(app);
    else
        render2D(app);

    ImGui::End();
}

// ============================================================================
// render2D — Vista 2D existente
// ============================================================================
void GraphPanel::render2D(App& app) {
    auto& graph = app.getGraph();
    auto& nodes = graph.getNodes();
    auto& edges = graph.getEdges();
    ImGuiIO& io = ImGui::GetIO();

    if (m_autoCenter && !nodes.empty()) {
        float avgX = 0.0f, avgY = 0.0f;
        for (const auto& node : nodes) { avgX += node.x; avgY += node.y; }
        avgX /= (float)nodes.size();
        avgY /= (float)nodes.size();
        float targetPanX = -avgX * m_zoom;
        float targetPanY = -avgY * m_zoom;
        m_panX += (targetPanX - m_panX) * 0.08f;
        m_panY += (targetPanY - m_panY) * 0.08f;
    }

    ImVec2 canvasOrig = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.x = std::max(canvasSize.x, 50.0f);
    canvasSize.y = std::max(canvasSize.y, 50.0f);
    ImVec2 canvasEnd { canvasOrig.x + canvasSize.x, canvasOrig.y + canvasSize.y };

    ImGui::InvisibleButton("##canvas", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mousePos = io.MousePos;

    if (hovered && io.MouseWheel != 0.0f) {
        ImVec2 mouseLocal {
            io.MousePos.x - canvasOrig.x - canvasSize.x * 0.5f - m_panX,
            io.MousePos.y - canvasOrig.y - canvasSize.y * 0.5f - m_panY
        };
        float factor = 1.0f + io.MouseWheel * 0.09f;
        float newZoom = std::max(0.15f, std::min(6.0f, m_zoom * factor));
        float zoomRatio = newZoom / m_zoom;
        m_panX -= mouseLocal.x * (zoomRatio - 1.0f);
        m_panY -= mouseLocal.y * (zoomRatio - 1.0f);
        m_zoom = newZoom;
    }

    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
        m_panX += io.MouseDelta.x;
        m_panY += io.MouseDelta.y;
        m_autoCenter = false;
    }

    auto graphToScreen = [&](float gx, float gy) -> ImVec2 {
        return {
            canvasOrig.x + canvasSize.x * 0.5f + gx * m_zoom + m_panX,
            canvasOrig.y + canvasSize.y * 0.5f + gy * m_zoom + m_panY
        };
    };
    auto screenToGraph = [&](float sx, float sy) -> ImVec2 {
        return {
            (sx - canvasOrig.x - canvasSize.x * 0.5f - m_panX) / m_zoom,
            (sy - canvasOrig.y - canvasSize.y * 0.5f - m_panY) / m_zoom
        };
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvasOrig, canvasEnd, true);
    dl->AddRectFilled(canvasOrig, canvasEnd, IM_COL32(10, 11, 16, 255));

    // Grid
    {
        float minorStep = 30.0f * m_zoom;
        if (minorStep >= 4.0f) {
            float ox = fmodf(m_panX + canvasSize.x * 0.5f, minorStep);
            float oy = fmodf(m_panY + canvasSize.y * 0.5f, minorStep);
            ImU32 minorCol = IM_COL32(22, 24, 36, 90);
            for (float x = ox; x < canvasSize.x; x += minorStep)
                dl->AddLine({ canvasOrig.x + x, canvasOrig.y }, { canvasOrig.x + x, canvasEnd.y }, minorCol);
            for (float y = oy; y < canvasSize.y; y += minorStep)
                dl->AddLine({ canvasOrig.x, canvasOrig.y + y }, { canvasEnd.x, canvasOrig.y + y }, minorCol);
        }
        float majorStep = 150.0f * m_zoom;
        if (majorStep >= 8.0f) {
            float ox = fmodf(m_panX + canvasSize.x * 0.5f, majorStep);
            float oy = fmodf(m_panY + canvasSize.y * 0.5f, majorStep);
            ImU32 majorCol = IM_COL32(28, 32, 50, 130);
            for (float x = ox; x < canvasSize.x; x += majorStep)
                dl->AddLine({ canvasOrig.x + x, canvasOrig.y }, { canvasOrig.x + x, canvasEnd.y }, majorCol, 1.5f);
            for (float y = oy; y < canvasSize.y; y += majorStep)
                dl->AddLine({ canvasOrig.x, canvasOrig.y + y }, { canvasEnd.x, canvasOrig.y + y }, majorCol, 1.5f);
        }
    }

    const float CULL_MARGIN = 80.0f;
    auto isOnScreen = [&](ImVec2 sp, float radius) -> bool {
        return sp.x + radius + CULL_MARGIN >= canvasOrig.x &&
               sp.x - radius - CULL_MARGIN <= canvasEnd.x &&
               sp.y + radius + CULL_MARGIN >= canvasOrig.y &&
               sp.y - radius - CULL_MARGIN <= canvasEnd.y;
    };

    int lodLevel;
    int nodeCount = (int)nodes.size();
    if (m_zoom >= 0.5f || nodeCount <= 200) lodLevel = 0;
    else if (m_zoom >= 0.25f || nodeCount <= 1000) lodLevel = 1;
    else lodLevel = 2;
    int circleSegments = (lodLevel == 0) ? 32 : (lodLevel == 1) ? 16 : 8;

    // Edges
    for (const auto& edge : edges) {
        if (edge.from < 0 || edge.from >= (int)nodes.size()) continue;
        if (edge.to < 0 || edge.to >= (int)nodes.size()) continue;
        const auto& a = nodes[edge.from];
        const auto& b = nodes[edge.to];
        ImVec2 p1 = graphToScreen(a.x, a.y);
        ImVec2 p2 = graphToScreen(b.x, b.y);

        if ((p1.x < canvasOrig.x - CULL_MARGIN && p2.x < canvasOrig.x - CULL_MARGIN) ||
            (p1.x > canvasEnd.x + CULL_MARGIN && p2.x > canvasEnd.x + CULL_MARGIN) ||
            (p1.y < canvasOrig.y - CULL_MARGIN && p2.y < canvasOrig.y - CULL_MARGIN) ||
            (p1.y > canvasEnd.y + CULL_MARGIN && p2.y > canvasEnd.y + CULL_MARGIN))
            continue;

        bool dimA = (a.noteId == app.selectedNoteId);
        bool dimB = (b.noteId == app.selectedNoteId);
        ImU32 edgeCol;
        if (dimA || dimB) {
            ImVec4 colA = nodeColor(a);
            ImVec4 colB = nodeColor(b);
            edgeCol = IM_COL32(
                (int)((colA.x + colB.x) * 0.5f * 255),
                (int)((colA.y + colB.y) * 0.5f * 255),
                (int)((colA.z + colB.z) * 0.5f * 255), 200);
        } else {
            edgeCol = IM_COL32(45, 55, 90, 100);
        }
        float thickness = (dimA || dimB)
            ? std::max(0.8f, 1.8f * m_zoom) : std::max(0.5f, 1.0f * m_zoom);
        dl->AddLine(p1, p2, edgeCol, thickness);
    }

    // Nodes
    int hoveredNode = -1;
    int nodesDrawn = 0;

    for (int i = 0; i < (int)nodes.size(); ++i) {
        const auto& node = nodes[i];
        ImVec2 sp = graphToScreen(node.x, node.y);
        float sr = node.radius * m_zoom;
        if (!isOnScreen(sp, sr)) continue;
        ++nodesDrawn;

        float dx = mousePos.x - sp.x;
        float dy = mousePos.y - sp.y;
        bool nodeHov = (dx * dx + dy * dy) <= ((sr + 4) * (sr + 4)) && hovered;
        if (nodeHov) hoveredNode = i;

        bool isSelected = (node.noteId == app.selectedNoteId);
        ImVec4 col4 = nodeColor(node);
        ImU32 col = toCol32(col4);

        if (lodLevel == 0) {
            float extraGlow = std::min((float)node.connectionCount * 2.5f, 18.0f) * m_zoom;
            if (isSelected) {
                dl->AddCircleFilled(sp, sr + extraGlow + 14.0f * m_zoom, toCol32(col4, 0.07f), circleSegments);
                dl->AddCircleFilled(sp, sr + extraGlow + 8.0f * m_zoom, toCol32(col4, 0.14f), circleSegments);
                dl->AddCircleFilled(sp, sr + extraGlow + 4.0f * m_zoom, toCol32(col4, 0.22f), circleSegments);
            }
            if (extraGlow > 1.0f) {
                dl->AddCircleFilled(sp, sr + extraGlow, toCol32(col4, 0.08f), circleSegments);
                dl->AddCircleFilled(sp, sr + extraGlow * 0.55f, toCol32(col4, 0.18f), circleSegments);
                dl->AddCircleFilled(sp, sr + extraGlow * 0.25f, toCol32(col4, 0.30f), circleSegments);
            }
        }

        if (nodeHov) {
            dl->AddCircleFilled(sp, sr + 6.0f * m_zoom, IM_COL32(240, 245, 255, 28), circleSegments);
        }
        dl->AddCircleFilled(sp, sr, col, circleSegments);

        if (lodLevel <= 1) {
            ImU32 borderCol = isSelected ? IM_COL32(150, 225, 220, 230) : toCol32(col4, 0.55f);
            float borderW = isSelected ? 1.8f : 1.2f;
            dl->AddCircle(sp, sr, borderCol, circleSegments, borderW);
        }

        if (lodLevel <= 1) {
            float labelAlpha = m_zoom >= 0.6f ? 1.0f : m_zoom >= 0.4f ? (m_zoom - 0.4f) / 0.2f : 0.0f;
            if (labelAlpha > 0.01f) {
                const char* label = node.noteId.c_str();
                ImVec2 textSz = ImGui::CalcTextSize(label);
                ImVec2 textPos { sp.x - textSz.x * 0.5f, sp.y + sr + 15.0f };
                dl->AddText(ImVec2(textPos.x + 1, textPos.y + 1),
                            IM_COL32(0, 0, 0, (int)(140 * labelAlpha)), label);
                ImU32 textCol = isSelected
                    ? IM_COL32(180, 235, 230, (int)(240 * labelAlpha))
                    : IM_COL32(170, 175, 200, (int)(190 * labelAlpha));
                dl->AddText(textPos, textCol, label);
            }
        }
    }

    m_lodLevel = lodLevel;
    m_nodesVisible = nodesDrawn;

    // Invalidar m_draggedNode si fuera de rango (por rebuild de grafo)
    if (m_draggedNode >= (int)nodes.size()) m_draggedNode = -1;

    // Interacción
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_draggedNode = hoveredNode;
        if (hoveredNode >= 0 && hoveredNode < (int)nodes.size())
            app.selectedNoteId = nodes[hoveredNode].noteId;
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f) && m_draggedNode >= 0 && m_draggedNode < (int)nodes.size()) {
        auto& n = graph.getNodes()[m_draggedNode];
        auto gp = screenToGraph(mousePos.x, mousePos.y);
        n.x = gp.x;
        n.y = gp.y;
        n.pinned = true;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (m_draggedNode >= 0 && m_draggedNode < (int)graph.getNodes().size())
            graph.getNodes()[m_draggedNode].pinned = false;
        m_draggedNode = -1;
    }

    // Leyenda
    {
        const float ox = canvasEnd.x - 155.0f;
        float oy = canvasOrig.y + 8.0f;
        dl->AddRectFilled({ ox - 6, oy - 4 }, { canvasEnd.x - 4, oy + 130 },
                          IM_COL32(8, 9, 14, 200), 5.0f);

        struct LegendEntry { const char* label; ImU32 col; };
        LegendEntry legend[] = {
            { "proyecto",    toCol32(typeColor("proyecto"))   },
            { "concepto",    toCol32(typeColor("concepto"))   },
            { "referencia",  toCol32(typeColor("referencia")) },
            { "diario",      toCol32(typeColor("diario"))     },
            { "tarea",       toCol32(typeColor("tarea"))      },
            { "0-1 enlaces", toCol32(connectivityColor(0))    },
            { "6+  enlaces", toCol32(connectivityColor(6))    },
        };
        for (auto& e : legend) {
            dl->AddCircleFilled({ ox + 8, oy + 7 }, 5.0f, e.col, 20);
            dl->AddText({ ox + 18, oy }, IM_COL32(140, 145, 170, 200), e.label);
            oy += 18.0f;
        }
    }

    dl->PopClipRect();

    // Tooltip
    if (hoveredNode >= 0 && hoveredNode < (int)nodes.size() && m_draggedNode < 0) {
        const auto& node = nodes[hoveredNode];
        auto* note = app.getNoteManager().getNoteById(node.noteId);
        ImGui::BeginTooltip();
        ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "%s", node.noteId.c_str());
        if (note) {
            ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.65f, 1.0f),
                               "Conexiones: %d", node.connectionCount);
            if (!node.nodeType.empty()) {
                ImVec4 tc = typeColor(node.nodeType);
                if (tc.x >= 0.0f) ImGui::TextColored(tc, "Tipo: %s", node.nodeType.c_str());
            }
            std::string preview = note->content.substr(0, 180);
            if (note->content.size() > 180) preview += "...";
            if (!preview.empty()) {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.70f, 0.80f, 1.0f));
                ImGui::TextWrapped("%s", preview.c_str());
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::TextColored(ImVec4(0.88f, 0.48f, 0.32f, 1.0f), "(Nota no creada aun)");
        }
        ImGui::EndTooltip();
    }
}

// ============================================================================
// render3D — Vista 3D con proyección perspectiva y rotación de cámara
// ============================================================================
void GraphPanel::render3D(App& app) {
    auto& graph = app.getGraph();
    auto& nodes = graph.getNodes();
    auto& edges = graph.getEdges();
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 canvasOrig = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.x = std::max(canvasSize.x, 50.0f);
    canvasSize.y = std::max(canvasSize.y, 50.0f);
    ImVec2 canvasEnd { canvasOrig.x + canvasSize.x, canvasOrig.y + canvasSize.y };

    ImGui::InvisibleButton("##canvas3d", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mousePos = io.MousePos;

    // ── Controles de cámara ─────────────────────────────────────────────────
    // Click izquierdo arrastrar = rotar
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f) && m_draggedNode < 0) {
        m_camYaw   += io.MouseDelta.x * 0.005f;
        m_camPitch += io.MouseDelta.y * 0.005f;
        // Limitar pitch para evitar gimbal lock
        if (m_camPitch > 1.5f) m_camPitch = 1.5f;
        if (m_camPitch < -1.5f) m_camPitch = -1.5f;
    }

    // Click medio arrastrar = pan (ajustar distancia y FOV)
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
        m_camDist += io.MouseDelta.y * 2.0f;
        if (m_camDist < 100.0f) m_camDist = 100.0f;
        if (m_camDist > 3000.0f) m_camDist = 3000.0f;
    }

    // Scroll = zoom (ajustar distancia)
    if (hovered && io.MouseWheel != 0.0f) {
        m_camDist -= io.MouseWheel * 40.0f;
        if (m_camDist < 100.0f) m_camDist = 100.0f;
        if (m_camDist > 3000.0f) m_camDist = 3000.0f;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvasOrig, canvasEnd, true);

    // Fondo con gradiente sutil
    dl->AddRectFilled(canvasOrig, canvasEnd, IM_COL32(6, 7, 12, 255));

    // ── Proyectar todos los nodos ───────────────────────────────────────────
    struct NodeProj {
        int   idx;
        float sx, sy, depth, scale;
    };
    std::vector<NodeProj> projNodes;
    projNodes.reserve(nodes.size());

    for (int i = 0; i < (int)nodes.size(); ++i) {
        const auto& node = nodes[i];
        auto p = project3D(node.x, node.y, node.z,
                          canvasOrig.x, canvasOrig.y, canvasSize.x, canvasSize.y);
        projNodes.push_back({ i, p.sx, p.sy, p.depth, p.scale });
    }

    // Ordenar por profundidad (más lejano primero = pintor's algorithm)
    std::vector<int> drawOrder(projNodes.size());
    std::iota(drawOrder.begin(), drawOrder.end(), 0);
    std::sort(drawOrder.begin(), drawOrder.end(), [&](int a, int b) {
        return projNodes[a].depth > projNodes[b].depth;
    });

    // ── Dibujar aristas ─────────────────────────────────────────────────────
    for (const auto& edge : edges) {
        if (edge.from < 0 || edge.from >= (int)nodes.size() || edge.from >= (int)projNodes.size()) continue;
        if (edge.to < 0 || edge.to >= (int)nodes.size() || edge.to >= (int)projNodes.size()) continue;

        const auto& pa = projNodes[edge.from];
        const auto& pb = projNodes[edge.to];

        // Alpha basado en profundidad promedio
        float avgDepth = (pa.depth + pb.depth) * 0.5f;
        float depthAlpha = std::max(0.05f, std::min(1.0f, 1.0f - (avgDepth - 200.0f) / 1500.0f));

        bool sel = (nodes[edge.from].noteId == app.selectedNoteId) ||
                   (nodes[edge.to].noteId == app.selectedNoteId);

        int alpha = (int)(depthAlpha * (sel ? 180.0f : 80.0f));
        ImU32 edgeCol;
        if (sel) {
            ImVec4 colA = nodeColor(nodes[edge.from]);
            ImVec4 colB = nodeColor(nodes[edge.to]);
            edgeCol = IM_COL32(
                (int)((colA.x + colB.x) * 0.5f * 255),
                (int)((colA.y + colB.y) * 0.5f * 255),
                (int)((colA.z + colB.z) * 0.5f * 255), alpha);
        } else {
            edgeCol = IM_COL32(45, 55, 90, alpha);
        }

        float thickness = std::max(0.5f, 1.2f * (pa.scale + pb.scale) * 0.5f * 15.0f);
        dl->AddLine({ pa.sx, pa.sy }, { pb.sx, pb.sy }, edgeCol, thickness);
    }

    // ── Dibujar nodos (orden de profundidad) ────────────────────────────────
    int hoveredNode = -1;

    for (int di : drawOrder) {
        const auto& pn = projNodes[di];
        const auto& node = nodes[pn.idx];

        float sr = node.radius * pn.scale * 15.0f; // escalar por perspectiva
        if (sr < 1.0f) sr = 1.0f;
        if (sr > 60.0f) sr = 60.0f;

        // Profundidad → alpha
        float depthAlpha = std::max(0.15f, std::min(1.0f, 1.0f - (pn.depth - 200.0f) / 1500.0f));

        // Culling
        if (pn.sx + sr < canvasOrig.x || pn.sx - sr > canvasEnd.x ||
            pn.sy + sr < canvasOrig.y || pn.sy - sr > canvasEnd.y)
            continue;

        // Hit test
        float dx = mousePos.x - pn.sx;
        float dy = mousePos.y - pn.sy;
        if (hovered && (dx * dx + dy * dy) <= ((sr + 4) * (sr + 4))) {
            hoveredNode = pn.idx;
        }

        bool isSelected = (node.noteId == app.selectedNoteId);
        ImVec4 col4 = nodeColor(node);

        int segments = (sr > 10.0f) ? 24 : (sr > 4.0f) ? 16 : 8;

        // Halo para nodo seleccionado
        if (isSelected) {
            dl->AddCircleFilled({ pn.sx, pn.sy }, sr + 10.0f * depthAlpha,
                                toCol32(col4, 0.12f * depthAlpha), segments);
            dl->AddCircleFilled({ pn.sx, pn.sy }, sr + 5.0f * depthAlpha,
                                toCol32(col4, 0.25f * depthAlpha), segments);
        }

        // Nodo
        dl->AddCircleFilled({ pn.sx, pn.sy }, sr,
                            toCol32(col4, depthAlpha * col4.w), segments);

        // Borde
        ImU32 borderCol = isSelected
            ? IM_COL32(150, 225, 220, (int)(200 * depthAlpha))
            : toCol32(col4, 0.4f * depthAlpha);
        dl->AddCircle({ pn.sx, pn.sy }, sr, borderCol, segments, 1.2f);

        // Label (solo si suficientemente grande y cercano)
        if (sr > 6.0f && depthAlpha > 0.3f) {
            const char* label = node.noteId.c_str();
            ImVec2 textSz = ImGui::CalcTextSize(label);
            ImVec2 textPos { pn.sx - textSz.x * 0.5f, pn.sy + sr + 4.0f };

            int textAlpha = (int)(depthAlpha * 190.0f);
            dl->AddText(ImVec2(textPos.x + 1, textPos.y + 1),
                        IM_COL32(0, 0, 0, textAlpha / 2), label);
            ImU32 textCol = isSelected
                ? IM_COL32(180, 235, 230, textAlpha)
                : IM_COL32(170, 175, 200, textAlpha);
            dl->AddText(textPos, textCol, label);
        }
    }

    // ── Indicador de ejes 3D (esquina inferior izquierda) ───────────────────
    {
        float axLen = 30.0f;
        float bx = canvasOrig.x + 40.0f;
        float by = canvasEnd.y - 40.0f;
        float cosY = std::cos(m_camYaw), sinY = std::sin(m_camYaw);
        float cosP = std::cos(m_camPitch), sinP = std::sin(m_camPitch);

        // Eje X (rojo)
        float x1 = axLen * cosY, z1 = axLen * -sinY;
        dl->AddLine({ bx, by }, { bx + x1, by }, IM_COL32(220, 60, 60, 200), 2.0f);
        dl->AddText({ bx + x1 + 2, by - 6 }, IM_COL32(220, 60, 60, 200), "X");

        // Eje Y (verde)
        float y2 = -axLen * cosP;
        dl->AddLine({ bx, by }, { bx, by + y2 }, IM_COL32(60, 220, 60, 200), 2.0f);
        dl->AddText({ bx + 4, by + y2 - 6 }, IM_COL32(60, 220, 60, 200), "Y");

        // Eje Z (azul)
        float x3 = axLen * sinY, y3 = -axLen * sinP;
        dl->AddLine({ bx, by }, { bx + x3, by + y3 }, IM_COL32(60, 120, 220, 200), 2.0f);
        dl->AddText({ bx + x3 + 2, by + y3 - 6 }, IM_COL32(60, 120, 220, 200), "Z");
    }

    // ── Leyenda ─────────────────────────────────────────────────────────────
    {
        const float ox = canvasEnd.x - 155.0f;
        float oy = canvasOrig.y + 8.0f;
        dl->AddRectFilled({ ox - 6, oy - 4 }, { canvasEnd.x - 4, oy + 58 },
                          IM_COL32(8, 9, 14, 200), 5.0f);
        dl->AddText({ ox, oy }, IM_COL32(140, 145, 170, 200), "Arrastrar: rotar");
        oy += 16.0f;
        dl->AddText({ ox, oy }, IM_COL32(140, 145, 170, 200), "Scroll: zoom");
        oy += 16.0f;
        dl->AddText({ ox, oy }, IM_COL32(140, 145, 170, 200), "Click: seleccionar");
    }

    dl->PopClipRect();

    // ── Interacción: click para seleccionar ─────────────────────────────────
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredNode >= 0 && hoveredNode < (int)nodes.size()) {
        app.selectedNoteId = nodes[hoveredNode].noteId;
        m_draggedNode = -1; // no drag en 3D por ahora
    }

    // ── Tooltip ─────────────────────────────────────────────────────────────
    if (hoveredNode >= 0 && hoveredNode < (int)nodes.size()) {
        const auto& node = nodes[hoveredNode];
        auto* note = app.getNoteManager().getNoteById(node.noteId);
        ImGui::BeginTooltip();
        ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "%s", node.noteId.c_str());
        if (note) {
            ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.65f, 1.0f),
                               "Conexiones: %d | Pos: (%.0f, %.0f, %.0f)",
                               node.connectionCount, node.x, node.y, node.z);
            std::string preview = note->content.substr(0, 120);
            if (note->content.size() > 120) preview += "...";
            if (!preview.empty()) {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.70f, 0.80f, 1.0f));
                ImGui::TextWrapped("%s", preview.c_str());
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTooltip();
    }
}

} // namespace nodepanda
