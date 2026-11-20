#include "graph_panel.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <numeric>
#include <queue>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace nodepanda {

namespace {

char g_graphSearchBuf[128] = {};
int g_graphDepthFilter = 0;

std::string toLowerStr(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool nodeMatchesSearch(const GraphNode& node, const Note* note, const std::string& queryLower) {
    if (queryLower.empty()) return false;
    if (toLowerStr(node.noteId).find(queryLower) != std::string::npos) return true;
    if (note && toLowerStr(note->title).find(queryLower) != std::string::npos) return true;
    return false;
}

std::vector<bool> computeVisibleMask(const std::vector<GraphNode>& nodes,
                                     const std::vector<GraphEdge>& edges,
                                     const std::string& selectedId,
                                     int maxDepth) {
    std::vector<bool> visible(nodes.size(), true);
    if (maxDepth <= 0 || selectedId.empty() || nodes.empty()) {
        return visible;
    }

    int startIdx = -1;
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        if (nodes[i].noteId == selectedId) {
            startIdx = i;
            break;
        }
    }
    if (startIdx < 0) {
        return visible;
    }

    std::vector<std::vector<int>> adj(nodes.size());
    for (const auto& e : edges) {
        if (e.from >= 0 && e.from < static_cast<int>(nodes.size()) &&
            e.to >= 0 && e.to < static_cast<int>(nodes.size())) {
            adj[e.from].push_back(e.to);
            adj[e.to].push_back(e.from);
        }
    }

    std::fill(visible.begin(), visible.end(), false);
    std::vector<int> dist(nodes.size(), -1);
    std::queue<int> q;
    dist[startIdx] = 0;
    visible[startIdx] = true;
    q.push(startIdx);

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        if (dist[u] >= maxDepth) continue;
        for (int v : adj[u]) {
            if (dist[v] == -1) {
                dist[v] = dist[u] + 1;
                visible[v] = true;
                q.push(v);
            }
        }
    }
    return visible;
}

ImVec4 typeColor(const std::string& tipo, const ThemePalette& pal) {
    if (tipo == "proyecto")   return pal.aqua;
    if (tipo == "concepto")   return pal.proViolet;
    if (tipo == "referencia") return pal.warning;
    if (tipo == "diario")     return pal.success;
    if (tipo == "tarea")      return pal.orange;
    return ImVec4(-1.0f, -1.0f, -1.0f, -1.0f);
}

ImVec4 connectivityColor(int count, const ThemePalette& pal) {
    if (count <= 0) return pal.textMuted;
    if (count == 1) return pal.accent;
    if (count <= 3) return pal.aqua;
    if (count <= 5) return pal.proViolet;
    return pal.success;
}

ImVec4 nodeColor(const GraphNode& node, const ThemePalette& pal) {
    ImVec4 tc = typeColor(node.nodeType, pal);
    if (tc.x >= 0.0f) return tc;
    return connectivityColor(node.connectionCount, pal);
}

ImU32 toCol32(ImVec4 c, float alphaOverride = -1.0f) {
    float a = (alphaOverride >= 0.0f) ? alphaOverride : c.w;
    a = std::max(0.0f, std::min(1.0f, a));
    return IM_COL32(
        static_cast<int>(c.x * 255.0f),
        static_cast<int>(c.y * 255.0f),
        static_cast<int>(c.z * 255.0f),
        static_cast<int>(a * 255.0f)
    );
}

}

void GraphPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Grafo de Nodos", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    auto& graph = app.getGraph();
    auto& nodes = graph.getNodes();
    auto& edges = graph.getEdges();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bgSurface1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
    ImGui::BeginChild("##graph_toolbar", ImVec2(0.0f, 44.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    int modeIdx = m_mode3D ? 1 : 0;
    if (ui::segmentedControl("##graph_mode_seg", &modeIdx, {"2D", "3D"}, 92.0f)) {
        m_mode3D = (modeIdx == 1);
        app.getForceLayout().mode3D = m_mode3D;
        app.getForceLayout().reset();
        app.getForceLayout().initialize(app.getGraph(), 800.0f, 600.0f);
    }

    ImGui::SameLine(0.0f, 10.0f);
    char nodeBadge[48];
    char edgeBadge[48];
    char zoomBadge[48];
    std::snprintf(nodeBadge, sizeof(nodeBadge), "Nodos: %d", static_cast<int>(nodes.size()));
    std::snprintf(edgeBadge, sizeof(edgeBadge), "Aristas: %d", static_cast<int>(edges.size()));
    if (m_mode3D) {
        std::snprintf(zoomBadge, sizeof(zoomBadge), "Dist: %.0f", m_camDist);
    } else {
        std::snprintf(zoomBadge, sizeof(zoomBadge), "%.0f%%", m_zoom * 100.0f);
    }

    ui::badge(nodeBadge, pal.bgSurface3, pal.textPrimary);
    ImGui::SameLine(0.0f, 6.0f);
    ui::badge(edgeBadge, pal.bgSurface3, pal.textSecondary);
    ImGui::SameLine(0.0f, 6.0f);
    ui::badge(zoomBadge, pal.accentSoft, pal.accent);

    ImGui::SameLine(0.0f, 10.0f);
    const char* depthLabel = "Filtro: Todo";
    if (g_graphDepthFilter == 1) depthLabel = "Filtro: 1 salto";
    else if (g_graphDepthFilter == 2) depthLabel = "Filtro: 2 saltos";

    if (ui::subtleButton(depthLabel)) {
        g_graphDepthFilter = (g_graphDepthFilter + 1) % 3;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Alternar profundidad local alrededor de la nota activa");
    }

    if (!m_mode3D) {
        ImGui::SameLine(0.0f, 6.0f);
        if (ui::subtleButton(m_autoCenter ? "Auto-Center: ON" : "Auto-Center: OFF")) {
            m_autoCenter = !m_autoCenter;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Centrar automaticamente en el centroide");
        }
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (ui::subtleButton("Reset")) {
        m_zoom = 1.0f;
        m_panX = 0.0f;
        m_panY = 0.0f;
        m_autoCenter = true;
        m_camYaw = 0.4f;
        m_camPitch = 0.3f;
        m_camDist = 600.0f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Restablecer camara y zoom");
    }

    ImGui::SameLine(0.0f, 10.0f);
    float searchW = std::max(110.0f, std::min(190.0f, ImGui::GetContentRegionAvail().x - 32.0f));
    ImGui::SetNextItemWidth(searchW);
    bool searchEnter = ImGui::InputTextWithHint("##graph_search", "Buscar nodo...",
                                                g_graphSearchBuf, sizeof(g_graphSearchBuf),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
    if (searchEnter && g_graphSearchBuf[0] != '\0') {
        std::string qLower = toLowerStr(g_graphSearchBuf);
        for (const auto& n : nodes) {
            Note* note = app.getNoteManager().getNoteById(n.noteId);
            if (nodeMatchesSearch(n, note, qLower)) {
                app.selectedNoteId = n.noteId;
                if (!m_mode3D) {
                    m_panX = -n.x * m_zoom;
                    m_panY = -n.y * m_zoom;
                    m_autoCenter = false;
                }
                break;
            }
        }
    }

    if (g_graphSearchBuf[0] != '\0') {
        ImGui::SameLine(0.0f, 4.0f);
        if (ui::subtleButton("X##clear_gsearch")) {
            g_graphSearchBuf[0] = '\0';
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Separator, pal.borderLine);
    ImGui::Separator();
    ImGui::PopStyleColor();

    if (nodes.empty()) {
        ImGui::SetCursorPos(ImVec2(20.0f, 64.0f));
        ImGui::TextColored(pal.textMuted, "Crea notas con [[enlaces]] para ver el grafo.");
        ImGui::End();
        return;
    }

    if (m_mode3D)
        render3D(app);
    else
        render2D(app);

    ImGui::End();
}

void GraphPanel::render2D(App& app) {
    const ThemePalette& pal = UITheme::palette();
    auto& graph = app.getGraph();
    auto& nodes = graph.getNodes();
    auto& edges = graph.getEdges();
    ImGuiIO& io = ImGui::GetIO();

    std::vector<bool> visibleMask = computeVisibleMask(nodes, edges, app.selectedNoteId, g_graphDepthFilter);
    std::string searchLower = toLowerStr(g_graphSearchBuf);
    bool hasSearch = !searchLower.empty();

    if (m_autoCenter && !nodes.empty()) {
        float avgX = 0.0f, avgY = 0.0f;
        int visCount = 0;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (!visibleMask[i]) continue;
            avgX += nodes[i].x;
            avgY += nodes[i].y;
            ++visCount;
        }
        if (visCount > 0) {
            avgX /= static_cast<float>(visCount);
            avgY /= static_cast<float>(visCount);
            float targetPanX = -avgX * m_zoom;
            float targetPanY = -avgY * m_zoom;
            m_panX += (targetPanX - m_panX) * 0.08f;
            m_panY += (targetPanY - m_panY) * 0.08f;
        }
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
    dl->AddRectFilled(canvasOrig, canvasEnd, toCol32(pal.bgBase, 1.0f));

    {
        float minorStep = 30.0f * m_zoom;
        if (minorStep >= 4.0f) {
            float ox = fmodf(m_panX + canvasSize.x * 0.5f, minorStep);
            float oy = fmodf(m_panY + canvasSize.y * 0.5f, minorStep);
            ImU32 minorCol = toCol32(pal.border, 0.35f);
            for (float x = ox; x < canvasSize.x; x += minorStep)
                dl->AddLine({ canvasOrig.x + x, canvasOrig.y }, { canvasOrig.x + x, canvasEnd.y }, minorCol);
            for (float y = oy; y < canvasSize.y; y += minorStep)
                dl->AddLine({ canvasOrig.x, canvasOrig.y + y }, { canvasEnd.x, canvasOrig.y + y }, minorCol);
        }
        float majorStep = 150.0f * m_zoom;
        if (majorStep >= 8.0f) {
            float ox = fmodf(m_panX + canvasSize.x * 0.5f, majorStep);
            float oy = fmodf(m_panY + canvasSize.y * 0.5f, majorStep);
            ImU32 majorCol = toCol32(pal.borderLine, 0.55f);
            for (float x = ox; x < canvasSize.x; x += majorStep)
                dl->AddLine({ canvasOrig.x + x, canvasOrig.y }, { canvasOrig.x + x, canvasEnd.y }, majorCol, 1.2f);
            for (float y = oy; y < canvasSize.y; y += majorStep)
                dl->AddLine({ canvasOrig.x, canvasOrig.y + y }, { canvasEnd.x, canvasOrig.y + y }, majorCol, 1.2f);
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
    int nodeCount = static_cast<int>(nodes.size());
    if (m_zoom >= 0.5f || nodeCount <= 200) lodLevel = 0;
    else if (m_zoom >= 0.25f || nodeCount <= 1000) lodLevel = 1;
    else lodLevel = 2;
    int circleSegments = (lodLevel == 0) ? 32 : (lodLevel == 1) ? 16 : 8;

    for (const auto& edge : edges) {
        if (edge.from < 0 || edge.from >= nodeCount) continue;
        if (edge.to < 0 || edge.to >= nodeCount) continue;
        if (!visibleMask[edge.from] || !visibleMask[edge.to]) continue;

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
            ImVec4 colA = nodeColor(a, pal);
            ImVec4 colB = nodeColor(b, pal);
            ImVec4 blend(
                (colA.x + colB.x) * 0.5f,
                (colA.y + colB.y) * 0.5f,
                (colA.z + colB.z) * 0.5f,
                hasSearch ? 0.35f : 0.78f
            );
            edgeCol = toCol32(blend);
        } else {
            edgeCol = toCol32(pal.textMuted, hasSearch ? 0.14f : 0.34f);
        }
        float thickness = (dimA || dimB)
            ? std::max(0.8f, 1.8f * m_zoom) : std::max(0.5f, 1.0f * m_zoom);
        dl->AddLine(p1, p2, edgeCol, thickness);
    }

    int hoveredNode = -1;
    int nodesDrawn = 0;

    for (int i = 0; i < nodeCount; ++i) {
        if (!visibleMask[i]) continue;
        const auto& node = nodes[i];
        ImVec2 sp = graphToScreen(node.x, node.y);
        float sr = node.radius * m_zoom;
        if (!isOnScreen(sp, sr)) continue;
        ++nodesDrawn;

        float dx = mousePos.x - sp.x;
        float dy = mousePos.y - sp.y;
        bool nodeHov = (dx * dx + dy * dy) <= ((sr + 4.0f) * (sr + 4.0f)) && hovered;
        if (nodeHov) hoveredNode = i;

        Note* notePtr = app.getNoteManager().getNoteById(node.noteId);
        bool isSelected = (node.noteId == app.selectedNoteId);
        bool isSearchHit = hasSearch && nodeMatchesSearch(node, notePtr, searchLower);
        float nodeAlpha = (!hasSearch || isSearchHit || isSelected) ? 0.95f : 0.22f;

        ImVec4 col4 = nodeColor(node, pal);
        ImU32 col = toCol32(col4, nodeAlpha);

        if (lodLevel == 0) {
            float extraGlow = std::min(static_cast<float>(node.connectionCount) * 2.5f, 18.0f) * m_zoom;
            if (isSelected) {
                dl->AddCircleFilled(sp, sr + extraGlow + 14.0f * m_zoom, toCol32(pal.accent, 0.08f), circleSegments);
                dl->AddCircleFilled(sp, sr + extraGlow + 8.0f * m_zoom, toCol32(pal.accent, 0.16f), circleSegments);
                dl->AddCircleFilled(sp, sr + extraGlow + 4.0f * m_zoom, toCol32(pal.accent, 0.24f), circleSegments);
            }
            if (isSearchHit) {
                dl->AddCircleFilled(sp, sr + 12.0f * m_zoom, toCol32(pal.warning, 0.20f), circleSegments);
                dl->AddCircle(sp, sr + 6.0f * m_zoom, toCol32(pal.warning, 0.95f), circleSegments, 2.2f);
            }
            if (extraGlow > 1.0f && nodeAlpha > 0.5f) {
                dl->AddCircleFilled(sp, sr + extraGlow, toCol32(col4, 0.08f), circleSegments);
                dl->AddCircleFilled(sp, sr + extraGlow * 0.55f, toCol32(col4, 0.16f), circleSegments);
            }
        } else if (isSearchHit) {
            dl->AddCircle(sp, sr + 4.0f, toCol32(pal.warning, 0.95f), circleSegments, 2.0f);
        }

        if (nodeHov) {
            dl->AddCircleFilled(sp, sr + 6.0f * m_zoom, toCol32(pal.textPrimary, 0.14f), circleSegments);
        }
        dl->AddCircleFilled(sp, sr, col, circleSegments);

        if (lodLevel <= 1) {
            ImU32 borderCol = isSelected
                ? toCol32(pal.accent, 0.95f)
                : (isSearchHit ? toCol32(pal.warning, 0.95f) : toCol32(col4, 0.55f * nodeAlpha));
            float borderW = (isSelected || isSearchHit) ? 2.0f : 1.2f;
            dl->AddCircle(sp, sr, borderCol, circleSegments, borderW);
        }

        if (lodLevel <= 1 || isSearchHit || isSelected) {
            float labelAlpha = (isSearchHit || isSelected)
                ? 1.0f
                : (m_zoom >= 0.6f ? 1.0f : m_zoom >= 0.4f ? (m_zoom - 0.4f) / 0.2f : 0.0f);
            labelAlpha *= nodeAlpha;
            if (labelAlpha > 0.02f) {
                const char* label = node.noteId.c_str();
                ImVec2 textSz = ImGui::CalcTextSize(label);
                ImVec2 textPos { sp.x - textSz.x * 0.5f, sp.y + sr + 10.0f };
                dl->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f),
                            toCol32(pal.bgBase, 0.65f * labelAlpha), label);
                ImU32 textCol = isSearchHit
                    ? toCol32(pal.warning, labelAlpha)
                    : (isSelected ? toCol32(pal.textPrimary, labelAlpha) : toCol32(pal.textSecondary, labelAlpha));
                dl->AddText(textPos, textCol, label);
            }
        }
    }

    m_lodLevel = lodLevel;
    m_nodesVisible = nodesDrawn;

    if (m_draggedNode >= nodeCount) m_draggedNode = -1;

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_draggedNode = hoveredNode;
        if (hoveredNode >= 0 && hoveredNode < nodeCount) {
            app.selectedNoteId = nodes[hoveredNode].noteId;
            m_isPanning = false;
        } else {
            m_isPanning = true;
        }
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        if (m_draggedNode >= 0 && m_draggedNode < nodeCount) {
            auto& n = graph.getNodes()[m_draggedNode];
            auto gp = screenToGraph(mousePos.x, mousePos.y);
            n.x = gp.x;
            n.y = gp.y;
            n.pinned = true;
        } else if (m_isPanning && hovered) {
            m_panX += io.MouseDelta.x;
            m_panY += io.MouseDelta.y;
            m_autoCenter = false;
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (m_draggedNode >= 0 && m_draggedNode < static_cast<int>(graph.getNodes().size()))
            graph.getNodes()[m_draggedNode].pinned = false;
        m_draggedNode = -1;
        m_isPanning = false;
    }

    {
        const float ox = canvasEnd.x - 155.0f;
        float oy = canvasOrig.y + 8.0f;
        dl->AddRectFilled({ ox - 8.0f, oy - 6.0f }, { canvasEnd.x - 6.0f, oy + 132.0f },
                          toCol32(pal.bgPopover, 0.88f), 8.0f);
        dl->AddRect({ ox - 8.0f, oy - 6.0f }, { canvasEnd.x - 6.0f, oy + 132.0f },
                    toCol32(pal.borderLine, 0.70f), 8.0f);

        struct LegendEntry { const char* label; ImU32 col; };
        LegendEntry legend[] = {
            { "proyecto",    toCol32(typeColor("proyecto", pal))   },
            { "concepto",    toCol32(typeColor("concepto", pal))   },
            { "referencia",  toCol32(typeColor("referencia", pal)) },
            { "diario",      toCol32(typeColor("diario", pal))     },
            { "tarea",       toCol32(typeColor("tarea", pal))      },
            { "0-1 enlaces", toCol32(connectivityColor(0, pal))    },
            { "6+ enlaces",  toCol32(connectivityColor(6, pal))    },
        };
        for (auto& e : legend) {
            dl->AddCircleFilled({ ox + 6.0f, oy + 7.0f }, 4.5f, e.col, 20);
            dl->AddText({ ox + 16.0f, oy }, toCol32(pal.textSecondary, 0.92f), e.label);
            oy += 18.0f;
        }
    }

    dl->PopClipRect();

    if (hoveredNode >= 0 && hoveredNode < nodeCount && m_draggedNode < 0) {
        const auto& node = nodes[hoveredNode];
        auto* note = app.getNoteManager().getNoteById(node.noteId);
        ImGui::BeginTooltip();
        ImGui::TextColored(pal.accent, "%s", node.noteId.c_str());
        if (note) {
            ImGui::TextColored(pal.textMuted, "Conexiones: %d", node.connectionCount);
            if (!node.nodeType.empty()) {
                ImVec4 tc = typeColor(node.nodeType, pal);
                if (tc.x >= 0.0f) ImGui::TextColored(tc, "Tipo: %s", node.nodeType.c_str());
            }
            std::string preview = note->content.substr(0, 180);
            if (note->content.size() > 180) preview += "...";
            if (!preview.empty()) {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
                ImGui::TextWrapped("%s", preview.c_str());
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::TextColored(pal.warning, "(Nota no creada aun)");
        }
        ImGui::EndTooltip();
    }
}

void GraphPanel::render3D(App& app) {
    const ThemePalette& pal = UITheme::palette();
    auto& graph = app.getGraph();
    auto& nodes = graph.getNodes();
    auto& edges = graph.getEdges();
    ImGuiIO& io = ImGui::GetIO();

    std::vector<bool> visibleMask = computeVisibleMask(nodes, edges, app.selectedNoteId, g_graphDepthFilter);
    std::string searchLower = toLowerStr(g_graphSearchBuf);
    bool hasSearch = !searchLower.empty();

    ImVec2 canvasOrig = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.x = std::max(canvasSize.x, 50.0f);
    canvasSize.y = std::max(canvasSize.y, 50.0f);
    ImVec2 canvasEnd { canvasOrig.x + canvasSize.x, canvasOrig.y + canvasSize.y };

    ImGui::InvisibleButton("##canvas3d", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mousePos = io.MousePos;

    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f) && m_draggedNode < 0) {
        m_camYaw   += io.MouseDelta.x * 0.005f;
        m_camPitch += io.MouseDelta.y * 0.005f;
        if (m_camPitch > 1.5f) m_camPitch = 1.5f;
        if (m_camPitch < -1.5f) m_camPitch = -1.5f;
    }

    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
        m_camDist += io.MouseDelta.y * 2.0f;
        if (m_camDist < 100.0f) m_camDist = 100.0f;
        if (m_camDist > 3000.0f) m_camDist = 3000.0f;
    }

    if (hovered && io.MouseWheel != 0.0f) {
        m_camDist -= io.MouseWheel * 40.0f;
        if (m_camDist < 100.0f) m_camDist = 100.0f;
        if (m_camDist > 3000.0f) m_camDist = 3000.0f;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvasOrig, canvasEnd, true);
    dl->AddRectFilled(canvasOrig, canvasEnd, toCol32(pal.bgBase, 1.0f));

    struct NodeProj {
        int idx;
        float sx, sy, depth, scale;
    };
    std::vector<NodeProj> projNodes;
    projNodes.reserve(nodes.size());

    int nodeCount = static_cast<int>(nodes.size());
    for (int i = 0; i < nodeCount; ++i) {
        const auto& node = nodes[i];
        auto p = project3D(node.x, node.y, node.z,
                           canvasOrig.x, canvasOrig.y, canvasSize.x, canvasSize.y);
        projNodes.push_back({ i, p.sx, p.sy, p.depth, p.scale });
    }

    std::vector<int> drawOrder(projNodes.size());
    std::iota(drawOrder.begin(), drawOrder.end(), 0);
    std::sort(drawOrder.begin(), drawOrder.end(), [&](int a, int b) {
        return projNodes[a].depth > projNodes[b].depth;
    });

    for (const auto& edge : edges) {
        if (edge.from < 0 || edge.from >= nodeCount || edge.from >= static_cast<int>(projNodes.size())) continue;
        if (edge.to < 0 || edge.to >= nodeCount || edge.to >= static_cast<int>(projNodes.size())) continue;
        if (!visibleMask[edge.from] || !visibleMask[edge.to]) continue;

        const auto& pa = projNodes[edge.from];
        const auto& pb = projNodes[edge.to];

        float avgDepth = (pa.depth + pb.depth) * 0.5f;
        float depthAlpha = std::max(0.08f, std::min(1.0f, 1.0f - (avgDepth - 200.0f) / 1500.0f));

        bool sel = (nodes[edge.from].noteId == app.selectedNoteId) ||
                   (nodes[edge.to].noteId == app.selectedNoteId);

        ImU32 edgeCol;
        if (sel) {
            ImVec4 colA = nodeColor(nodes[edge.from], pal);
            ImVec4 colB = nodeColor(nodes[edge.to], pal);
            ImVec4 blend(
                (colA.x + colB.x) * 0.5f,
                (colA.y + colB.y) * 0.5f,
                (colA.z + colB.z) * 0.5f,
                depthAlpha * (hasSearch ? 0.35f : 0.72f)
            );
            edgeCol = toCol32(blend);
        } else {
            edgeCol = toCol32(pal.textMuted, depthAlpha * (hasSearch ? 0.12f : 0.30f));
        }

        float thickness = std::max(0.5f, 1.2f * (pa.scale + pb.scale) * 0.5f * 15.0f);
        dl->AddLine({ pa.sx, pa.sy }, { pb.sx, pb.sy }, edgeCol, thickness);
    }

    int hoveredNode = -1;

    for (int di : drawOrder) {
        const auto& pn = projNodes[di];
        if (!visibleMask[pn.idx]) continue;
        const auto& node = nodes[pn.idx];

        float sr = node.radius * pn.scale * 15.0f;
        if (sr < 1.0f) sr = 1.0f;
        if (sr > 60.0f) sr = 60.0f;

        float depthAlpha = std::max(0.15f, std::min(1.0f, 1.0f - (pn.depth - 200.0f) / 1500.0f));

        if (pn.sx + sr < canvasOrig.x || pn.sx - sr > canvasEnd.x ||
            pn.sy + sr < canvasOrig.y || pn.sy - sr > canvasEnd.y)
            continue;

        float dx = mousePos.x - pn.sx;
        float dy = mousePos.y - pn.sy;
        if (hovered && (dx * dx + dy * dy) <= ((sr + 4.0f) * (sr + 4.0f))) {
            hoveredNode = pn.idx;
        }

        Note* notePtr = app.getNoteManager().getNoteById(node.noteId);
        bool isSelected = (node.noteId == app.selectedNoteId);
        bool isSearchHit = hasSearch && nodeMatchesSearch(node, notePtr, searchLower);
        float searchMul = (!hasSearch || isSearchHit || isSelected) ? 1.0f : 0.22f;

        ImVec4 col4 = nodeColor(node, pal);
        int segments = (sr > 10.0f) ? 24 : (sr > 4.0f) ? 16 : 8;

        if (isSelected) {
            dl->AddCircleFilled({ pn.sx, pn.sy }, sr + 10.0f * depthAlpha,
                                toCol32(pal.accent, 0.14f * depthAlpha), segments);
            dl->AddCircleFilled({ pn.sx, pn.sy }, sr + 5.0f * depthAlpha,
                                toCol32(pal.accent, 0.26f * depthAlpha), segments);
        }
        if (isSearchHit) {
            dl->AddCircleFilled({ pn.sx, pn.sy }, sr + 10.0f * depthAlpha,
                                toCol32(pal.warning, 0.22f * depthAlpha), segments);
            dl->AddCircle({ pn.sx, pn.sy }, sr + 5.0f * depthAlpha,
                          toCol32(pal.warning, 0.95f * depthAlpha), segments, 2.0f);
        }

        dl->AddCircleFilled({ pn.sx, pn.sy }, sr,
                            toCol32(col4, depthAlpha * col4.w * searchMul), segments);

        ImU32 borderCol = isSelected
            ? toCol32(pal.accent, 0.85f * depthAlpha)
            : (isSearchHit ? toCol32(pal.warning, 0.95f * depthAlpha) : toCol32(col4, 0.45f * depthAlpha * searchMul));
        dl->AddCircle({ pn.sx, pn.sy }, sr, borderCol, segments, (isSelected || isSearchHit) ? 1.8f : 1.2f);

        if ((sr > 6.0f && depthAlpha > 0.3f) || isSearchHit || isSelected) {
            const char* label = node.noteId.c_str();
            ImVec2 textSz = ImGui::CalcTextSize(label);
            ImVec2 textPos { pn.sx - textSz.x * 0.5f, pn.sy + sr + 4.0f };

            float textAlpha = depthAlpha * searchMul;
            dl->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f),
                        toCol32(pal.bgBase, 0.55f * textAlpha), label);
            ImU32 textCol = isSearchHit
                ? toCol32(pal.warning, textAlpha)
                : (isSelected ? toCol32(pal.textPrimary, textAlpha) : toCol32(pal.textSecondary, textAlpha));
            dl->AddText(textPos, textCol, label);
        }
    }

    {
        float axLen = 30.0f;
        float bx = canvasOrig.x + 40.0f;
        float by = canvasEnd.y - 40.0f;
        float cosY = std::cos(m_camYaw), sinY = std::sin(m_camYaw);
        float cosP = std::cos(m_camPitch), sinP = std::sin(m_camPitch);

        float x1 = axLen * cosY;
        dl->AddLine({ bx, by }, { bx + x1, by }, toCol32(pal.danger, 0.85f), 2.0f);
        dl->AddText({ bx + x1 + 2.0f, by - 6.0f }, toCol32(pal.danger, 0.85f), "X");

        float y2 = -axLen * cosP;
        dl->AddLine({ bx, by }, { bx, by + y2 }, toCol32(pal.success, 0.85f), 2.0f);
        dl->AddText({ bx + 4.0f, by + y2 - 6.0f }, toCol32(pal.success, 0.85f), "Y");

        float x3 = axLen * sinY, y3 = -axLen * sinP;
        dl->AddLine({ bx, by }, { bx + x3, by + y3 }, toCol32(pal.accent, 0.85f), 2.0f);
        dl->AddText({ bx + x3 + 2.0f, by + y3 - 6.0f }, toCol32(pal.accent, 0.85f), "Z");
    }

    {
        const float ox = canvasEnd.x - 165.0f;
        float oy = canvasOrig.y + 8.0f;
        dl->AddRectFilled({ ox - 8.0f, oy - 6.0f }, { canvasEnd.x - 6.0f, oy + 56.0f },
                          toCol32(pal.bgPopover, 0.88f), 8.0f);
        dl->AddRect({ ox - 8.0f, oy - 6.0f }, { canvasEnd.x - 6.0f, oy + 56.0f },
                    toCol32(pal.borderLine, 0.70f), 8.0f);
        dl->AddText({ ox, oy }, toCol32(pal.textSecondary, 0.90f), "Arrastrar: rotar");
        oy += 16.0f;
        dl->AddText({ ox, oy }, toCol32(pal.textSecondary, 0.90f), "Scroll: zoom");
        oy += 16.0f;
        dl->AddText({ ox, oy }, toCol32(pal.textSecondary, 0.90f), "Click: seleccionar");
    }

    dl->PopClipRect();

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredNode >= 0 && hoveredNode < nodeCount) {
        app.selectedNoteId = nodes[hoveredNode].noteId;
        m_draggedNode = -1;
    }

    if (hoveredNode >= 0 && hoveredNode < nodeCount) {
        const auto& node = nodes[hoveredNode];
        auto* note = app.getNoteManager().getNoteById(node.noteId);
        ImGui::BeginTooltip();
        ImGui::TextColored(pal.accent, "%s", node.noteId.c_str());
        if (note) {
            ImGui::TextColored(pal.textMuted,
                               "Conexiones: %d | Pos: (%.0f, %.0f, %.0f)",
                               node.connectionCount, node.x, node.y, node.z);
            std::string preview = note->content.substr(0, 120);
            if (note->content.size() > 120) preview += "...";
            if (!preview.empty()) {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
                ImGui::TextWrapped("%s", preview.c_str());
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTooltip();
    }
}

}
