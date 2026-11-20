#pragma once

#include "imgui.h"
#include <string>
#include <vector>

namespace nodepanda {

namespace ui {

struct ToastItem {
    std::string text;
    std::string kind;
    float remainingSeconds = 3.5f;
};

void pushToast(const std::string& text, const std::string& kind = "info", float duration = 3.5f);
void renderToasts(float deltaTime);

bool segmentedControl(const char* id, int* currentIndex, const std::vector<std::string>& labels, float width = 0.0f);
void statusDot(const char* label, ImVec4 color, bool pulsing = false);
void badge(const char* text, ImVec4 bgColor, ImVec4 textColor);
bool beginCard(const char* id, const ImVec2& size = ImVec2(0, 0), bool interactive = false);
void endCard();
void metricCard(const char* id, const char* label, const std::string& value, ImVec4 accentColor, float width = 0.0f);
void progressBarModern(float fraction, ImVec4 fillColor, float width = 0.0f, float height = 6.0f);
void sparkline(const char* id, const std::vector<float>& values, ImVec4 lineColor, ImVec2 size = ImVec2(140.0f, 36.0f));
void sectionHeader(const char* title);
bool primaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool subtleButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool dangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));

}

}
