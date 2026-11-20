#include "ui_theme.h"

#include <filesystem>
#include <vector>

namespace nodepanda {

ThemeMode UITheme::s_mode = ThemeMode::CarbonDark;
ThemePalette UITheme::s_palette{};
ImFont* UITheme::s_bodyFont = nullptr;
ImFont* UITheme::s_monoFont = nullptr;
ImFont* UITheme::s_headingFont = nullptr;

static ImVec4 hexRgba(unsigned int hex, float alpha = 1.0f) {
    float r = static_cast<float>((hex >> 16) & 0xff) / 255.0f;
    float g = static_cast<float>((hex >> 8) & 0xff) / 255.0f;
    float b = static_cast<float>(hex & 0xff) / 255.0f;
    return ImVec4(r, g, b, alpha);
}

void UITheme::loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    std::vector<std::string> bodyCandidates = {
        "data/fonts/inter.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };
    std::vector<std::string> monoCandidates = {
        "data/fonts/jetbrains.ttf",
        "C:/Windows/Fonts/cascadiacode.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
    };

    ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;

    std::string chosenBody;
    for (const auto& p : bodyCandidates) {
        if (std::filesystem::exists(p)) {
            chosenBody = p;
            break;
        }
    }
    if (!chosenBody.empty()) {
        s_bodyFont = io.Fonts->AddFontFromFileTTF(chosenBody.c_str(), 16.0f, &cfg);
        s_headingFont = io.Fonts->AddFontFromFileTTF(chosenBody.c_str(), 21.0f, &cfg);
    }
    if (!s_bodyFont) {
        s_bodyFont = io.Fonts->AddFontDefault();
        s_headingFont = s_bodyFont;
    }

    std::string chosenMono;
    for (const auto& p : monoCandidates) {
        if (std::filesystem::exists(p)) {
            chosenMono = p;
            break;
        }
    }
    if (!chosenMono.empty()) {
        s_monoFont = io.Fonts->AddFontFromFileTTF(chosenMono.c_str(), 15.0f, &cfg);
    }
    if (!s_monoFont) {
        s_monoFont = s_bodyFont;
    }
}

void UITheme::apply(ThemeMode mode) {
    s_mode = mode;

    if (mode == ThemeMode::CarbonDark) {
        s_palette.bgBase        = hexRgba(0x151515);
        s_palette.bgSurface1    = hexRgba(0x191919);
        s_palette.bgSurface2    = hexRgba(0x1f1f1f);
        s_palette.bgSurface3    = hexRgba(0x282828);
        s_palette.bgPopover     = hexRgba(0x303030, 0.98f);
        s_palette.bgSidebar     = hexRgba(0x141414);
        s_palette.border        = hexRgba(0xffffff, 0.08f);
        s_palette.borderLine    = hexRgba(0xffffff, 0.14f);
        s_palette.textPrimary   = hexRgba(0xf2f2f2);
        s_palette.textSecondary = hexRgba(0xb3b3b3);
        s_palette.textMuted     = hexRgba(0x7d7d7d);
        s_palette.accent        = hexRgba(0x257de0);
        s_palette.accentHover   = hexRgba(0x338ced);
        s_palette.accentSoft    = hexRgba(0x257de0, 0.18f);
        s_palette.success       = hexRgba(0x10a122);
        s_palette.warning       = hexRgba(0xfbb315);
        s_palette.danger        = hexRgba(0xdc3c3d);
        s_palette.proViolet     = hexRgba(0x785ee8);
        s_palette.aqua          = hexRgba(0x019c7f);
        s_palette.orange        = hexRgba(0xd25f25);
        s_palette.magenta       = hexRgba(0xce447d);
    } else {
        s_palette.bgBase        = hexRgba(0xf5f5f4);
        s_palette.bgSurface1    = hexRgba(0xffffff);
        s_palette.bgSurface2    = hexRgba(0xfafafa);
        s_palette.bgSurface3    = hexRgba(0xefefef);
        s_palette.bgPopover     = hexRgba(0xffffff, 0.98f);
        s_palette.bgSidebar     = hexRgba(0xf2f2f0);
        s_palette.border        = hexRgba(0x000000, 0.09f);
        s_palette.borderLine    = hexRgba(0x000000, 0.14f);
        s_palette.textPrimary   = hexRgba(0x171717);
        s_palette.textSecondary = hexRgba(0x525252);
        s_palette.textMuted     = hexRgba(0x787878);
        s_palette.accent        = hexRgba(0x257de0);
        s_palette.accentHover   = hexRgba(0x1b64be);
        s_palette.accentSoft    = hexRgba(0x257de0, 0.14f);
        s_palette.success       = hexRgba(0x008a17);
        s_palette.warning       = hexRgba(0xd28e05);
        s_palette.danger        = hexRgba(0xc83839);
        s_palette.proViolet     = hexRgba(0x6a4ce2);
        s_palette.aqua          = hexRgba(0x008670);
        s_palette.orange        = hexRgba(0xbd521b);
        s_palette.magenta       = hexRgba(0xba396f);
    }

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding       = 0.0f;
    s.ChildRounding        = 8.0f;
    s.FrameRounding        = 6.0f;
    s.PopupRounding        = 8.0f;
    s.ScrollbarRounding    = 8.0f;
    s.GrabRounding         = 6.0f;
    s.TabRounding          = 6.0f;
    s.WindowBorderSize     = 0.0f;
    s.ChildBorderSize      = 1.0f;
    s.PopupBorderSize      = 1.0f;
    s.FrameBorderSize      = 1.0f;
    s.TabBorderSize        = 0.0f;
    s.WindowPadding        = ImVec2(14.0f, 12.0f);
    s.FramePadding         = ImVec2(10.0f, 6.0f);
    s.ItemSpacing          = ImVec2(8.0f, 7.0f);
    s.ItemInnerSpacing     = ImVec2(6.0f, 4.0f);
    s.ScrollbarSize        = 10.0f;
    s.GrabMinSize          = 10.0f;
    s.IndentSpacing        = 18.0f;
    s.DockingSeparatorSize = 2.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = s_palette.textPrimary;
    c[ImGuiCol_TextDisabled]          = s_palette.textMuted;
    c[ImGuiCol_WindowBg]              = s_palette.bgSurface1;
    c[ImGuiCol_ChildBg]               = s_palette.bgSurface2;
    c[ImGuiCol_PopupBg]               = s_palette.bgPopover;
    c[ImGuiCol_Border]                = s_palette.border;
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = s_palette.bgSurface2;
    c[ImGuiCol_FrameBgHovered]        = s_palette.bgSurface3;
    c[ImGuiCol_FrameBgActive]         = s_palette.bgSurface3;
    c[ImGuiCol_TitleBg]               = s_palette.bgBase;
    c[ImGuiCol_TitleBgActive]         = s_palette.bgSurface1;
    c[ImGuiCol_TitleBgCollapsed]      = s_palette.bgBase;
    c[ImGuiCol_MenuBarBg]             = s_palette.bgBase;
    c[ImGuiCol_ScrollbarBg]           = s_palette.bgBase;
    c[ImGuiCol_ScrollbarGrab]         = s_palette.bgSurface3;
    c[ImGuiCol_ScrollbarGrabHovered]  = s_palette.textMuted;
    c[ImGuiCol_ScrollbarGrabActive]   = s_palette.textSecondary;
    c[ImGuiCol_CheckMark]             = s_palette.accent;
    c[ImGuiCol_SliderGrab]            = s_palette.accent;
    c[ImGuiCol_SliderGrabActive]      = s_palette.accentHover;
    c[ImGuiCol_Button]                = s_palette.bgSurface3;
    c[ImGuiCol_ButtonHovered]         = s_palette.accentSoft;
    c[ImGuiCol_ButtonActive]          = s_palette.accent;
    c[ImGuiCol_Header]                = s_palette.bgSurface3;
    c[ImGuiCol_HeaderHovered]         = s_palette.accentSoft;
    c[ImGuiCol_HeaderActive]          = s_palette.accent;
    c[ImGuiCol_Separator]             = s_palette.borderLine;
    c[ImGuiCol_SeparatorHovered]      = s_palette.accent;
    c[ImGuiCol_SeparatorActive]       = s_palette.accentHover;
    c[ImGuiCol_ResizeGrip]            = s_palette.borderLine;
    c[ImGuiCol_ResizeGripHovered]     = s_palette.accent;
    c[ImGuiCol_ResizeGripActive]      = s_palette.accentHover;
    c[ImGuiCol_Tab]                   = s_palette.bgBase;
    c[ImGuiCol_TabHovered]            = s_palette.bgSurface3;
    c[ImGuiCol_TabActive]             = s_palette.bgSurface2;
    c[ImGuiCol_TabUnfocused]          = s_palette.bgBase;
    c[ImGuiCol_TabUnfocusedActive]    = s_palette.bgSurface2;
    c[ImGuiCol_DockingPreview]        = ImVec4(s_palette.accent.x, s_palette.accent.y, s_palette.accent.z, 0.55f);
    c[ImGuiCol_DockingEmptyBg]        = s_palette.bgBase;
    c[ImGuiCol_PlotLines]             = s_palette.accent;
    c[ImGuiCol_PlotLinesHovered]      = s_palette.aqua;
    c[ImGuiCol_PlotHistogram]         = s_palette.accent;
    c[ImGuiCol_PlotHistogramHovered]  = s_palette.accentHover;
    c[ImGuiCol_TableHeaderBg]         = s_palette.bgSurface2;
    c[ImGuiCol_TableBorderStrong]     = s_palette.borderLine;
    c[ImGuiCol_TableBorderLight]      = s_palette.border;
    c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, mode == ThemeMode::CarbonDark ? 0.025f : 0.03f);
    c[ImGuiCol_TextSelectedBg]        = s_palette.accentSoft;
    c[ImGuiCol_DragDropTarget]        = s_palette.accent;
    c[ImGuiCol_NavHighlight]          = s_palette.accent;
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.58f);
}

ThemeMode UITheme::currentMode() { return s_mode; }
const ThemePalette& UITheme::palette() { return s_palette; }
ImFont* UITheme::fontBody() { return s_bodyFont; }
ImFont* UITheme::fontMono() { return s_monoFont; }
ImFont* UITheme::fontHeading() { return s_headingFont; }

}
