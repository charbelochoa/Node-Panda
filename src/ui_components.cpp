#include "ui_components.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>

namespace nodepanda {

namespace ui {

namespace {
std::vector<ToastItem> g_toasts;
}

void pushToast(const std::string& text, const std::string& kind, float duration) {
    ToastItem item;
    item.text = text;
    item.kind = kind;
    item.remainingSeconds = duration;
    g_toasts.push_back(item);
    if (g_toasts.size() > 6) {
        g_toasts.erase(g_toasts.begin());
    }
}

void renderToasts(float deltaTime) {
    if (g_toasts.empty()) return;
    const auto& pal = UITheme::palette();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float pad = 16.0f;
    float yOffset = vp->WorkPos.y + vp->WorkSize.y - pad;

    for (auto it = g_toasts.begin(); it != g_toasts.end();) {
        it->remainingSeconds -= deltaTime;
        if (it->remainingSeconds <= 0.0f) {
            it = g_toasts.erase(it);
        } else {
            ++it;
        }
    }

    int idx = 0;
    for (auto it = g_toasts.rbegin(); it != g_toasts.rend(); ++it, ++idx) {
        ImVec4 dotCol = pal.accent;
        if (it->kind == "success") dotCol = pal.success;
        else if (it->kind == "warn") dotCol = pal.warning;
        else if (it->kind == "error") dotCol = pal.danger;

        float alpha = std::min(1.0f, it->remainingSeconds * 2.0f);
        ImVec2 textSz = ImGui::CalcTextSize(it->text.c_str(), nullptr, false, 320.0f);
        float w = std::max(220.0f, textSz.x + 42.0f);
        float h = textSz.y + 22.0f;
        yOffset -= h;

        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - pad - w, yOffset));
        ImGui::SetNextWindowSize(ImVec2(w, h));
        ImGui::SetNextWindowBgAlpha(0.95f * alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, pal.bgPopover);
        ImGui::PushStyleColor(ImGuiCol_Border, pal.borderLine);

        char winName[32];
        std::snprintf(winName, sizeof(winName), "##toast_%d", idx);
        constexpr ImGuiWindowFlags kFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing;

        if (ImGui::Begin(winName, nullptr, kFlags)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            dl->AddCircleFilled(ImVec2(wp.x + 14.0f, wp.y + h * 0.5f), 4.5f,
                                ImGui::ColorConvertFloat4ToU32(dotCol));
            ImGui::SetCursorPos(ImVec2(26.0f, 10.0f));
            ImGui::TextWrapped("%s", it->text.c_str());
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        yOffset -= 8.0f;
    }
}

bool segmentedControl(const char* id, int* currentIndex, const std::vector<std::string>& labels, float width) {
    if (labels.empty() || !currentIndex) return false;
    const auto& pal = UITheme::palette();
    bool changed = false;

    float totalW = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    float height = 30.0f;
    float itemW = totalW / static_cast<float>(labels.size());

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p, ImVec2(p.x + totalW, p.y + height),
                      ImGui::ColorConvertFloat4ToU32(pal.bgBase), 8.0f);
    dl->AddRect(p, ImVec2(p.x + totalW, p.y + height),
                ImGui::ColorConvertFloat4ToU32(pal.borderLine), 8.0f, 0, 1.0f);

    ImGui::PushID(id);
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        ImVec2 segMin(p.x + i * itemW + 2.0f, p.y + 2.0f);
        ImVec2 segMax(p.x + (i + 1) * itemW - 2.0f, p.y + height - 2.0f);

        ImGui::SetCursorScreenPos(segMin);
        ImGui::InvisibleButton(labels[i].c_str(), ImVec2(segMax.x - segMin.x, segMax.y - segMin.y));
        bool hovered = ImGui::IsItemHovered();
        bool active = (*currentIndex == i);

        if (ImGui::IsItemClicked()) {
            *currentIndex = i;
            changed = true;
            active = true;
        }

        if (active) {
            dl->AddRectFilled(segMin, segMax, ImGui::ColorConvertFloat4ToU32(pal.bgSurface3), 6.0f);
            dl->AddRect(segMin, segMax, ImGui::ColorConvertFloat4ToU32(pal.borderLine), 6.0f, 0, 1.0f);
        } else if (hovered) {
            dl->AddRectFilled(segMin, segMax, ImGui::ColorConvertFloat4ToU32(pal.bgSurface2), 6.0f);
        }

        ImVec2 tsz = ImGui::CalcTextSize(labels[i].c_str());
        ImVec2 tpos(segMin.x + ((segMax.x - segMin.x) - tsz.x) * 0.5f,
                    segMin.y + ((segMax.y - segMin.y) - tsz.y) * 0.5f);
        dl->AddText(tpos, ImGui::ColorConvertFloat4ToU32(active ? pal.textPrimary : pal.textSecondary),
                    labels[i].c_str());
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + height + 4.0f));
    return changed;
}

void statusDot(const char* label, ImVec4 color, bool pulsing) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    float cy = p.y + h * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float alpha = 1.0f;
    if (pulsing) {
        double t = ImGui::GetTime();
        alpha = 0.45f + 0.55f * static_cast<float>(0.5 + 0.5 * std::sin(t * 4.0));
        ImVec4 halo(color.x, color.y, color.z, 0.25f * alpha);
        dl->AddCircleFilled(ImVec2(p.x + 6.0f, cy), 6.5f, ImGui::ColorConvertFloat4ToU32(halo));
    }

    ImVec4 core(color.x, color.y, color.z, alpha);
    dl->AddCircleFilled(ImVec2(p.x + 6.0f, cy), 3.8f, ImGui::ColorConvertFloat4ToU32(core));
    ImGui::Dummy(ImVec2(14.0f, h));
    if (label && label[0] != '\0') {
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextUnformatted(label);
    }
}

void badge(const char* text, ImVec4 bgColor, ImVec4 textColor) {
    ImVec2 tsz = ImGui::CalcTextSize(text);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float padX = 7.0f;
    float padY = 2.0f;
    ImVec2 bMax(p.x + tsz.x + padX * 2.0f, p.y + tsz.y + padY * 2.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, bMax, ImGui::ColorConvertFloat4ToU32(bgColor), 10.0f);
    dl->AddText(ImVec2(p.x + padX, p.y + padY), ImGui::ColorConvertFloat4ToU32(textColor), text);
    ImGui::Dummy(ImVec2(bMax.x - p.x, bMax.y - p.y));
}

bool beginCard(const char* id, const ImVec2& size, bool interactive) {
    const auto& pal = UITheme::palette();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bgSurface2);
    ImGui::PushStyleColor(ImGuiCol_Border, pal.borderLine);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGuiChildFlags flags = ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY;
    if (size.y > 0.0f) flags = ImGuiChildFlags_Border;
    bool open = ImGui::BeginChild(id, size, flags);
    if (interactive && ImGui::IsWindowHovered()) {
        ImVec2 rmin = ImGui::GetWindowPos();
        ImVec2 rmax(rmin.x + ImGui::GetWindowWidth(), rmin.y + ImGui::GetWindowHeight());
        ImGui::GetWindowDrawList()->AddRect(rmin, rmax, ImGui::ColorConvertFloat4ToU32(pal.accent), 8.0f, 0, 1.2f);
    }
    return open;
}

void endCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void metricCard(const char* id, const char* label, const std::string& value, ImVec4 accentColor, float width) {
    const auto& pal = UITheme::palette();
    float w = width > 0.0f ? width : 150.0f;
    if (beginCard(id, ImVec2(w, 64.0f))) {
        ImGui::TextColored(pal.textMuted, "%s", label);
        ImGui::TextColored(accentColor, "%s", value.c_str());
    }
    endCard();
}

void progressBarModern(float fraction, ImVec4 fillColor, float width, float height) {
    const auto& pal = UITheme::palette();
    float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    float clamped = std::max(0.0f, std::min(1.0f, fraction));
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + height),
                      ImGui::ColorConvertFloat4ToU32(pal.bgSurface3), height * 0.5f);
    if (clamped > 0.0f) {
        dl->AddRectFilled(p, ImVec2(p.x + w * clamped, p.y + height),
                          ImGui::ColorConvertFloat4ToU32(fillColor), height * 0.5f);
    }
    ImGui::Dummy(ImVec2(w, height + 2.0f));
}

void sparkline(const char* id, const std::vector<float>& values, ImVec4 lineColor, ImVec2 size) {
    const auto& pal = UITheme::palette();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
                      ImGui::ColorConvertFloat4ToU32(pal.bgBase), 6.0f);
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y),
                ImGui::ColorConvertFloat4ToU32(pal.border), 6.0f);

    if (values.size() >= 2) {
        float minV = *std::min_element(values.begin(), values.end());
        float maxV = *std::max_element(values.begin(), values.end());
        float span = (maxV - minV) > 0.0001f ? (maxV - minV) : 1.0f;
        float pad = 5.0f;
        float stepX = (size.x - pad * 2.0f) / static_cast<float>(values.size() - 1);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(lineColor);
        for (size_t i = 1; i < values.size(); ++i) {
            float y0 = p.y + size.y - pad - ((values[i - 1] - minV) / span) * (size.y - pad * 2.0f);
            float y1 = p.y + size.y - pad - ((values[i] - minV) / span) * (size.y - pad * 2.0f);
            dl->AddLine(ImVec2(p.x + pad + (i - 1) * stepX, y0),
                        ImVec2(p.x + pad + i * stepX, y1), col, 1.8f);
        }
    }
    ImGui::InvisibleButton(id, size);
}

void sectionHeader(const char* title) {
    const auto& pal = UITheme::palette();
    ImGui::TextColored(pal.textPrimary, "%s", title);
    ImGui::Spacing();
}

bool primaryButton(const char* label, const ImVec2& size) {
    const auto& pal = UITheme::palette();
    ImGui::PushStyleColor(ImGuiCol_Button, pal.accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal.accent);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return clicked;
}

bool subtleButton(const char* label, const ImVec2& size) {
    const auto& pal = UITheme::palette();
    ImGui::PushStyleColor(ImGuiCol_Button, pal.bgSurface3);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accentSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal.accent);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

bool dangerButton(const char* label, const ImVec2& size) {
    const auto& pal = UITheme::palette();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pal.danger.x, pal.danger.y, pal.danger.z, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.danger);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal.danger);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

}

}
