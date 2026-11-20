#include "command_palette.h"
#include "app.h"
#include "asset_manager.h"
#include "backup_manager.h"
#include "datalog_engine.h"
#include "note_manager.h"
#include "sync_engine.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <string>

namespace nodepanda {

namespace {

std::string toLowerAscii(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string timestampNoteName() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmInfo{};
#ifdef _WIN32
    localtime_s(&tmInfo, &t);
#else
    localtime_r(&t, &tmInfo);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "note-%Y%m%d-%H%M%S", &tmInfo);
    return std::string(buf);
}

ImVec4 categoryColor(const std::string& category, const ThemePalette& pal) {
    if (category == "Note") return pal.aqua;
    if (category == "Navigation") return pal.accent;
    if (category == "Backup") return pal.success;
    if (category == "Sync") return pal.proViolet;
    if (category == "Assets") return pal.orange;
    if (category == "Datalog") return pal.magenta;
    if (category == "Theme") return pal.warning;
    return pal.textSecondary;
}

}

CommandPalette::CommandPalette() {
    registerDefaultCommands();
}

void CommandPalette::open() {
    m_open = true;
    m_focusInput = true;
    m_selectedIndex = 0;
    m_searchBuf[0] = '\0';
}

void CommandPalette::close() {
    m_open = false;
    m_focusInput = false;
}

void CommandPalette::registerCommand(const PaletteCommand& cmd) {
    for (auto& existing : m_commands) {
        if (existing.id == cmd.id) {
            existing = cmd;
            return;
        }
    }
    m_commands.push_back(cmd);
}

void CommandPalette::registerDefaultCommands() {
    registerCommand({
        "file.new-note",
        "Create New Note",
        "File",
        "Ctrl+N",
        [](App& app) {
            std::string name = timestampNoteName();
            Note* created = app.getNoteManager().createNote(name);
            if (created) {
                app.selectedNoteId = created->id;
                app.currentState = AppState::ENTORNO_NOTAS;
                app.graphNeedsRebuild = true;
                ui::pushToast("Created " + created->id, "success");
            }
        }
    });

    registerCommand({
        "file.save-all",
        "Save All Dirty Notes",
        "File",
        "Ctrl+S",
        [](App& app) {
            app.getNoteManager().saveAllDirty();
            ui::pushToast("All notes saved", "success");
        }
    });

    registerCommand({
        "file.rescan",
        "Rescan Notes Directory",
        "File",
        "",
        [](App& app) {
            app.getNoteManager().scanDirectory();
            app.getNoteManager().rebuildAllLinks();
            app.graphNeedsRebuild = true;
            ui::pushToast("Notes rescanned", "info");
        }
    });

    registerCommand({
        "nav.workspace",
        "Switch to Notes Workspace",
        "Navigation",
        "",
        [](App& app) {
            app.currentState = AppState::ENTORNO_NOTAS;
        }
    });

    registerCommand({
        "nav.hub",
        "Switch to Welcome Hub",
        "Navigation",
        "",
        [](App& app) {
            app.currentState = AppState::HUB_BIENVENIDA;
        }
    });

    registerCommand({
        "graph.rebuild",
        "Rebuild Knowledge Graph",
        "Navigation",
        "",
        [](App& app) {
            app.getNoteManager().rebuildAllLinks();
            app.graphNeedsRebuild = true;
            ui::pushToast("Graph rebuild scheduled", "info");
        }
    });

    registerCommand({
        "datalog.rebuild",
        "Rebuild Datalog Fact Index",
        "Datalog",
        "",
        [](App& app) {
            app.getDatalogEngine().rebuildIndex(app.getNoteManager());
            ui::pushToast("Datalog index rebuilt", "success");
        }
    });

    registerCommand({
        "backup.create",
        "Create Manual Backup",
        "Backup",
        "",
        [](App& app) {
            auto info = app.getBackupManager().createBackup(app.getNoteManager(), "palette", "manual");
            ui::pushToast("Backup created: " + info.name, "success");
        }
    });

    registerCommand({
        "assets.verify",
        "Verify All Assets Integrity",
        "Assets",
        "",
        [](App& app) {
            bool ok = app.getAssetManager().verifyAll();
            ui::pushToast(ok ? "All assets verified" : "Asset mismatch found", ok ? "success" : "error");
        }
    });

    registerCommand({
        "sync.toggle",
        "Toggle Sync Engine",
        "Sync",
        "",
        [](App& app) {
            auto diag = app.getSyncEngine().diagnostics(app.getNoteManager());
            if (diag.running) {
                app.getSyncEngine().stopSync();
                ui::pushToast("Sync stopped", "info");
            } else {
                app.getSyncEngine().startSync();
                ui::pushToast("Sync started", "success");
            }
        }
    });

    registerCommand({
        "theme.dark",
        "Apply Carbon Dark Theme",
        "Theme",
        "",
        [](App&) {
            UITheme::apply(ThemeMode::CarbonDark);
            ui::pushToast("Carbon Dark theme active", "info");
        }
    });

    registerCommand({
        "theme.light",
        "Apply Paper Light Theme",
        "Theme",
        "",
        [](App&) {
            UITheme::apply(ThemeMode::PaperLight);
            ui::pushToast("Paper Light theme active", "info");
        }
    });
}

int CommandPalette::fuzzyScore(const std::string& query, const std::string& candidate) {
    if (query.empty()) return 1;
    std::string q = toLowerAscii(query);
    std::string c = toLowerAscii(candidate);

    if (c == q) return 1000;
    if (c.rfind(q, 0) == 0) return 800 - static_cast<int>(c.size());

    size_t subPos = c.find(q);
    if (subPos != std::string::npos) {
        int bonus = (subPos == 0 || c[subPos - 1] == ' ' || c[subPos - 1] == '-' || c[subPos - 1] == '/' || c[subPos - 1] == '_') ? 120 : 0;
        return 500 + bonus - static_cast<int>(subPos);
    }

    size_t qi = 0;
    int score = 0;
    int consecutive = 0;
    for (size_t ci = 0; ci < c.size() && qi < q.size(); ++ci) {
        if (c[ci] == q[qi]) {
            score += 15 + consecutive * 10;
            if (ci == 0 || c[ci - 1] == ' ' || c[ci - 1] == '-' || c[ci - 1] == '/' || c[ci - 1] == '_') {
                score += 20;
            }
            ++consecutive;
            ++qi;
        } else {
            consecutive = 0;
        }
    }
    if (qi < q.size()) return 0;
    return score > 0 ? score : 1;
}

void CommandPalette::render(App& app) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        if (m_open) close();
        else open();
    }

    if (!m_open) return;

    const auto& pal = UITheme::palette();
    ImGuiViewport* vp = ImGui::GetMainViewport();

    ImVec2 paletteSize(620.0f, 420.0f);
    ImVec2 palettePos(
        vp->Pos.x + (vp->Size.x - paletteSize.x) * 0.5f,
        vp->Pos.y + vp->Size.y * 0.14f
    );

    ImGui::GetBackgroundDrawList(vp)->AddRectFilled(
        vp->Pos,
        ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
        IM_COL32(0, 0, 0, 110)
    );

    std::string query(m_searchBuf);
    std::vector<MatchEntry> matches;
    matches.reserve(m_commands.size() + app.getNoteManager().getAllNotes().size());

    for (size_t i = 0; i < m_commands.size(); ++i) {
        const auto& cmd = m_commands[i];
        int s1 = fuzzyScore(query, cmd.title);
        int s2 = fuzzyScore(query, cmd.category + " " + cmd.title);
        int s3 = fuzzyScore(query, cmd.id);
        int best = std::max({s1, s2, s3});
        if (best > 0) {
            matches.push_back({cmd.title, cmd.category, cmd.shortcut, "", static_cast<int>(i), best + 25});
        }
    }

    for (const auto& [id, note] : app.getNoteManager().getAllNotes()) {
        std::string displayTitle = note.title.empty() ? id : note.title;
        int s1 = fuzzyScore(query, displayTitle);
        int s2 = fuzzyScore(query, id);
        int best = std::max(s1, s2);
        if (best > 0) {
            matches.push_back({displayTitle, "Note", "", id, -1, best});
        }
    }

    std::sort(matches.begin(), matches.end(), [](const MatchEntry& a, const MatchEntry& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.title < b.title;
    });

    if (matches.empty()) {
        m_selectedIndex = 0;
    } else if (m_selectedIndex >= static_cast<int>(matches.size())) {
        m_selectedIndex = static_cast<int>(matches.size()) - 1;
    } else if (m_selectedIndex < 0) {
        m_selectedIndex = 0;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        close();
        return;
    }
    if (!matches.empty()) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
            m_selectedIndex = (m_selectedIndex + 1) % static_cast<int>(matches.size());
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
            m_selectedIndex = (m_selectedIndex - 1 + static_cast<int>(matches.size())) % static_cast<int>(matches.size());
        }
    }

    int executeIndex = -1;
    if (!matches.empty() &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
        executeIndex = m_selectedIndex;
    }

    ImGui::SetNextWindowPos(palettePos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(paletteSize, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, pal.bgPopover);
    ImGui::PushStyleColor(ImGuiCol_Border, pal.border);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("##CommandPaletteOverlay", nullptr, flags)) {
        if (m_focusInput) {
            ImGui::SetKeyboardFocusHere();
            m_focusInput = false;
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##palette_input", "Type a command or search notes...", m_searchBuf, sizeof(m_searchBuf))) {
            m_selectedIndex = 0;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild("##palette_results", ImVec2(0.0f, -26.0f), false);
        if (matches.empty()) {
            ImGui::TextDisabled("No matching commands or notes");
        } else {
            for (int i = 0; i < static_cast<int>(matches.size()); ++i) {
                const auto& m = matches[i];
                bool isSelected = (i == m_selectedIndex);

                ImGui::PushID(i);
                ImVec4 rowBg = isSelected ? pal.accentSoft : ImVec4(0, 0, 0, 0);
                ImGui::PushStyleColor(ImGuiCol_Header, rowBg);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, pal.bgSurface2);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, pal.accentSoft);

                float rowH = 30.0f;
                ImVec2 rowStart = ImGui::GetCursorPos();
                if (ImGui::Selectable("##row", isSelected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, rowH))) {
                    executeIndex = i;
                }
                if (isSelected) {
                    ImGui::SetScrollHereY(0.5f);
                }

                ImGui::SetCursorPos(ImVec2(rowStart.x + 8.0f, rowStart.y + 5.0f));
                ImVec4 catCol = categoryColor(m.category, pal);
                ui::badge(m.category.c_str(), ImVec4(catCol.x, catCol.y, catCol.z, 0.22f), catCol);
                ImGui::SameLine(0.0f, 10.0f);
                ImGui::TextColored(pal.textPrimary, "%s", m.title.c_str());

                if (!m.shortcut.empty()) {
                    float textW = ImGui::CalcTextSize(m.shortcut.c_str()).x;
                    float rightX = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - textW - 10.0f;
                    if (rightX > ImGui::GetCursorPosX() + 10.0f) {
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(rightX);
                        ImGui::TextColored(pal.textMuted, "%s", m.shortcut.c_str());
                    }
                }

                ImGui::PopStyleColor(3);
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextColored(pal.textMuted, "Up/Down Navigate   Enter Run   Esc Close   Ctrl+K Toggle");

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
            close();
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (executeIndex >= 0 && executeIndex < static_cast<int>(matches.size())) {
        const auto& chosen = matches[executeIndex];
        close();
        if (chosen.commandIndex >= 0 && chosen.commandIndex < static_cast<int>(m_commands.size())) {
            if (m_commands[chosen.commandIndex].action) {
                m_commands[chosen.commandIndex].action(app);
            }
        } else if (!chosen.noteId.empty()) {
            app.selectedNoteId = chosen.noteId;
            app.currentState = AppState::ENTORNO_NOTAS;
        }
    }
}

}
