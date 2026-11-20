#include "pdf_panel.h"
#include "pdf_engine.h"
#include "app.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace nodepanda {

namespace {

struct HighlightPaletteEntry {
    const char* name;
    ImVec4 color;
};

const HighlightPaletteEntry kHighlightColors[5] = {
    {"Amarillo", ImVec4(0.98f, 0.84f, 0.22f, 1.0f)},
    {"Verde",    ImVec4(0.34f, 0.82f, 0.48f, 1.0f)},
    {"Azul",     ImVec4(0.32f, 0.66f, 0.96f, 1.0f)},
    {"Rosa",     ImVec4(0.95f, 0.44f, 0.68f, 1.0f)},
    {"Naranja",  ImVec4(0.97f, 0.58f, 0.24f, 1.0f)}
};

ImVec4 highlightColorFor(int colorIndex) {
    int idx = std::clamp(colorIndex, 0, 4);
    return kHighlightColors[idx].color;
}

const char* highlightNameFor(int colorIndex) {
    int idx = std::clamp(colorIndex, 0, 4);
    return kHighlightColors[idx].name;
}

struct PageLineLayout {
    size_t lineIndex = 0;
    float y0Norm = 0.0f;
    float y1Norm = 0.0f;
    float fontScale = 1.0f;
};

std::vector<PageLineLayout> computePageLineLayouts(const PdfPage& page, int pageIdx) {
    std::vector<PageLineLayout> spans;
    const size_t count = page.textLines.size();
    if (count == 0) return spans;

    const float marginTop = 0.068f;
    const float marginBottom = 0.055f;
    const float usableH = 1.0f - marginTop - marginBottom;
    const float step = std::min(0.029f, usableH / static_cast<float>(std::max<size_t>(26, count + 2)));

    float cursorY = marginTop;
    for (size_t i = 0; i < count; ++i) {
        bool isTitleLine = (pageIdx == 0 && i == 0);
        float lineH = isTitleLine ? (step * 1.48f) : step;
        if (cursorY + lineH > 1.0f - marginBottom) break;
        PageLineLayout span;
        span.lineIndex = i;
        span.y0Norm = cursorY;
        span.y1Norm = cursorY + lineH;
        span.fontScale = isTitleLine ? 1.24f : 0.92f;
        spans.push_back(span);
        cursorY += lineH + (isTitleLine ? (step * 0.35f) : (step * 0.14f));
    }
    return spans;
}

std::string extractQuotedTextForRect(const PdfPage& page, const std::vector<PageLineLayout>& spans,
                                     float ny0, float ny1) {
    std::string combined;
    for (const auto& s : spans) {
        if (s.y1Norm >= ny0 && s.y0Norm <= ny1) {
            const std::string& line = page.textLines[s.lineIndex];
            if (line.empty()) continue;
            if (!combined.empty()) combined.push_back(' ');
            combined += line;
            if (combined.size() >= 260) {
                combined = combined.substr(0, 260);
                break;
            }
        }
    }
    if (combined.empty()) {
        combined = "Fragmento en pagina " + std::to_string(page.pageNumber);
    }
    return combined;
}

std::vector<std::string> buildDemoParagraphs() {
    return {
        "Arquitectura de conocimiento en grafos locales y anotacion visual de documentos PDF.",
        "Node Panda organiza ideas en notas Markdown interconectadas mediante enlaces wiki, consultas estructuradas y referencias directas hacia paginas especificas de documentos tecnicos.",
        "Cada documento abierto en el visor calcula su huella criptografica SHA-256 para vincular sus resaltados de manera persistente en disco sin alterar el archivo PDF original.",
        "Los resaltados se almacenan en coordenadas normalizadas entre 0 y 1 respecto al ancho y alto de cada pagina, permitiendo cambiar el nivel de zoom manteniendo alineacion exacta.",
        "Al arrastrar el cursor sobre cualquier parrafo de la hoja, el motor captura el texto subyacente, crea una anotacion cromatica y habilita comentarios editables en el panel lateral.",
        "Con el boton Insertar Cita en Nota se anexa una referencia estructurada con sintaxis de cita hacia la nota activa, manteniendo trazabilidad bidireccional entre lecturas y sintesis.",
        "Flujo recomendado de lectura activa: abrir un articulo, marcar definiciones clave en amarillo, evidencia empirica en verde, dudas en azul, criticas en rosa y acciones en naranja.",
        "Segunda seccion: integracion con repeticion espaciada, grafos de citas y analisis de backlinks.",
        "Cuando varias notas citan fragmentos del mismo documento, la estructura de backlinks revela nucleos tematicos y facilita revisar las fuentes primarias con un solo clic.",
        "El formato de almacenamiento tabular TSV permite inspeccionar, respaldar y sincronizar todas las anotaciones de forma transparente en el directorio de activos del espacio de trabajo.",
        "Este documento de demostracion incluye varias paginas y una tabla xref estandar PDF 1.4 para validar la navegacion entre paginas, el zoom dinamico y la exportacion de citas."
    };
}

}

void PdfPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();
    PdfEngine& engine = app.getPdfEngine();

    ImGui::SetNextWindowSize(ImVec2(1060, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Anotador PDF")) {
        ImGui::End();
        return;
    }

    float openBarW = std::max(180.0f, ImGui::GetContentRegionAvail().x - 360.0f);
    ImGui::SetNextItemWidth(openBarW);
    bool enterOpen = ImGui::InputTextWithHint("##pdf_path_input", "Ruta del archivo PDF...", m_pathBuf, sizeof(m_pathBuf),
        ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::SameLine(0.0f, 6.0f);
    if (ui::primaryButton("Abrir PDF") || enterOpen) {
        if (m_pathBuf[0] != '\0') {
            std::string err;
            if (engine.openPdf(std::filesystem::path(m_pathBuf), err)) {
                m_currentPage = 0;
                m_selectedHighlightId.clear();
                m_editingHighlightId.clear();
                ui::pushToast("PDF cargado correctamente", "success");
            } else {
                ui::pushToast(err.empty() ? "Error al abrir PDF" : err, "error");
            }
        }
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (ui::subtleButton("Crear Demo PDF")) {
        std::filesystem::path baseDir = engine.dataDir().empty()
            ? std::filesystem::path("data")
            : engine.dataDir();
        std::filesystem::path demoPath = baseDir / "assets" / "demo_nodepanda.pdf";
        if (engine.createSamplePdf(demoPath, "Guia de Anotacion Visual PDF", buildDemoParagraphs())) {
            std::string err;
            if (engine.openPdf(demoPath, err)) {
                std::string pstr = demoPath.string();
                std::strncpy(m_pathBuf, pstr.c_str(), sizeof(m_pathBuf) - 1);
                m_pathBuf[sizeof(m_pathBuf) - 1] = '\0';
                m_currentPage = 0;
                m_selectedHighlightId.clear();
                m_editingHighlightId.clear();
                ui::pushToast("Demo PDF creado y abierto", "success");
            } else {
                ui::pushToast(err, "error");
            }
        } else {
            ui::pushToast("No se pudo crear el Demo PDF", "error");
        }
    }

    PdfDocument* doc = engine.activeDocument();
    const auto& docs = engine.documents();

    if (!docs.empty() && doc) {
        ImGui::Spacing();
        if (docs.size() > 1) {
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::BeginCombo("##pdf_doc_select", doc->title.c_str())) {
                for (const auto& candidate : docs) {
                    bool sel = (candidate.id == doc->id);
                    if (ImGui::Selectable(candidate.title.c_str(), sel)) {
                        engine.setActiveDocument(candidate.id);
                        doc = engine.activeDocument();
                        m_currentPage = 0;
                        m_selectedHighlightId.clear();
                        m_editingHighlightId.clear();
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine(0.0f, 8.0f);
        }

        int totalPages = static_cast<int>(doc->pages.size());
        if (totalPages > 0) {
            m_currentPage = std::clamp(m_currentPage, 0, totalPages - 1);
        } else {
            m_currentPage = 0;
        }

        if (ui::subtleButton("Anterior")) {
            if (m_currentPage > 0) --m_currentPage;
        }
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextColored(pal.textPrimary, "Pag %d / %d", totalPages > 0 ? (m_currentPage + 1) : 0, totalPages);
        ImGui::SameLine(0.0f, 4.0f);
        if (ui::subtleButton("Siguiente")) {
            if (m_currentPage + 1 < totalPages) ++m_currentPage;
        }

        ImGui::SameLine(0.0f, 12.0f);
        ImGui::SetNextItemWidth(130.0f);
        ImGui::SliderFloat("##pdf_zoom", &m_zoom, 0.5f, 2.0f, "Zoom %.2fx");

        ImGui::SameLine(0.0f, 12.0f);
        for (int ci = 0; ci < 5; ++ci) {
            if (ci > 0) ImGui::SameLine(0.0f, 4.0f);
            ImVec4 col = kHighlightColors[ci].color;
            bool active = (m_activeColorIndex == ci);
            ImVec4 bgCol = active ? col : ImVec4(col.x, col.y, col.z, 0.24f);
            ImVec4 hovCol = ImVec4(col.x, col.y, col.z, active ? 0.95f : 0.48f);
            ImVec4 txtCol = active ? ImVec4(0.08f, 0.09f, 0.11f, 1.0f) : pal.textPrimary;

            ImGui::PushID(ci);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, bgCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
            ImGui::PushStyleColor(ImGuiCol_Text, txtCol);
            if (ImGui::Button(kHighlightColors[ci].name)) {
                m_activeColorIndex = ci;
            }
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
    }

    ImGui::Separator();

    if (!doc || !doc->loaded || doc->pages.empty()) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(24.0f, avail.y * 0.28f));
        ImGui::TextColored(pal.textSecondary, "Abre un archivo PDF o pulsa 'Crear Demo PDF' para comenzar a anotar.");
        ImGui::End();
        return;
    }

    const float sidebarW = 315.0f;
    ImVec2 totalAvail = ImGui::GetContentRegionAvail();
    float canvasPanelW = std::max(280.0f, totalAvail.x - sidebarW - 10.0f);

    const PdfPage& page = doc->pages[static_cast<size_t>(m_currentPage)];
    std::vector<PageLineLayout> lineSpans = computePageLineLayouts(page, m_currentPage);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bgBase);
    ImGui::BeginChild("##pdf_canvas_viewport", ImVec2(canvasPanelW, totalAvail.y), true,
        ImGuiWindowFlags_HorizontalScrollbar);

    float pageW = std::max(240.0f, page.widthPt * 0.85f * m_zoom);
    float pageH = std::max(320.0f, page.heightPt * 0.85f * m_zoom);
    float pad = 26.0f;

    ImVec2 innerAvail = ImGui::GetContentRegionAvail();
    float areaW = std::max(innerAvail.x, pageW + pad * 2.0f);
    float areaH = std::max(innerAvail.y, pageH + pad * 2.0f);

    ImVec2 areaOrigin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##pdf_page_canvas", ImVec2(areaW, areaH), ImGuiButtonFlags_MouseButtonLeft);
    bool canvasHovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();

    if (canvasHovered && io.KeyCtrl && io.MouseWheel != 0.0f) {
        m_zoom = std::clamp(m_zoom + io.MouseWheel * 0.1f, 0.5f, 2.0f);
    }

    ImVec2 pageMin(areaOrigin.x + std::max(pad, (areaW - pageW) * 0.5f), areaOrigin.y + pad);
    ImVec2 pageMax(pageMin.x + pageW, pageMin.y + pageH);

    ImVec2 mouse = io.MousePos;
    bool mouseInPage = (mouse.x >= pageMin.x && mouse.x <= pageMax.x &&
                        mouse.y >= pageMin.y && mouse.y <= pageMax.y);

    auto toNorm = [&](const ImVec2& p) -> ImVec2 {
        return ImVec2(
            std::clamp((p.x - pageMin.x) / pageW, 0.0f, 1.0f),
            std::clamp((p.y - pageMin.y) / pageH, 0.0f, 1.0f));
    };

    if (canvasHovered && mouseInPage && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 normClick = toNorm(mouse);
        std::string clickedHl;
        for (int i = static_cast<int>(doc->highlights.size()) - 1; i >= 0; --i) {
            const PdfHighlight& h = doc->highlights[static_cast<size_t>(i)];
            if (h.pageIndex != m_currentPage) continue;
            if (normClick.x >= h.x && normClick.x <= h.x + h.w &&
                normClick.y >= h.y && normClick.y <= h.y + h.h) {
                clickedHl = h.id;
                break;
            }
        }
        m_selectedHighlightId = clickedHl;
        m_isDragging = true;
        m_dragStartNormX = normClick.x;
        m_dragStartNormY = normClick.y;
        m_dragCurrNormX = normClick.x;
        m_dragCurrNormY = normClick.y;
    }

    if (m_isDragging) {
        ImVec2 normCurr = toNorm(mouse);
        m_dragCurrNormX = normCurr.x;
        m_dragCurrNormY = normCurr.y;

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_isDragging = false;
            float nx0 = std::min(m_dragStartNormX, m_dragCurrNormX);
            float ny0 = std::min(m_dragStartNormY, m_dragCurrNormY);
            float nx1 = std::max(m_dragStartNormX, m_dragCurrNormX);
            float ny1 = std::max(m_dragStartNormY, m_dragCurrNormY);
            float dragPxW = (nx1 - nx0) * pageW;
            float dragPxH = (ny1 - ny0) * pageH;

            if (dragPxW >= 8.0f && dragPxH >= 6.0f) {
                std::string quoted = extractQuotedTextForRect(page, lineSpans, ny0, ny1);
                std::string newId = engine.addHighlight(
                    m_currentPage, nx0, ny0, nx1 - nx0, ny1 - ny0,
                    m_activeColorIndex, quoted, "", app.selectedNoteId);
                if (!newId.empty()) {
                    m_selectedHighlightId = newId;
                    m_editingHighlightId.clear();
                    ui::pushToast("Resaltado agregado", "success");
                }
            }
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(
        ImVec2(pageMin.x + 6.0f, pageMin.y + 8.0f),
        ImVec2(pageMax.x + 6.0f, pageMax.y + 8.0f),
        IM_COL32(0, 0, 0, 68), 4.0f);
    dl->AddRectFilled(pageMin, pageMax, IM_COL32(252, 251, 248, 255), 3.0f);
    dl->AddRect(pageMin, pageMax, IM_COL32(190, 192, 198, 255), 3.0f, 0, 1.2f);

    dl->PushClipRect(pageMin, pageMax, true);

    for (const auto& hl : doc->highlights) {
        if (hl.pageIndex != m_currentPage) continue;
        ImVec2 hMin(pageMin.x + hl.x * pageW, pageMin.y + hl.y * pageH);
        ImVec2 hMax(pageMin.x + (hl.x + hl.w) * pageW, pageMin.y + (hl.y + hl.h) * pageH);
        ImVec4 c = highlightColorFor(hl.colorIndex);
        bool selected = (hl.id == m_selectedHighlightId);
        ImVec4 fillCol(c.x, c.y, c.z, selected ? 0.42f : 0.30f);
        dl->AddRectFilled(hMin, hMax, ImGui::ColorConvertFloat4ToU32(fillCol), 2.5f);
    }

    ImFont* font = UITheme::fontBody() ? UITheme::fontBody() : ImGui::GetFont();
    float baseFontPx = std::clamp(12.2f * m_zoom, 8.0f, 28.0f);
    float leftTextX = pageMin.x + pageW * 0.085f;
    float wrapWidth = pageW * 0.83f;
    ImU32 inkTitle = IM_COL32(18, 22, 28, 255);
    ImU32 inkBody = IM_COL32(34, 38, 46, 255);

    for (const auto& span : lineSpans) {
        const std::string& line = page.textLines[span.lineIndex];
        if (line.empty()) continue;
        float yPx = pageMin.y + span.y0Norm * pageH;
        float sz = baseFontPx * span.fontScale;
        ImU32 col = (m_currentPage == 0 && span.lineIndex == 0) ? inkTitle : inkBody;
        dl->AddText(font, sz, ImVec2(leftTextX, yPx), col, line.c_str(), nullptr, wrapWidth);
    }

    char footerBuf[48];
    std::snprintf(footerBuf, sizeof(footerBuf), "%d / %zu", page.pageNumber, doc->pages.size());
    ImVec2 fSz = font->CalcTextSizeA(std::max(9.0f, 10.0f * m_zoom), 120.0f, 0.0f, footerBuf);
    dl->AddText(font, std::max(9.0f, 10.0f * m_zoom),
                ImVec2(pageMin.x + (pageW - fSz.x) * 0.5f, pageMax.y - pageH * 0.042f),
                IM_COL32(120, 126, 136, 255), footerBuf);

    for (const auto& hl : doc->highlights) {
        if (hl.pageIndex != m_currentPage) continue;
        ImVec2 hMin(pageMin.x + hl.x * pageW, pageMin.y + hl.y * pageH);
        ImVec2 hMax(pageMin.x + (hl.x + hl.w) * pageW, pageMin.y + (hl.y + hl.h) * pageH);
        ImVec4 c = highlightColorFor(hl.colorIndex);
        bool selected = (hl.id == m_selectedHighlightId);
        ImVec4 borderCol(c.x, c.y, c.z, selected ? 0.98f : 0.65f);
        dl->AddRect(hMin, hMax, ImGui::ColorConvertFloat4ToU32(borderCol), 2.5f, 0, selected ? 2.2f : 1.1f);
    }

    if (m_isDragging) {
        float nx0 = std::min(m_dragStartNormX, m_dragCurrNormX);
        float ny0 = std::min(m_dragStartNormY, m_dragCurrNormY);
        float nx1 = std::max(m_dragStartNormX, m_dragCurrNormX);
        float ny1 = std::max(m_dragStartNormY, m_dragCurrNormY);
        ImVec2 dMin(pageMin.x + nx0 * pageW, pageMin.y + ny0 * pageH);
        ImVec2 dMax(pageMin.x + nx1 * pageW, pageMin.y + ny1 * pageH);
        ImVec4 c = highlightColorFor(m_activeColorIndex);
        dl->AddRectFilled(dMin, dMax, ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, 0.32f)), 2.0f);
        dl->AddRect(dMin, dMax, ImGui::ColorConvertFloat4ToU32(c), 2.0f, 0, 1.8f);
    }

    dl->PopClipRect();

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 10.0f);

    ImGui::BeginChild("##pdf_annotations_sidebar", ImVec2(sidebarW, totalAvail.y), true);

    ui::sectionHeader("Anotaciones");
    ImGui::TextColored(pal.textMuted, "%s", doc->title.c_str());
    ImGui::TextColored(pal.textMuted, "Total: %zu | Pag: %d", doc->highlights.size(), m_currentPage + 1);
    ImGui::Checkbox("Solo pagina actual", &m_filterCurrentPage);
    ImGui::Separator();

    std::string pendingDeleteId;

    for (size_t i = 0; i < doc->highlights.size(); ++i) {
        PdfHighlight& hl = doc->highlights[i];
        if (m_filterCurrentPage && hl.pageIndex != m_currentPage) continue;

        bool selected = (hl.id == m_selectedHighlightId);
        ImVec4 col = highlightColorFor(hl.colorIndex);

        ImGui::PushID(hl.id.c_str());
        ImGui::PushStyleColor(ImGuiCol_Border, selected ? col : pal.borderLine);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, selected ? pal.bgSurface2 : pal.bgSurface1);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);

        float cardH = selected ? 205.0f : 118.0f;
        if (ImGui::BeginChild("##hl_card", ImVec2(-1.0f, cardH), true)) {
            ImVec2 dotPos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(dotPos.x + 6.0f, dotPos.y + 9.0f), 5.0f,
                ImGui::ColorConvertFloat4ToU32(col));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18.0f);

            char pageBadge[24];
            std::snprintf(pageBadge, sizeof(pageBadge), "P.%d", hl.pageIndex + 1);
            ui::badge(pageBadge, ImVec4(col.x, col.y, col.z, 0.24f), pal.textPrimary);
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextColored(pal.textMuted, "%s", highlightNameFor(hl.colorIndex));

            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 56.0f);
            if (ui::subtleButton("Ir")) {
                m_currentPage = std::clamp(hl.pageIndex, 0, std::max(0, static_cast<int>(doc->pages.size()) - 1));
                m_selectedHighlightId = hl.id;
            }

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_selectedHighlightId = hl.id;
            }

            if (!hl.quotedText.empty()) {
                std::string preview = hl.quotedText.size() > 110
                    ? (hl.quotedText.substr(0, 107) + "...")
                    : hl.quotedText;
                ImGui::TextColored(pal.textSecondary, "\"%s\"", preview.c_str());
            }

            if (selected) {
                if (m_editingHighlightId != hl.id) {
                    m_editingHighlightId = hl.id;
                    std::strncpy(m_commentBuf, hl.comment.c_str(), sizeof(m_commentBuf) - 1);
                    m_commentBuf[sizeof(m_commentBuf) - 1] = '\0';
                }
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputTextMultiline("##hl_comment", m_commentBuf, sizeof(m_commentBuf), ImVec2(-1.0f, 54.0f))) {
                    hl.comment = std::string(m_commentBuf);
                    engine.saveAnnotations(*doc);
                }

                if (ui::primaryButton("Insertar Cita en Nota")) {
                    if (app.selectedNoteId.empty()) {
                        ui::pushToast("Selecciona una nota primero", "warn");
                    } else if (engine.exportCitationToNote(app.getNoteManager(), app.selectedNoteId, hl.id)) {
                        app.graphNeedsRebuild = true;
                        ui::pushToast("Cita insertada en " + app.selectedNoteId, "success");
                    } else {
                        ui::pushToast("No se pudo insertar la cita", "error");
                    }
                }
                ImGui::SameLine(0.0f, 6.0f);
                if (ui::dangerButton("Eliminar")) {
                    pendingDeleteId = hl.id;
                }
            } else {
                if (!hl.comment.empty()) {
                    ImGui::TextWrapped("%s", hl.comment.c_str());
                }
                if (ui::subtleButton("Insertar Cita en Nota")) {
                    if (app.selectedNoteId.empty()) {
                        ui::pushToast("Selecciona una nota primero", "warn");
                    } else if (engine.exportCitationToNote(app.getNoteManager(), app.selectedNoteId, hl.id)) {
                        app.graphNeedsRebuild = true;
                        ui::pushToast("Cita insertada en " + app.selectedNoteId, "success");
                    } else {
                        ui::pushToast("No se pudo insertar la cita", "error");
                    }
                }
                ImGui::SameLine(0.0f, 6.0f);
                if (ui::dangerButton("Eliminar")) {
                    pendingDeleteId = hl.id;
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::PopID();
        ImGui::Spacing();
    }

    if (!pendingDeleteId.empty()) {
        if (m_selectedHighlightId == pendingDeleteId) m_selectedHighlightId.clear();
        if (m_editingHighlightId == pendingDeleteId) m_editingHighlightId.clear();
        engine.removeHighlight(pendingDeleteId);
        ui::pushToast("Resaltado eliminado", "info");
    }

    if (doc->highlights.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("Arrastra sobre la pagina PDF para crear un resaltado.");
    }

    ImGui::EndChild();
    ImGui::End();
}

}
