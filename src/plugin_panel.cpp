#include "plugin_panel.h"
#include "app.h"
#include "plugin_manager.h"
#include "imgui.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <cstring>
#include <algorithm>

namespace nodepanda {

static const ImVec4 kTeal       = ImVec4(0.28f, 0.82f, 0.76f, 1.0f);
static const ImVec4 kTealDim    = ImVec4(0.16f, 0.50f, 0.46f, 1.0f);
static const ImVec4 kGreen      = ImVec4(0.36f, 0.72f, 0.48f, 1.0f);
static const ImVec4 kRed        = ImVec4(0.90f, 0.35f, 0.30f, 1.0f);
static const ImVec4 kAmber      = ImVec4(0.90f, 0.72f, 0.42f, 1.0f);
static const ImVec4 kDimText    = ImVec4(0.45f, 0.46f, 0.58f, 1.0f);
static const ImVec4 kBrightText = ImVec4(0.86f, 0.87f, 0.93f, 1.0f);

void PluginPanel::render(App& app) {
    auto& pm = app.getPluginManager();

    ImGui::Begin("Gestor de Plugins", nullptr, ImGuiWindowFlags_NoCollapse);

    auto& plugins = pm.getPlugins();
    int total   = (int)plugins.size();
    int active  = 0;
    int errored = 0;
    for (const auto& p : plugins) {
        if (p.manifest.enabled) active++;
        if (p.hasError) errored++;
    }

    ImGui::TextColored(kTeal, "Plugins");
    ImGui::SameLine();
    ImGui::TextColored(kDimText, "  %d total  |  %d activos", total, active);
    if (errored > 0) {
        ImGui::SameLine();
        ImGui::TextColored(kRed, "  |  %d errores", errored);
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(-80.0f);
    ImGui::InputTextWithHint("##plugin_search", "Buscar plugin...",
                             m_searchBuf, sizeof(m_searchBuf));
    ImGui::SameLine();
    if (ImGui::SmallButton("Reescanear")) {
        pm.rescanPlugins();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reescanear carpeta data/plugins/");

    ImGui::Separator();

    std::string filter(m_searchBuf);
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    ImGui::BeginChild("##plugin_list", ImVec2(0, 0), false);

    for (auto& plugin : pm.getPlugins()) {

        if (!filter.empty()) {
            std::string name = plugin.manifest.name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::string desc = plugin.manifest.description;
            std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);
            if (name.find(filter) == std::string::npos &&
                desc.find(filter) == std::string::npos) {
                continue;
            }
        }

        ImGui::PushID(plugin.manifest.id.c_str());

        if (plugin.hasError) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.06f, 0.06f, 1.0f));
        } else if (plugin.manifest.enabled) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.10f, 0.09f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.10f, 1.0f));
        }

        ImGui::BeginChild("##card", ImVec2(0, 0), true,
                          ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);

        bool enabled = plugin.manifest.enabled;
        if (ImGui::Checkbox("##enable", &enabled)) {
            if (enabled)
                pm.enablePlugin(plugin.manifest.id);
            else
                pm.disablePlugin(plugin.manifest.id);
        }
        ImGui::SameLine();

        ImGui::TextColored(plugin.manifest.enabled ? kBrightText : kDimText,
                           "%s", plugin.manifest.name.c_str());
        ImGui::SameLine();
        ImGui::TextColored(kDimText, "v%s", plugin.manifest.version.c_str());

        if (!plugin.manifest.author.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(kDimText, "  por %s", plugin.manifest.author.c_str());
        }

        if (!plugin.manifest.description.empty()) {
            ImGui::TextColored(kDimText, "  %s", plugin.manifest.description.c_str());
        }

        if (!plugin.manifest.hooks.empty()) {
            ImGui::TextColored(ImVec4(0.30f, 0.32f, 0.42f, 1.0f), "  Hooks:");
            for (const auto& hook : plugin.manifest.hooks) {
                ImGui::SameLine();
                ImGui::TextColored(kTealDim, "%s", hook.c_str());
            }
        }

        if (plugin.hasError) {
            ImGui::TextColored(kRed, "  Error: %s", plugin.lastError.c_str());
        }

        if (plugin.manifest.enabled) {
            ImGui::SameLine(ImGui::GetWindowWidth() - 80.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.20f, 0.34f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.17f, 0.33f, 0.52f, 1.0f));
            if (ImGui::SmallButton("Recargar")) {
                pm.reloadPlugin(plugin.manifest.id);
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PopID();
    }

    if (plugins.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(kDimText, "  No se encontraron plugins.");
        ImGui::Spacing();
        ImGui::TextColored(kDimText, "  Crea una carpeta en data/plugins/ con:");
        ImGui::TextColored(kTealDim, "    manifest.json  +  plugin.lua");
    }

    ImGui::EndChild();

    ImGui::End();

    for (auto& panel : pm.getPanels()) {
        if (!panel.open) continue;

        std::string title = panel.name + " [" + panel.pluginId + "]";
        ImGui::Begin(title.c_str(), &panel.open, ImGuiWindowFlags_NoCollapse);

        if (panel.luaState && panel.renderRef != -1) {
            lua_rawgeti(panel.luaState, LUA_REGISTRYINDEX, panel.renderRef);
            if (lua_pcall(panel.luaState, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(panel.luaState, -1);
                ImGui::TextColored(kRed, "Error: %s", err ? err : "unknown");
                lua_pop(panel.luaState, 1);
            }
        }

        ImGui::End();
    }

    auto& notifications = pm.getNotifications();
    if (!notifications.empty()) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float yOffset = 40.0f;

        for (int i = (int)notifications.size() - 1; i >= 0; --i) {
            auto& notif = notifications[i];
            notif.timer -= ImGui::GetIO().DeltaTime;
            if (notif.timer <= 0.0f) {
                notifications.erase(notifications.begin() + i);
                continue;
            }

            float alpha = (notif.timer < 1.0f) ? notif.timer : 1.0f;

            ImVec4 bgColor;
            ImVec4 textColor;
            if (notif.type == "error") {
                bgColor   = ImVec4(0.20f, 0.06f, 0.06f, 0.92f * alpha);
                textColor = ImVec4(0.95f, 0.40f, 0.35f, alpha);
            } else if (notif.type == "warn") {
                bgColor   = ImVec4(0.20f, 0.16f, 0.06f, 0.92f * alpha);
                textColor = ImVec4(0.90f, 0.72f, 0.42f, alpha);
            } else if (notif.type == "success") {
                bgColor   = ImVec4(0.06f, 0.16f, 0.10f, 0.92f * alpha);
                textColor = ImVec4(0.36f, 0.88f, 0.55f, alpha);
            } else {
                bgColor   = ImVec4(0.08f, 0.12f, 0.18f, 0.92f * alpha);
                textColor = ImVec4(0.28f, 0.82f, 0.76f, alpha);
            }

            ImGui::SetNextWindowPos(
                ImVec2(vp->WorkPos.x + vp->WorkSize.x - 320.0f, vp->WorkPos.y + yOffset),
                ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Always);

            ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            char winId[64];
            std::snprintf(winId, sizeof(winId), "##notif_%d", i);
            ImGui::Begin(winId, nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoDocking);

            ImGui::TextColored(textColor, "%s", notif.text.c_str());

            yOffset += ImGui::GetWindowHeight() + 8.0f;
            ImGui::End();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }
    }
}

}
