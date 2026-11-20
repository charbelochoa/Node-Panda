#pragma once

#include "imgui.h"
#include <string>

namespace nodepanda {

enum class ThemeMode {
    CarbonDark,
    PaperLight
};

struct ThemePalette {
    ImVec4 bgBase;
    ImVec4 bgSurface1;
    ImVec4 bgSurface2;
    ImVec4 bgSurface3;
    ImVec4 bgPopover;
    ImVec4 bgSidebar;
    ImVec4 border;
    ImVec4 borderLine;
    ImVec4 textPrimary;
    ImVec4 textSecondary;
    ImVec4 textMuted;
    ImVec4 accent;
    ImVec4 accentHover;
    ImVec4 accentSoft;
    ImVec4 success;
    ImVec4 warning;
    ImVec4 danger;
    ImVec4 proViolet;
    ImVec4 aqua;
    ImVec4 orange;
    ImVec4 magenta;
};

class UITheme {
public:
    static void apply(ThemeMode mode);
    static void loadFonts();
    static ThemeMode currentMode();
    static const ThemePalette& palette();
    static ImFont* fontBody();
    static ImFont* fontMono();
    static ImFont* fontHeading();

private:
    static ThemeMode s_mode;
    static ThemePalette s_palette;
    static ImFont* s_bodyFont;
    static ImFont* s_monoFont;
    static ImFont* s_headingFont;
};

}
