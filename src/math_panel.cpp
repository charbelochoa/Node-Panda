#include "math_panel.h"
#include "math_renderer.h"
#include "app.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace nodepanda {

namespace {

struct FormulaPreset {
    const char* label;
    const char* latex;
};

const FormulaPreset kPresets[] = {
    {"Cuadratica", "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}"},
    {"Integral Gaussiana", "\\int_{-\\infty}^{\\infty} e^{-x^2} dx = \\sqrt{\\pi}"},
    {"Entropia", "H(X) = -\\sum_{i=1}^{n} p_i \\log_2 p_i"},
    {"PageRank", "PR(u) = \\frac{1 - d}{N} + d \\sum_{v \\in B_u} \\frac{PR(v)}{L(v)}"},
    {"SM-2", "EF' = EF + (0.1 - (5 - q)(0.08 + (5 - q) \\cdot 0.02))"}
};

struct SymbolChip {
    const char* label;
    const char* token;
};

const SymbolChip kQuickSymbols[] = {
    {"frac", "\\frac{a}{b}"},
    {"sqrt", "\\sqrt{x}"},
    {"sum", "\\sum_{i=1}^{n}"},
    {"int", "\\int_{a}^{b}"},
    {"prod", "\\prod_{i=1}^{n}"},
    {"alpha", "\\alpha"},
    {"beta", "\\beta"},
    {"theta", "\\theta"},
    {"lambda", "\\lambda"},
    {"pi", "\\pi"},
    {"sigma", "\\sigma"},
    {"Delta", "\\Delta"},
    {"infty", "\\infty"},
    {"pm", "\\pm"},
    {"le", "\\le"},
    {"ge", "\\ge"},
    {"neq", "\\neq"},
    {"approx", "\\approx"},
    {"to", "\\to"},
    {"Rightarrow", "\\Rightarrow"},
    {"in", "\\in"}
};

}

MathPanel::MathPanel() {
    setFormula(kPresets[0].latex);
}

void MathPanel::setFormula(const std::string& latex) {
    std::strncpy(m_formulaBuf, latex.c_str(), sizeof(m_formulaBuf) - 1);
    m_formulaBuf[sizeof(m_formulaBuf) - 1] = '\0';
}

void MathPanel::appendToken(const std::string& token) {
    std::string cur(m_formulaBuf);
    if (!cur.empty() && cur.back() != ' ' && cur.back() != '{') {
        cur.push_back(' ');
    }
    cur += token;
    setFormula(cur);
}

void MathPanel::insertIntoSelectedNote(App& app) {
    std::string raw(m_formulaBuf);
    size_t first = raw.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        ui::pushToast("La formula esta vacia", "warning");
        return;
    }
    size_t last = raw.find_last_not_of(" \t\r\n");
    std::string trimmed = raw.substr(first, last - first + 1);

    if (app.selectedNoteId.empty()) {
        ui::pushToast("Selecciona una nota antes de insertar", "warning");
        return;
    }

    Note* note = app.getNoteManager().getNoteById(app.selectedNoteId);
    if (!note) {
        ui::pushToast("Nota activa no encontrada", "danger");
        return;
    }

    if (!note->content.empty() && note->content.back() != '\n') {
        note->content.push_back('\n');
    }
    if (m_insertMode == 0) {
        note->content += "\n$$\n" + trimmed + "\n$$\n";
    } else {
        note->content += "$" + trimmed + "$\n";
    }

    note->dirty = true;
    note->saveToFile();
    app.rebuildKnowledgeIndices();
    ui::pushToast("Formula insertada en " + note->id, "success");
}

void MathPanel::render(App& app) {
    const auto& pal = UITheme::palette();
    ImGui::SetNextWindowSize(ImVec2(760, 620), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Matematicas LaTeX")) {
        ImGui::End();
        return;
    }

    ui::sectionHeader("Presets");
    for (int i = 0; i < static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0])); ++i) {
        if (i > 0) ImGui::SameLine();
        if (ui::subtleButton(kPresets[i].label)) {
            setFormula(kPresets[i].latex);
        }
    }

    ImGui::Spacing();
    ui::sectionHeader("Formula LaTeX");

    if (UITheme::fontMono()) ImGui::PushFont(UITheme::fontMono());
    ImGui::InputTextMultiline("##math_latex_input", m_formulaBuf, sizeof(m_formulaBuf),
                              ImVec2(-1.0f, 76.0f), ImGuiInputTextFlags_AllowTabInput);
    if (UITheme::fontMono()) ImGui::PopFont();

    ImGui::Spacing();
    float availW = ImGui::GetContentRegionAvail().x;
    float lineW = 0.0f;
    for (int i = 0; i < static_cast<int>(sizeof(kQuickSymbols) / sizeof(kQuickSymbols[0])); ++i) {
        float btnW = ImGui::CalcTextSize(kQuickSymbols[i].label).x + 20.0f;
        if (i > 0 && lineW + btnW < availW) {
            ImGui::SameLine();
        } else if (i > 0) {
            lineW = 0.0f;
        }
        ImGui::PushID(i);
        if (ui::subtleButton(kQuickSymbols[i].label)) {
            appendToken(kQuickSymbols[i].token);
        }
        ImGui::PopID();
        lineW += btnW + ImGui::GetStyle().ItemSpacing.x;
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Escala", &m_fontSize, 14.0f, 36.0f, "%.0f px");
    ImGui::SameLine(0.0f, 12.0f);
    ui::segmentedControl("##math_insert_mode", &m_insertMode, {"Bloque $$", "Inline $"}, 170.0f);
    ImGui::SameLine(0.0f, 12.0f);
    if (ui::primaryButton("Insertar en Nota")) {
        insertIntoSelectedNote(app);
    }
    ImGui::SameLine();
    if (ui::subtleButton("Limpiar")) {
        m_formulaBuf[0] = '\0';
    }

    ImGui::Spacing();
    ui::sectionHeader("Vista Previa");
    MathRenderer::renderDisplayBox(m_formulaBuf, m_fontSize);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    Note* activeNote = app.selectedNoteId.empty()
        ? nullptr
        : app.getNoteManager().getNoteById(app.selectedNoteId);

    ui::sectionHeader("Formulas en Nota Activa");
    if (!activeNote) {
        ImGui::TextColored(pal.textMuted, "Ninguna nota seleccionada.");
        ImGui::End();
        return;
    }

    std::vector<MathBlock> blocks = MathRenderer::extractMathBlocks(activeNote->content);
    char countBuf[64];
    std::snprintf(countBuf, sizeof(countBuf), "%zu detectadas", blocks.size());
    ui::badge(activeNote->id.c_str(), pal.accentSoft, pal.accent);
    ImGui::SameLine(0.0f, 8.0f);
    ui::badge(countBuf, pal.bgSurface3, pal.textSecondary);
    ImGui::Spacing();

    if (blocks.empty()) {
        ImGui::TextColored(pal.textMuted, "No se encontraron bloques $...$ o $$...$$ en esta nota.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginChild("##note_math_blocks", ImVec2(0.0f, 0.0f), false)) {
        for (size_t i = 0; i < blocks.size(); ++i) {
            const auto& blk = blocks[i];
            ImGui::PushID(static_cast<int>(i));

            char lineBadge[32];
            std::snprintf(lineBadge, sizeof(lineBadge), "L%d", blk.lineNumber);
            ui::badge(lineBadge, pal.bgSurface3, pal.textSecondary);
            ImGui::SameLine(0.0f, 6.0f);
            ui::badge(blk.displayMode ? "Bloque" : "Inline",
                      blk.displayMode ? pal.accentSoft : pal.bgSurface3,
                      blk.displayMode ? pal.accent : pal.aqua);
            ImGui::SameLine(0.0f, 10.0f);
            if (ui::subtleButton("Editar")) {
                setFormula(blk.rawLatex);
            }
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextColored(pal.textMuted, "%s", blk.rawLatex.c_str());

            if (blk.displayMode) {
                MathRenderer::renderDisplayBox(blk.rawLatex, 20.0f);
            } else {
                MathRenderer::renderInlineWidget(blk.rawLatex, 17.0f);
            }

            ImGui::Spacing();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

}
