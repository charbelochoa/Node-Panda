#include "journal_panel.h"
#include "journal_engine.h"
#include "app.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <set>
#include <sstream>
#include <vector>

namespace nodepanda {

namespace {

int daysInMonth(int year, int month) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 30;
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

int firstWeekdayMondayBased(int year, int month) {
    struct tm tmInfo = {};
    tmInfo.tm_year = year - 1900;
    tmInfo.tm_mon = month - 1;
    tmInfo.tm_mday = 1;
    tmInfo.tm_hour = 12;
    std::mktime(&tmInfo);
    int wday = tmInfo.tm_wday;
    return (wday == 0) ? 6 : (wday - 1);
}

const char* monthNameEs(int month) {
    static const char* names[12] = {
        "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
        "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"
    };
    if (month < 1 || month > 12) return "";
    return names[month - 1];
}

std::string buildDateStr(int year, int month, int day) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    return std::string(buf);
}

std::string notePreviewSnippet(const Note& note, int maxLines) {
    std::istringstream iss(note.content);
    std::string line;
    std::string out;
    int count = 0;
    while (std::getline(iss, line) && count < maxLines) {
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        std::string trimmed = line.substr(s);
        if (!trimmed.empty() && trimmed[0] == '#') continue;
        if (!out.empty()) out += "\n";
        out += trimmed;
        ++count;
    }
    return out;
}

}

JournalPanel::JournalPanel() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm tmInfo;
#ifdef _WIN32
    localtime_s(&tmInfo, &tt);
#else
    localtime_r(&tt, &tmInfo);
#endif
    m_viewYear = tmInfo.tm_year + 1900;
    m_viewMonth = tmInfo.tm_mon + 1;
}

void JournalPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();
    JournalEngine& engine = app.getJournalEngine();
    NoteManager& nm = app.getNoteManager();

    ImGui::SetNextWindowSize(ImVec2(820, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Diario")) {
        ImGui::End();
        return;
    }

    int streak = engine.calculateStreak(nm);
    std::vector<JournalEntry> entries = engine.listJournals(nm);
    std::string today = JournalEngine::todayDateString();

    int todayWords = 0;
    for (const auto& e : entries) {
        if (e.date == today) {
            todayWords = e.wordCount;
            break;
        }
    }

    float availW = ImGui::GetContentRegionAvail().x;
    float metricW = (availW - 16.0f) / 3.0f;
    ui::metricCard("##j_streak", "Racha actual", std::to_string(streak) + " d", pal.orange, metricW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##j_total", "Entradas", std::to_string(entries.size()), pal.accent, metricW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##j_words", "Palabras hoy", std::to_string(todayWords), pal.success, metricW);

    ImGui::Spacing();
    if (ui::beginCard("##quick_capture_card", ImVec2(0, 0), false)) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 215.0f);
        bool enter = ImGui::InputTextWithHint("##quick_capture_input", "Captura rapida para hoy...",
            m_captureBuffer, sizeof(m_captureBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine(0.0f, 6.0f);
        if (ui::primaryButton("Capturar", ImVec2(100.0f, 0.0f)) || enter) {
            if (engine.quickCapture(nm, std::string(m_captureBuffer))) {
                m_captureBuffer[0] = '\0';
                app.graphNeedsRebuild = true;
                ui::pushToast("Agregado al diario de hoy", "success");
            }
        }
        ImGui::SameLine(0.0f, 6.0f);
        if (ui::subtleButton("Abrir hoy", ImVec2(95.0f, 0.0f))) {
            std::string id = engine.ensureTodayJournal(nm);
            if (!id.empty()) {
                app.selectedNoteId = id;
                app.graphNeedsRebuild = true;
            }
        }
    }
    ui::endCard();

    ImGui::Spacing();

    if (ImGui::BeginTable("##journal_layout", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Calendario", ImGuiTableColumnFlags_WidthFixed, 300.0f);
        ImGui::TableSetupColumn("Linea temporal", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        renderCalendar(app);

        ImGui::TableSetColumnIndex(1);
        renderTimeline(app);

        ImGui::EndTable();
    }

    ImGui::End();
}

void JournalPanel::renderCalendar(App& app) {
    const ThemePalette& pal = UITheme::palette();
    JournalEngine& engine = app.getJournalEngine();
    NoteManager& nm = app.getNoteManager();

    if (ui::beginCard("##calendar_card", ImVec2(0, 0), false)) {
        if (ImGui::SmallButton("<")) {
            --m_viewMonth;
            if (m_viewMonth < 1) {
                m_viewMonth = 12;
                --m_viewYear;
            }
        }
        ImGui::SameLine();
        std::string header = std::string(monthNameEs(m_viewMonth)) + " " + std::to_string(m_viewYear);
        ImGui::TextColored(pal.textPrimary, "%s", header.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(">")) {
            ++m_viewMonth;
            if (m_viewMonth > 12) {
                m_viewMonth = 1;
                ++m_viewYear;
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Hoy")) {
            auto now = std::chrono::system_clock::now();
            auto tt = std::chrono::system_clock::to_time_t(now);
            struct tm tmInfo;
#ifdef _WIN32
            localtime_s(&tmInfo, &tt);
#else
            localtime_r(&tt, &tmInfo);
#endif
            m_viewYear = tmInfo.tm_year + 1900;
            m_viewMonth = tmInfo.tm_mon + 1;
        }

        ImGui::Spacing();

        std::vector<int> activeVec = engine.activeDaysInMonth(nm, m_viewYear, m_viewMonth);
        std::set<int> activeSet(activeVec.begin(), activeVec.end());
        std::string today = JournalEngine::todayDateString();

        int firstCol = firstWeekdayMondayBased(m_viewYear, m_viewMonth);
        int totalDays = daysInMonth(m_viewYear, m_viewMonth);

        if (ImGui::BeginTable("##cal_grid", 7, ImGuiTableFlags_SizingStretchSame)) {
            static const char* dayHeaders[7] = {"L", "M", "X", "J", "V", "S", "D"};
            for (int c = 0; c < 7; ++c) {
                ImGui::TableSetupColumn(dayHeaders[c]);
            }
            ImGui::TableHeadersRow();

            int dayCounter = 1;
            int totalCells = firstCol + totalDays;
            int rows = (totalCells + 6) / 7;

            for (int r = 0; r < rows; ++r) {
                ImGui::TableNextRow();
                for (int c = 0; c < 7; ++c) {
                    ImGui::TableSetColumnIndex(c);
                    int cellIdx = r * 7 + c;
                    if (cellIdx < firstCol || dayCounter > totalDays) {
                        ImGui::Dummy(ImVec2(28.0f, 26.0f));
                        continue;
                    }

                    int d = dayCounter++;
                    std::string dStr = buildDateStr(m_viewYear, m_viewMonth, d);
                    bool isToday = (dStr == today);
                    bool isActive = (activeSet.find(d) != activeSet.end());
                    bool isSelected = (app.selectedNoteId == dStr);

                    ImVec4 btnBg = pal.bgSurface2;
                    ImVec4 btnText = pal.textSecondary;
                    if (isToday) {
                        btnBg = pal.accent;
                        btnText = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                    } else if (isActive) {
                        btnBg = pal.accentSoft;
                        btnText = pal.accent;
                    }
                    if (isSelected && !isToday) {
                        btnBg = pal.bgSurface3;
                        btnText = pal.textPrimary;
                    }

                    ImGui::PushID(d);
                    ImGui::PushStyleColor(ImGuiCol_Button, btnBg);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accentHover);
                    ImGui::PushStyleColor(ImGuiCol_Text, btnText);
                    char label[16];
                    if (isActive) {
                        std::snprintf(label, sizeof(label), "%d*", d);
                    } else {
                        std::snprintf(label, sizeof(label), "%d", d);
                    }
                    if (ImGui::Button(label, ImVec2(-1.0f, 28.0f))) {
                        bool existed = (nm.getNoteById(dStr) != nullptr);
                        std::string openedId = engine.openOrCreateDate(nm, dStr);
                        if (!openedId.empty()) {
                            app.selectedNoteId = openedId;
                            if (!existed) app.graphNeedsRebuild = true;
                        }
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
    }
    ui::endCard();
}

void JournalPanel::renderTimeline(App& app) {
    const ThemePalette& pal = UITheme::palette();
    JournalEngine& engine = app.getJournalEngine();
    NoteManager& nm = app.getNoteManager();

    ui::sectionHeader("Entradas recientes");

    std::vector<JournalEntry> entries = engine.listJournals(nm);
    if (entries.empty()) {
        ImGui::TextColored(pal.textMuted, "No hay diarios registrados todavia.");
        return;
    }

    float timelineH = ImGui::GetContentRegionAvail().y;
    if (timelineH < 200.0f) timelineH = 200.0f;
    ImGui::BeginChild("##journal_timeline_scroll", ImVec2(0, timelineH), false);
    for (const auto& entry : entries) {
        ImGui::PushID(entry.noteId.c_str());
        if (ui::beginCard("##timeline_entry", ImVec2(0, 0), true)) {
            ImGui::PushStyleColor(ImGuiCol_Text, pal.textPrimary);
            if (ImGui::Selectable(entry.date.c_str(), app.selectedNoteId == entry.noteId)) {
                app.selectedNoteId = entry.noteId;
            }
            ImGui::PopStyleColor();

            std::string blkBadge = std::to_string(entry.blockCount) + " bloques";
            std::string wrdBadge = std::to_string(entry.wordCount) + " palabras";
            std::string tskBadge = std::to_string(entry.taskCount) + " tareas";

            ui::badge(blkBadge.c_str(), pal.bgSurface2, pal.aqua);
            ImGui::SameLine(0.0f, 6.0f);
            ui::badge(wrdBadge.c_str(), pal.bgSurface2, pal.textSecondary);
            if (entry.taskCount > 0) {
                ImGui::SameLine(0.0f, 6.0f);
                ui::badge(tskBadge.c_str(), pal.accentSoft, pal.warning);
            }

            Note* note = nm.getNoteById(entry.noteId);
            if (note) {
                std::string snippet = notePreviewSnippet(*note, 3);
                if (!snippet.empty()) {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
                    ImGui::TextWrapped("%s", snippet.c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
        ui::endCard();
        ImGui::Spacing();
        ImGui::PopID();
    }
    ImGui::EndChild();
}

}
