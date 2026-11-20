#include "plugin_panel.h"
#include "app.h"
#include "plugin_manager.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <string>

namespace nodepanda {

namespace {

std::string toLowerPlugin(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

}

void PluginPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();
    auto& pm = app.getPluginManager();

    ImGui::Begin("Gestor de Plugins", nullptr, ImGuiWindowFlags_NoCollapse);

    auto& plugins = pm.getPlugins();
    int total = static_cast<int>(plugins.size());
    int active = 0;
    int errored = 0;
    for (const auto& p : plugins) {
        if (p.manifest.enabled) active++;
        if (p.hasError) errored++;
    }

    float availW = ImGui::GetContentRegionAvail().x;
    float cardW = std::max(80.0f, (availW - 16.0f) / 3.0f);
    ui::metricCard("##plug_total", "Total", std::to_string(total), pal.textPrimary, cardW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##plug_active", "Activos", std::to_string(active), pal.success, cardW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##plug_err", "Errores", std::to_string(errored), errored > 0 ? pal.danger : pal.textMuted, cardW);

    ImGui::Spacing();
    ImGui::SetNextItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x - 102.0f));
    ImGui::InputTextWithHint("##plugin_search", "Buscar plugin...",
                             m_searchBuf, sizeof(m_searchBuf));
    ImGui::SameLine(0.0f, 6.0f);
    if (ui::subtleButton("Reescanear", ImVec2(96.0f, 0.0f))) {
        pm.rescanPlugins();
        ui::pushToast("Plugins reescaneados", "info");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Reescanear carpeta data/plugins/");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    std::string filter = toLowerPlugin(m_searchBuf);

    ImGui::BeginChild("##plugin_list", ImVec2(0, 0), false);

    for (auto& plugin : pm.getPlugins()) {
        if (!filter.empty()) {
            std::string name = toLowerPlugin(plugin.manifest.name);
            std::string desc = toLowerPlugin(plugin.manifest.description);
            if (name.find(filter) == std::string::npos &&
                desc.find(filter) == std::string::npos) {
                continue;
            }
        }

        ImGui::PushID(plugin.manifest.id.c_str());

        if (ui::beginCard("##plugin_card", ImVec2(0, 0), true)) {
            bool enabled = plugin.manifest.enabled;
            if (ImGui::Checkbox("##enable", &enabled)) {
                if (enabled)
                    pm.enablePlugin(plugin.manifest.id);
                else
                    pm.disablePlugin(plugin.manifest.id);
            }
            ImGui::SameLine(0.0f, 6.0f);

            ImVec4 dotCol = plugin.hasError ? pal.danger : (plugin.manifest.enabled ? pal.success : pal.textMuted);
            ui::statusDot("", dotCol, plugin.manifest.enabled && !plugin.hasError);
            ImGui::SameLine(0.0f, 4.0f);

            ImGui::TextColored(plugin.manifest.enabled ? pal.textPrimary : pal.textMuted,
                               "%s", plugin.manifest.name.c_str());
            ImGui::SameLine(0.0f, 8.0f);

            std::string verStr = "v" + plugin.manifest.version;
            ui::badge(verStr.c_str(), pal.bgSurface3, pal.textSecondary);
            ImGui::SameLine(0.0f, 6.0f);

            if (plugin.hasError) {
                ui::badge("Error", ImVec4(pal.danger.x, pal.danger.y, pal.danger.z, 0.20f), pal.danger);
            } else if (plugin.manifest.enabled) {
                ui::badge("Activo", ImVec4(pal.success.x, pal.success.y, pal.success.z, 0.20f), pal.success);
            } else {
                ui::badge("Inactivo", pal.bgSurface3, pal.textMuted);
            }

            if (plugin.manifest.enabled) {
                ImGui::SameLine(0.0f, 8.0f);
                if (ui::subtleButton("Recargar")) {
                    pm.reloadPlugin(plugin.manifest.id);
                    ui::pushToast("Plugin recargado", "info");
                }
            }

            if (!plugin.manifest.author.empty()) {
                ImGui::TextColored(pal.textMuted, "Autor: %s", plugin.manifest.author.c_str());
            }

            if (!plugin.manifest.description.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
                ImGui::TextWrapped("%s", plugin.manifest.description.c_str());
                ImGui::PopStyleColor();
            }

            if (!plugin.manifest.hooks.empty()) {
                ImGui::Spacing();
                for (size_t h = 0; h < plugin.manifest.hooks.size(); ++h) {
                    if (h > 0) ImGui::SameLine(0.0f, 5.0f);
                    ui::badge(plugin.manifest.hooks[h].c_str(), pal.accentSoft, pal.aqua);
                }
            }

            if (plugin.hasError) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, pal.danger);
                ImGui::TextWrapped("Error: %s", plugin.lastError.c_str());
                ImGui::PopStyleColor();
            }
        }
        ui::endCard();

        ImGui::Spacing();
        ImGui::PopID();
    }

    if (plugins.empty()) {
        if (ui::beginCard("##no_plugins", ImVec2(0, 0), false)) {
            ImGui::TextColored(pal.textMuted, "No se encontraron plugins.");
            ImGui::Spacing();
            ImGui::TextColored(pal.textSecondary, "Crea una carpeta en data/plugins/ con:");
            ImGui::TextColored(pal.aqua, "manifest.json + plugin.lua");
        }
        ui::endCard();
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
                ImGui::TextColored(pal.danger, "Error: %s", err ? err : "unknown");
                lua_pop(panel.luaState, 1);
            }
        }

        ImGui::End();
    }

    auto& notifications = pm.getNotifications();
    if (!notifications.empty()) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float yOffset = 40.0f;

        for (int i = static_cast<int>(notifications.size()) - 1; i >= 0; --i) {
            auto& notif = notifications[i];
            notif.timer -= ImGui::GetIO().DeltaTime;
            if (notif.timer <= 0.0f) {
                notifications.erase(notifications.begin() + i);
                continue;
            }

            float alpha = (notif.timer < 1.0f) ? notif.timer : 1.0f;

            ImVec4 accentCol = pal.aqua;
            if (notif.type == "error") accentCol = pal.danger;
            else if (notif.type == "warn") accentCol = pal.warning;
            else if (notif.type == "success") accentCol = pal.success;

            ImVec4 bgColor(pal.bgPopover.x, pal.bgPopover.y, pal.bgPopover.z, 0.94f * alpha);
            ImVec4 textColor(accentCol.x, accentCol.y, accentCol.z, alpha);

            ImGui::SetNextWindowPos(
                ImVec2(vp->WorkPos.x + vp->WorkSize.x - 320.0f, vp->WorkPos.y + yOffset),
                ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Always);

            ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
            ImGui::PushStyleColor(ImGuiCol_Border, pal.borderLine);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

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
