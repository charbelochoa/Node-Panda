#include "editor_panel.h"
#include "ai_exporter.h"
#include "texture_manager.h"
#include "markdown_parser.h"
#include "lua_manager.h"
#include "plugin_manager.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"

#include <cstring>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace nodepanda {

void EditorPanel::renderPandaBanner(App& app) {
    const ThemePalette& pal = UITheme::palette();

    if (!m_pandaLoaded) {
        m_pandaLoaded = true;

        namespace fs = std::filesystem;
        fs::path p1 = fs::current_path() / "panda.png";
        fs::path p2 = fs::current_path() / "data" / "panda.png";
        std::string pandaPath = fs::exists(p1) ? p1.string() : p2.string();

        const TextureInfo* tex = app.getTextureManager().getTexture(pandaPath);
        if (tex && tex->textureId != 0)
            m_pandaTexId = static_cast<unsigned int>(tex->textureId);
    }

    constexpr float IMG_SIZE = 76.0f;
    constexpr float PAD_TOP  = 8.0f;
    constexpr float PAD_BOT  = 8.0f;
    constexpr float BANNER_H = PAD_TOP + IMG_SIZE + PAD_BOT;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bgBase);
    if (ImGui::BeginChild("##panda_banner", ImVec2(0, BANNER_H), false,
                          ImGuiWindowFlags_NoScrollbar)) {

        const float availW = ImGui::GetContentRegionAvail().x;

        if (m_pandaTexId != 0) {
            ImGui::SetCursorPos(ImVec2((availW - IMG_SIZE) * 0.5f, PAD_TOP));
            ImGui::Image(
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_pandaTexId)),
                ImVec2(IMG_SIZE, IMG_SIZE));
        } else {
            const char* fallback = "Node Panda";
            ImVec2 fsz = ImGui::CalcTextSize(fallback);
            ImGui::SetCursorPos(
                ImVec2((availW - fsz.x) * 0.5f,
                       PAD_TOP + (IMG_SIZE - fsz.y) * 0.5f));
            ImGui::TextColored(pal.accent, "%s", fallback);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Separator, pal.borderLine);
    ImGui::Separator();
    ImGui::PopStyleColor();
}

std::vector<EditorPanel::LuaCodeBlock> EditorPanel::parseLuaBlocks(const std::string& content) {
    std::vector<LuaCodeBlock> blocks;
    std::istringstream stream(content);
    std::string line;
    size_t lineNum = 0;
    bool inLuaBlock = false;
    LuaCodeBlock current;

    while (std::getline(stream, line)) {
        ++lineNum;
        if (!inLuaBlock) {
            std::string trimmed = line;
            size_t start = trimmed.find_first_not_of(" \t");
            if (start != std::string::npos)
                trimmed = trimmed.substr(start);
            if (trimmed.rfind("```lua", 0) == 0) {
                inLuaBlock = true;
                current.code.clear();
                current.startLine = lineNum;
            }
        } else {
            std::string trimmed = line;
            size_t start = trimmed.find_first_not_of(" \t");
            if (start != std::string::npos)
                trimmed = trimmed.substr(start);

            std::string trimEnd = trimmed;
            while (!trimEnd.empty() && (trimEnd.back() == ' ' || trimEnd.back() == '\t' || trimEnd.back() == '\r'))
                trimEnd.pop_back();
            if (trimEnd == "```") {
                inLuaBlock = false;
                if (!current.code.empty())
                    blocks.push_back(std::move(current));
                current = {};
            } else {
                current.code += line + "\n";
            }
        }
    }
    return blocks;
}

void EditorPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::Begin("Editor de Notas", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    renderPandaBanner(app);
    ImGui::Spacing();

    if (app.selectedNoteId.empty()) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40.0f);
        ImVec2 winSize = ImGui::GetContentRegionAvail();
        float cx = ImGui::GetCursorPosX() + std::max(0.0f, (winSize.x - 280.0f) * 0.5f);
        ImGui::SetCursorPosX(cx);
        ImGui::TextColored(pal.textSecondary, "Selecciona una nota del explorador");
        ImGui::SetCursorPosX(cx);
        ImGui::TextColored(pal.textMuted, "o crea una nueva con + Nota");
        ImGui::End();
        return;
    }

    Note* note = app.getNoteManager().getNoteById(app.selectedNoteId);
    if (!note) {
        ImGui::TextColored(pal.danger, "Nota no encontrada: %s", app.selectedNoteId.c_str());
        ImGui::End();
        return;
    }

    if (m_currentNoteId != note->id) {
        m_currentNoteId = note->id;
        std::memset(m_editBuffer, 0, BUFFER_SIZE);
        std::strncpy(m_editBuffer, note->content.c_str(), BUFFER_SIZE - 1);
        m_previewMode = false;
        m_fieldBuffers.clear();
        for (const auto& [k, v] : note->frontmatter)
            m_fieldBuffers[k] = v;
        std::memset(m_newKey, 0, FIELD_SIZE);
        std::memset(m_newValue, 0, FIELD_SIZE);
        m_exportCachedDepth = -1;
        m_exportCachedNoteId.clear();
    }

    auto saveCurrentNote = [&]() {
        note->frontmatter.clear();
        for (const auto& [k, v] : m_fieldBuffers)
            if (!k.empty()) note->frontmatter[k] = v;
        note->content = std::string(m_editBuffer);
        note->saveToFile();
        note->parseLinks();
        note->parseTitle();
        app.graphNeedsRebuild = true;
        app.rebuildKnowledgeIndices();
        app.getPluginManager().callOnNoteSave(note->id);
        m_backlinksCachedId.clear();
        ui::pushToast("Nota guardada", "success");
    };

    auto appendSnippet = [&](const char* snippet) {
        size_t curLen = std::strlen(m_editBuffer);
        size_t addLen = std::strlen(snippet);
        if (curLen + addLen + 2 < BUFFER_SIZE) {
            if (curLen > 0 && m_editBuffer[curLen - 1] != '\n') {
                m_editBuffer[curLen] = '\n';
                m_editBuffer[curLen + 1] = '\0';
                curLen += 1;
            }
            std::strncat(m_editBuffer, snippet, BUFFER_SIZE - curLen - 1);
            note->dirty = true;
            if (m_previewMode) {
                note->content = std::string(m_editBuffer);
                note->parseLinks();
                note->parseTitle();
            }
        }
    };

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::GetIO().KeyCtrl &&
        ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        saveCurrentNote();
    }

    ImGui::TextColored(pal.textPrimary, "%s", note->title.c_str());
    if (note->dirty) {
        ImGui::SameLine(0.0f, 8.0f);
        ui::badge("Sin guardar", ImVec4(pal.warning.x, pal.warning.y, pal.warning.z, 0.18f), pal.warning);
    }
    {
        std::string tipo = note->getFrontmatter("tipo");
        if (!tipo.empty()) {
            ImGui::SameLine(0.0f, 8.0f);
            ui::badge(tipo.c_str(), pal.accentSoft, pal.accent);
        }
    }

    ImGui::Spacing();

    if (ui::primaryButton("  Guardar  ")) {
        saveCurrentNote();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Guardar la nota en disco (Ctrl+S)");

    ImGui::SameLine(0.0f, 6.0f);

    const char* previewLabel = m_previewMode ? "  Editar  " : "  Preview  ";
    if (ui::subtleButton(previewLabel)) {
        if (m_previewMode) {
            std::memset(m_editBuffer, 0, BUFFER_SIZE);
            std::strncpy(m_editBuffer, note->content.c_str(), BUFFER_SIZE - 1);
        } else {
            note->content = std::string(m_editBuffer);
            note->parseLinks();
            note->parseTitle();
        }
        m_previewMode = !m_previewMode;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(m_previewMode
            ? "Volver al modo edicion"
            : "Previsualizar el Markdown renderizado");

    ImGui::SameLine(0.0f, 6.0f);

    if (ui::subtleButton("  Exportar Contexto  ")) {
        m_exportCachedDepth = -1;
        m_exportCachedNoteId.clear();
        m_justCopied = false;
        ImGui::OpenPopup("Exportar Contexto");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Compilar un contexto estructurado incluyendo notas enlazadas hasta N saltos");

    ImGui::SameLine(0.0f, 10.0f);
    char metaStats[64];
    std::snprintf(metaStats, sizeof(metaStats), "%d enlaces", static_cast<int>(note->links.size()));
    ui::badge(metaStats, pal.bgSurface3, pal.textSecondary);
    ImGui::SameLine(0.0f, 6.0f);
    std::snprintf(metaStats, sizeof(metaStats), "%d campos", static_cast<int>(note->frontmatter.size()));
    ui::badge(metaStats, pal.bgSurface3, pal.textSecondary);

    {
        ImVec2 mc = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(mc, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Exportar Contexto", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoMove)) {

            ImGui::TextColored(pal.aqua, "Exportar contexto estructurado");
            ImGui::TextColored(pal.textMuted, "Nota raiz:");
            ImGui::SameLine();
            ImGui::TextColored(pal.textPrimary, "%s", note->id.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(pal.textSecondary, "Profundidad de enlaces:");
            ImGui::SetNextItemWidth(300.0f);
            ImGui::SliderInt("##xdepth", &m_exportDepth, 0, 3);
            ImGui::SameLine(0.0f, 10.0f);
            switch (m_exportDepth) {
                case 0: ImGui::TextColored(pal.success, "Solo la nota actual"); break;
                case 1: ImGui::TextColored(pal.success, "+ enlaces directos"); break;
                case 2: ImGui::TextColored(pal.warning, "2 saltos (extenso)"); break;
                case 3: ImGui::TextColored(pal.danger, "3 saltos (muy largo)"); break;
            }
            ImGui::Spacing();

            if (m_exportDepth != m_exportCachedDepth ||
                note->id != m_exportCachedNoteId) {
                m_exportCachedDepth = m_exportDepth;
                m_exportCachedNoteId = note->id;
                auto ids = GetContextNoteIds(note->id, m_exportDepth, app.getNoteManager());
                m_exportNoteCount = static_cast<int>(ids.size());
            }

            if (m_exportNoteCount <= 5)
                ImGui::TextColored(pal.success, "Se incluiran %d nota(s).", m_exportNoteCount);
            else if (m_exportNoteCount <= 15)
                ImGui::TextColored(pal.warning, "Se incluiran %d notas (contexto extenso).", m_exportNoteCount);
            else
                ImGui::TextColored(pal.danger, "Se incluiran %d notas - considera reducir profundidad.", m_exportNoteCount);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ui::primaryButton("Copiar al Portapapeles", ImVec2(210.0f, 0.0f))) {
                std::string ctx = CompileContext(note->id, m_exportDepth, app.getNoteManager());
                if (!ctx.empty()) {
                    ImGui::SetClipboardText(ctx.c_str());
                    m_justCopied = true;
                    ui::pushToast("Contexto copiado al portapapeles", "success");
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Compila el contexto y lo copia al portapapeles del sistema");

            if (m_justCopied) {
                ImGui::SameLine(0.0f, 10.0f);
                ImGui::TextColored(pal.success, "Copiado");
            }

            ImGui::SameLine();
            float closeX = ImGui::GetContentRegionAvail().x - 80.0f;
            if (closeX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + closeX);
            if (ui::subtleButton("Cerrar", ImVec2(80.0f, 0.0f))) {
                m_justCopied = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::Spacing();
            ImGui::EndPopup();
        }
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, pal.borderLine);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Metadatos (Frontmatter)",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(8.0f);

        if (m_fieldBuffers.empty())
            ImGui::TextColored(pal.textMuted, "Sin metadatos - agrega campos abajo.");

        std::string toDelete;
        for (auto& [key, val] : m_fieldBuffers) {
            ImGui::PushID(key.c_str());

            ImGui::TextColored(pal.accent, "%s:", key.c_str());
            ImGui::SameLine();

            char buf[FIELD_SIZE] = {};
            std::strncpy(buf, val.c_str(), FIELD_SIZE - 1);
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
            std::string inputId = "##v_" + key;
            if (ImGui::InputText(inputId.c_str(), buf, FIELD_SIZE)) {
                val = std::string(buf);
                note->dirty = true;
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ui::dangerButton("X", ImVec2(28.0f, 0.0f))) {
                toDelete = key;
                note->dirty = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Eliminar este campo de metadatos");

            ImGui::PopID();
        }
        if (!toDelete.empty()) {
            m_fieldBuffers.erase(toDelete);
            note->frontmatter.erase(toDelete);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(pal.textSecondary, "Agregar campo:");
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputTextWithHint("##nk", "clave", m_newKey, FIELD_SIZE);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 42.0f);
        ImGui::InputTextWithHint("##nv", "valor", m_newValue, FIELD_SIZE);
        ImGui::SameLine();
        if (ui::subtleButton(" + ", ImVec2(34.0f, 0.0f))) {
            std::string k(m_newKey), v(m_newValue);
            if (!k.empty()) {
                m_fieldBuffers[k] = v;
                note->frontmatter[k] = v;
                note->dirty = true;
                std::memset(m_newKey, 0, FIELD_SIZE);
                std::memset(m_newValue, 0, FIELD_SIZE);
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Agregar este campo al frontmatter YAML de la nota");

        ImGui::Unindent(8.0f);
        ImGui::Spacing();
    }

    ImGui::PushStyleColor(ImGuiCol_Separator, pal.borderLine);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 4.0f));
    ImGui::TextColored(pal.textMuted, "/");
    ImGui::SameLine();
    if (ui::subtleButton("+ TODO")) {
        appendSnippet("- TODO ");
    }
    ImGui::SameLine();
    if (ui::subtleButton("+ DOING")) {
        appendSnippet("- DOING ");
    }
    ImGui::SameLine();
    if (ui::subtleButton("+ Bloque")) {
        appendSnippet("- Nuevo bloque\n  - Detalle");
    }
    ImGui::SameLine();
    if (ui::subtleButton("+ Cloze")) {
        appendSnippet("{{cloze respuesta}}");
    }
    ImGui::SameLine();
    if (ui::subtleButton("+ LaTeX")) {
        appendSnippet("$$E = mc^2$$");
    }
    ImGui::SameLine();
    if (ui::subtleButton("+ Enlace")) {
        appendSnippet("[[NuevaNota]]");
    }
    ImGui::SameLine();
    if (ui::subtleButton("+ Propiedad")) {
        appendSnippet("clave:: valor");
    }
    ImGui::PopStyleVar(2);
    ImGui::Spacing();

    if (m_backlinksCachedId != note->id) {
        m_backlinksCachedId = note->id;
        m_cachedBacklinks = app.getNoteManager().getBacklinks(note->id);
    }

    float backlinksH = m_cachedBacklinks.empty()
        ? 48.0f
        : std::min(150.0f, 40.0f + static_cast<float>(m_cachedBacklinks.size()) * 24.0f);
    float bodyH = ImGui::GetContentRegionAvail().y - backlinksH - 16.0f;
    if (bodyH < 80.0f) bodyH = 80.0f;

    if (m_previewMode) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bgSurface2);
        ImGui::BeginChild("##preview", ImVec2(0, bodyH), true);
        ImGui::PopStyleColor();

        std::string clickedLink;
        std::string notesDir = app.getNoteManager().getNotesDirectory().string();
        MarkdownParser::renderToImGui(note->content, clickedLink,
                                      &app.getTextureManager(), notesDir);
        if (!clickedLink.empty()) {
            std::string resolved = app.getNoteManager().resolveAlias(clickedLink);
            if (app.getNoteManager().getNoteById(resolved)) {
                app.selectedNoteId = resolved;
                m_currentNoteId.clear();
            }
        }

        auto luaBlocks = parseLuaBlocks(note->content);
        if (!luaBlocks.empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(pal.aqua, "Bloques Lua detectados: %d", static_cast<int>(luaBlocks.size()));
            ImGui::Spacing();

            for (size_t i = 0; i < luaBlocks.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                const auto& block = luaBlocks[i];

                ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bgBase);
                float codeH = std::min(150.0f,
                    ImGui::GetTextLineHeightWithSpacing() *
                    static_cast<float>(std::count(block.code.begin(), block.code.end(), '\n') + 1) + 8.0f);
                char childId[32];
                std::snprintf(childId, sizeof(childId), "##luablock_%d", static_cast<int>(i));
                ImGui::BeginChild(childId, ImVec2(0, codeH), true);
                ImGui::PushStyleColor(ImGuiCol_Text, pal.textPrimary);
                ImGui::TextUnformatted(block.code.c_str());
                ImGui::PopStyleColor();
                ImGui::EndChild();
                ImGui::PopStyleColor();

                char btnLabel[64];
                std::snprintf(btnLabel, sizeof(btnLabel), "  Run bloque %d  ", static_cast<int>(i + 1));
                if (ui::primaryButton(btnLabel)) {
                    auto& lua = app.getLuaManager();
                    lua.logInfo(">>> [Bloque " + std::to_string(i + 1) +
                                " desde linea " + std::to_string(block.startLine) + "]");
                    auto result = lua.execute(block.code);
                    char timeBuf[64];
                    std::snprintf(timeBuf, sizeof(timeBuf), "Ejecutado en %.2f ms", result.elapsedMs);
                    lua.logInfo(timeBuf);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Ejecutar este bloque Lua en la consola");

                ImGui::SameLine();
                ImGui::TextColored(pal.textMuted, "linea %d", static_cast<int>(block.startLine));
                ImGui::Spacing();
                ImGui::PopID();
            }
        }

        ImGui::EndChild();
    } else {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, pal.bgBase);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, pal.bgSurface1);
        if (ImGui::InputTextMultiline("##editor", m_editBuffer, BUFFER_SIZE,
                                      ImVec2(-1.0f, bodyH),
                                      ImGuiInputTextFlags_AllowTabInput)) {
            note->dirty = true;
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::PushStyleColor(ImGuiCol_Separator, pal.borderLine);
    ImGui::Separator();
    ImGui::PopStyleColor();

    std::string blHeader = "Backlinks (" +
                           std::to_string(m_cachedBacklinks.size()) +
                           ")###backlinks";
    if (ImGui::CollapsingHeader(blHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_cachedBacklinks.empty()) {
            ImGui::TextColored(pal.textMuted, "  Ninguna nota enlaza aqui.");
        } else {
            ImGui::BeginChild("##bl_scroll", ImVec2(0, 0), false);
            for (size_t i = 0; i < m_cachedBacklinks.size(); ++i) {
                const auto& bl = m_cachedBacklinks[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::PushStyleColor(ImGuiCol_Text, pal.aqua);
                if (ImGui::Selectable(("  << [[" + bl + "]]").c_str(), false)) {
                    app.selectedNoteId = bl;
                    m_currentNoteId.clear();
                    m_backlinksCachedId.clear();
                }
                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    auto* blNote = app.getNoteManager().getNoteById(bl);
                    if (blNote) {
                        ImGui::BeginTooltip();
                        ImGui::TextColored(pal.accent, "%s", blNote->title.c_str());
                        std::string prev = blNote->content.substr(0, 180);
                        if (blNote->content.size() > 180) prev += "...";
                        ImGui::TextWrapped("%s", prev.c_str());
                        ImGui::EndTooltip();
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

}
