

#include "editor_panel.h"
#include "ai_exporter.h"
#include "texture_manager.h"
#include "markdown_parser.h"
#include "lua_manager.h"
#include "imgui.h"

#include <cstring>
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace nodepanda {


void EditorPanel::renderPandaBanner(App& app) {

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

    constexpr float IMG_SIZE = 100.0f;
    constexpr float PAD_TOP  = 10.0f;
    constexpr float GAP      = 8.0f;   
    constexpr float TEXT_H   = 18.0f;  
    constexpr float PAD_BOT  = 10.0f;
    constexpr float BANNER_H = PAD_TOP + IMG_SIZE + GAP + TEXT_H + PAD_BOT;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
    if (ImGui::BeginChild("##panda_banner", ImVec2(0, BANNER_H), false,
                          ImGuiWindowFlags_NoScrollbar)) {

        const float availW = ImGui::GetContentRegionAvail().x;

        if (m_pandaTexId != 0) {
            ImGui::SetCursorPos(ImVec2((availW - IMG_SIZE) * 0.5f, PAD_TOP));
            ImGui::Image(
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_pandaTexId)),
                ImVec2(IMG_SIZE, IMG_SIZE));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Node Panda — tu asistente de notas enlazadas");
        } else {
            const char* fallback = "Node Panda";
            ImVec2 fsz = ImGui::CalcTextSize(fallback);
            ImGui::SetCursorPos(
                ImVec2((availW - fsz.x) * 0.5f,
                       PAD_TOP + (IMG_SIZE - fsz.y) * 0.5f));
            ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "%s", fallback);
        }

        const char* greeting = "Asistente listo";
        ImVec2 tsz = ImGui::CalcTextSize(greeting);
        ImGui::SetCursorPos(
            ImVec2((availW - tsz.x) * 0.5f, PAD_TOP + IMG_SIZE + GAP));
        ImGui::TextDisabled("%s", greeting);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(); 

    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::Begin("Editor de Notas", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    renderPandaBanner(app);
    ImGui::Spacing();

    if (app.selectedNoteId.empty()) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40.0f);
        ImVec2 winSize = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPosX((winSize.x - 280.0f) * 0.5f + ImGui::GetCursorPosX());
        ImGui::TextColored(ImVec4(0.35f, 0.36f, 0.46f, 1.0f),
                           "Selecciona una nota del explorador");
        ImGui::SetCursorPosX((winSize.x - 280.0f) * 0.5f + ImGui::GetCursorPosX());
        ImGui::TextColored(ImVec4(0.28f, 0.30f, 0.40f, 1.0f),
                           "o crea una nueva con  + Nota");
        ImGui::End();
        return;
    }

    Note* note = app.getNoteManager().getNoteById(app.selectedNoteId);
    if (!note) {
        ImGui::TextColored(ImVec4(0.90f, 0.40f, 0.35f, 1.0f),
                           "Nota no encontrada: %s", app.selectedNoteId.c_str());
        ImGui::End();
        return;
    }

    if (m_currentNoteId != note->id) {
        m_currentNoteId = note->id;
        memset(m_editBuffer, 0, BUFFER_SIZE);
        strncpy(m_editBuffer, note->content.c_str(), BUFFER_SIZE - 1);
        m_previewMode = false;
        m_fieldBuffers.clear();
        for (const auto& [k, v] : note->frontmatter)
            m_fieldBuffers[k] = v;
        memset(m_newKey,   0, FIELD_SIZE);
        memset(m_newValue, 0, FIELD_SIZE);
        m_exportCachedDepth  = -1;
        m_exportCachedNoteId.clear();
    }

    ImGui::TextColored(ImVec4(0.80f, 0.82f, 0.96f, 1.0f),
                       "%s", note->title.c_str());
    if (note->dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.88f, 0.72f, 0.22f, 1.0f), " [sin guardar]");
    }
    {
        std::string tipo = note->getFrontmatter("tipo");
        if (!tipo.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.45f, 0.47f, 0.62f, 1.0f),
                               " [%s]", tipo.c_str());
        }
    }

    ImGui::Spacing();

    

  
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.20f, 0.36f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.17f, 0.30f, 0.52f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.20f, 0.38f, 0.62f, 1.0f));
    if (ImGui::Button("  Guardar  ")) {
        note->frontmatter.clear();
        for (const auto& [k, v] : m_fieldBuffers)
            if (!k.empty()) note->frontmatter[k] = v;
        note->content = std::string(m_editBuffer);
        note->saveToFile();
        note->parseLinks();
        note->parseTitle();
        app.graphNeedsRebuild = true;
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Guardar la nota en disco  (Ctrl+S)");

    ImGui::SameLine(0.0f, 4.0f);

    const char* previewLabel = m_previewMode ? "  Editar  " : "  Preview  ";
    if (ImGui::Button(previewLabel)) {
        if (m_previewMode) {
            memset(m_editBuffer, 0, BUFFER_SIZE);
            strncpy(m_editBuffer, note->content.c_str(), BUFFER_SIZE - 1);
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

    ImGui::SameLine(0.0f, 8.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.20f, 0.22f, 0.32f, 1.0f));
    ImGui::Text("|");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 8.0f);

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f, 0.30f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.12f, 0.44f, 0.50f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.06f, 0.24f, 0.28f, 1.0f));
    if (ImGui::Button("  Exportar Contexto  ")) {
        m_exportCachedDepth  = -1;
        m_exportCachedNoteId.clear();
        m_justCopied         = false;
        ImGui::OpenPopup("Exportar Contexto");
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Compilar un contexto estructurado\n"
                          "incluyendo notas enlazadas hasta N saltos");

    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.33f, 0.34f, 0.44f, 1.0f),
                       "  |  %d enlaces  |  %d campos",
                       (int)note->links.size(),
                       (int)note->frontmatter.size());

    
    {
        ImVec2 mc = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(mc, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Exportar Contexto", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoMove)) {
            
            ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f),
                               "Exportar contexto estructurado");
            ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.66f, 1.0f),
                               "Nota raiz: ");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.85f, 0.86f, 0.96f, 1.0f),
                               "%s", note->id.c_str());
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator,
                                  ImVec4(0.18f, 0.20f, 0.30f, 0.80f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            
            ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.45f, 1.0f),
                               "Profundidad de enlaces:");
            ImGui::SetNextItemWidth(310.0f);
            ImGui::SliderInt("##xdepth", &m_exportDepth, 0, 3);
            ImGui::SameLine(0.0f, 10.0f);
            switch (m_exportDepth) {
                case 0: ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.55f, 1.0f),
                                           "Solo la nota actual"); break;
                case 1: ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.55f, 1.0f),
                                           "+ enlaces directos"); break;
                case 2: ImGui::TextColored(ImVec4(0.88f, 0.78f, 0.28f, 1.0f),
                                           "2 saltos (extenso)"); break;
                case 3: ImGui::TextColored(ImVec4(0.88f, 0.45f, 0.28f, 1.0f),
                                           "3 saltos (muy largo)"); break;
            }
            ImGui::Spacing();

            if (m_exportDepth != m_exportCachedDepth ||
                note->id      != m_exportCachedNoteId) {
                m_exportCachedDepth  = m_exportDepth;
                m_exportCachedNoteId = note->id;
                auto ids             = GetContextNoteIds(note->id, m_exportDepth,
                                                         app.getNoteManager());
                m_exportNoteCount    = static_cast<int>(ids.size());
            }

            if (m_exportNoteCount <= 5)
                ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.60f, 1.0f),
                                   "Se incluiran %d nota(s).", m_exportNoteCount);
            else if (m_exportNoteCount <= 15)
                ImGui::TextColored(ImVec4(0.88f, 0.78f, 0.28f, 1.0f),
                                   "Se incluiran %d notas (contexto extenso).",
                                   m_exportNoteCount);
            else
                ImGui::TextColored(ImVec4(0.88f, 0.40f, 0.28f, 1.0f),
                                   "Se incluiran %d notas — considera reducir profundidad.",
                                   m_exportNoteCount);

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator,
                                  ImVec4(0.18f, 0.20f, 0.30f, 0.80f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.10f, 0.32f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImVec4(0.15f, 0.46f, 0.26f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  ImVec4(0.08f, 0.24f, 0.13f, 1.0f));
            if (ImGui::Button("Copiar al Portapapeles", ImVec2(210.0f, 0.0f))) {
                std::string ctx = CompileContext(note->id, m_exportDepth,
                                                 app.getNoteManager());
                if (!ctx.empty()) {
                    ImGui::SetClipboardText(ctx.c_str());
                    m_justCopied = true;
                }
            }
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Compila el contexto y lo copia al portapapeles del sistema");

            if (m_justCopied) {
                ImGui::SameLine(0.0f, 10.0f);
                ImGui::TextColored(ImVec4(0.28f, 0.95f, 0.60f, 1.0f),
                                   "  Copiado!");
            }

            ImGui::SameLine();
            float closeX = ImGui::GetContentRegionAvail().x - 70.0f;
            if (closeX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + closeX);
            if (ImGui::Button("Cerrar", ImVec2(70.0f, 0.0f))) {
                m_justCopied = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::Spacing();
            ImGui::EndPopup();
        }
    }

    
    ImGui::Spacing();
    ImGui::Spacing();   
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Metadatos (Frontmatter)",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(8.0f);

        if (m_fieldBuffers.empty())
            ImGui::TextColored(ImVec4(0.38f, 0.40f, 0.50f, 1.0f),
                               "Sin metadatos — agrega campos abajo.");

        std::string toDelete;
        for (auto& [key, val] : m_fieldBuffers) {
            ImGui::PushID(key.c_str());

            ImGui::TextColored(ImVec4(0.35f, 0.72f, 0.90f, 1.0f),
                               "%s:", key.c_str());
            ImGui::SameLine();

            char buf[FIELD_SIZE] = {};
            strncpy(buf, val.c_str(), FIELD_SIZE - 1);
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
            std::string inputId = "##v_" + key;
            if (ImGui::InputText(inputId.c_str(), buf, FIELD_SIZE)) {
                val         = std::string(buf);
                note->dirty = true;
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                toDelete    = key;
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
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.16f, 0.18f, 0.28f, 0.60f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::TextColored(ImVec4(0.40f, 0.65f, 0.35f, 1.0f), "Agregar campo:");
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputTextWithHint("##nk", "clave", m_newKey, FIELD_SIZE);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 36.0f);
        ImGui::InputTextWithHint("##nv", "valor", m_newValue, FIELD_SIZE);
        ImGui::SameLine();
        if (ImGui::SmallButton(" + ")) {
            std::string k(m_newKey), v(m_newValue);
            if (!k.empty()) {
                m_fieldBuffers[k]     = v;
                note->frontmatter[k]  = v;
                note->dirty           = true;
                memset(m_newKey,   0, FIELD_SIZE);
                memset(m_newValue, 0, FIELD_SIZE);
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Agregar este campo al frontmatter YAML de la nota");

        ImGui::Unindent(8.0f);
        ImGui::Spacing();
    }

    
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (m_backlinksCachedId != note->id) {
        m_backlinksCachedId = note->id;
        m_cachedBacklinks   = app.getNoteManager().getBacklinks(note->id);
    }

    float backlinksH = m_cachedBacklinks.empty()
        ? 48.0f
        : std::min(160.0f, 40.0f + (float)m_cachedBacklinks.size() * 24.0f);
    float bodyH = ImGui::GetContentRegionAvail().y - backlinksH - 16.0f;
    if (bodyH < 80.0f) bodyH = 80.0f;

    if (m_previewMode) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.08f, 0.12f, 1.0f));
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
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.18f, 0.20f, 0.30f, 0.80f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f),
                               "Bloques Lua detectados: %d", (int)luaBlocks.size());
            ImGui::Spacing();

            for (size_t i = 0; i < luaBlocks.size(); ++i) {
                ImGui::PushID((int)i);
                const auto& block = luaBlocks[i];

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.07f, 1.0f));
                float codeH = std::min(150.0f,
                    ImGui::GetTextLineHeightWithSpacing() *
                    (float)(std::count(block.code.begin(), block.code.end(), '\n') + 1) + 8.0f);
                char childId[32];
                std::snprintf(childId, sizeof(childId), "##luablock_%d", (int)i);
                ImGui::BeginChild(childId, ImVec2(0, codeH), true);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.82f, 0.65f, 1.0f));
                ImGui::TextUnformatted(block.code.c_str());
                ImGui::PopStyleColor();
                ImGui::EndChild();
                ImGui::PopStyleColor(); 

                
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.45f, 0.40f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.18f, 0.58f, 0.52f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.08f, 0.35f, 0.30f, 1.0f));
                char btnLabel[64];
                std::snprintf(btnLabel, sizeof(btnLabel), "  Run bloque %d  ", (int)(i + 1));
                if (ImGui::Button(btnLabel)) {
                    auto& lua = app.getLuaManager();
                    lua.logInfo(">>> [Bloque " + std::to_string(i + 1) +
                                " desde linea " + std::to_string(block.startLine) + "]");
                    auto result = lua.execute(block.code);
                    char timeBuf[64];
                    std::snprintf(timeBuf, sizeof(timeBuf),
                                  "Ejecutado en %.2f ms", result.elapsedMs);
                    lua.logInfo(timeBuf);
                }
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Ejecutar este bloque Lua en la consola");

                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.35f, 0.37f, 0.48f, 1.0f),
                                   "linea %d", (int)block.startLine);
                ImGui::Spacing();
                ImGui::PopID();
            }
        }

        ImGui::EndChild();
    } else {
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg,   ImVec4(0.08f, 0.08f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.06f, 0.06f, 0.09f, 1.0f));
        if (ImGui::InputTextMultiline("##editor", m_editBuffer, BUFFER_SIZE,
                                     ImVec2(-1.0f, bodyH),
                                     ImGuiInputTextFlags_AllowTabInput)) {
            note->dirty = true;
        }
        ImGui::PopStyleColor(2);
    }

    
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();

    std::string blHeader = "Backlinks (" +
                           std::to_string(m_cachedBacklinks.size()) +
                           ")###backlinks";
    if (ImGui::CollapsingHeader(blHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_cachedBacklinks.empty()) {
            ImGui::TextColored(ImVec4(0.35f, 0.36f, 0.46f, 1.0f),
                               "  Ninguna nota enlaza aqui.");
        } else {
            ImGui::BeginChild("##bl_scroll", ImVec2(0, 0), false);
            for (size_t i = 0; i < m_cachedBacklinks.size(); ++i) {
                const auto& bl = m_cachedBacklinks[i];
                ImGui::PushID((int)i);
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.28f, 0.80f, 0.55f, 1.0f));
                if (ImGui::Selectable(("  << [[" + bl + "]]").c_str(), false)) {
                    app.selectedNoteId  = bl;
                    m_currentNoteId.clear();
                    m_backlinksCachedId.clear();
                }
                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    auto* blNote = app.getNoteManager().getNoteById(bl);
                    if (blNote) {
                        ImGui::BeginTooltip();
                        ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f),
                                           "%s", blNote->title.c_str());
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
