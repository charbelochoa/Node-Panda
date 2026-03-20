

#include "lua_console.h"
#include "app.h"
#include "lua_manager.h"
#include "imgui.h"

#include <cstring>

namespace nodepanda {

void LuaConsole::render(App& app) {
    ImGui::Begin("Consola Lua", nullptr, ImGuiWindowFlags_NoCollapse);

    auto& lua = app.getLuaManager();
    const auto& log = lua.getLog();

    if (ImGui::SmallButton("Limpiar")) {
        lua.clearLog();
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.45f, 0.46f, 0.58f, 1.0f),
        "  %d entradas  |  Timeout: %dms", (int)log.size(), lua.timeoutMs);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.38f, 0.40f, 0.52f, 1.0f), "  |  API: notas, grafo, app");

    ImGui::Separator();

    float footerH = ImGui::GetStyle().ItemSpacing.y +
                    ImGui::GetFrameHeightWithSpacing() * 3.5f; 
    ImGui::BeginChild("##lua_log", ImVec2(0, -footerH), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

    for (const auto& entry : log) {
        ImVec4 color;
        const char* prefix;
        switch (entry.type) {
            case LuaLogEntry::OUTPUT:
                color  = ImVec4(0.82f, 0.85f, 0.92f, 1.0f); 
                prefix = "";
                break;
            case LuaLogEntry::LOG_ERROR:
                color  = ImVec4(0.95f, 0.40f, 0.35f, 1.0f); 
                prefix = "[ERROR] ";
                break;
            case LuaLogEntry::INFO:
            default:
                color  = ImVec4(0.45f, 0.75f, 0.65f, 1.0f); 
                prefix = "[INFO] ";
                break;
        }

        ImGui::TextColored(ImVec4(0.30f, 0.32f, 0.42f, 1.0f),
                           "[%s]", entry.timestamp.c_str());
        ImGui::SameLine();
        ImGui::TextColored(color, "%s%s", prefix, entry.text.c_str());
    }

    ImGui::PopStyleVar();

    if (m_scrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    m_scrollToBottom = false;

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), ">");
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.12f, 1.0f));
    bool enterPressed = false;

    ImGui::PushItemWidth(-70.0f);
    if (ImGui::InputTextMultiline("##lua_input", m_inputBuffer, INPUT_SIZE,
                                   ImVec2(-70.0f, ImGui::GetFrameHeightWithSpacing() * 2.5f),
                                   ImGuiInputTextFlags_CtrlEnterForNewLine |
                                   ImGuiInputTextFlags_EnterReturnsTrue)) {
        enterPressed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.45f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.18f, 0.58f, 0.52f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.08f, 0.35f, 0.30f, 1.0f));
    bool runClicked = ImGui::Button("Run\n(Enter)", ImVec2(60.0f, ImGui::GetFrameHeightWithSpacing() * 2.5f));
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ejecutar codigo Lua\nEnter = ejecutar | Ctrl+Enter = nueva linea");

    if ((enterPressed || runClicked) && std::strlen(m_inputBuffer) > 0) {
        std::string code(m_inputBuffer);

        lua.logInfo(">>> " + code);

        auto result = lua.execute(code);

        char timeBuf[64];
        std::snprintf(timeBuf, sizeof(timeBuf), "Ejecutado en %.2f ms", result.elapsedMs);
        lua.logInfo(timeBuf);

        m_inputBuffer[0] = '\0';
        m_scrollToBottom = true;
        m_focusInput = true;
    }

    if (m_focusInput) {
        ImGui::SetKeyboardFocusHere(-1);
        m_focusInput = false;
    }

    ImGui::End();
}

} 
