#include "task_panel.h"
#include "task_engine.h"
#include "app.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace nodepanda {

namespace {

std::string formatDuration(int64_t totalSeconds) {
    int64_t h = totalSeconds / 3600;
    int64_t m = (totalSeconds % 3600) / 60;
    int64_t s = totalSeconds % 60;
    char buf[32];
    if (h > 0) {
        std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld",
            static_cast<long long>(h), static_cast<long long>(m), static_cast<long long>(s));
    } else {
        std::snprintf(buf, sizeof(buf), "%02lld:%02lld",
            static_cast<long long>(m), static_cast<long long>(s));
    }
    return std::string(buf);
}

int64_t liveSeconds(const TaskItem& item) {
    int64_t total = item.trackedSeconds;
    if (item.clocking) {
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > item.clockStartedAt) {
            total += (now - item.clockStartedAt);
        }
    }
    return total;
}

TaskStatus nextStatusCycle(TaskStatus s) {
    switch (s) {
        case TaskStatus::Todo: return TaskStatus::Doing;
        case TaskStatus::Now: return TaskStatus::Doing;
        case TaskStatus::Later: return TaskStatus::Todo;
        case TaskStatus::Doing: return TaskStatus::Done;
        case TaskStatus::Waiting: return TaskStatus::Doing;
        case TaskStatus::Done: return TaskStatus::Todo;
        case TaskStatus::Canceled: return TaskStatus::Todo;
    }
    return TaskStatus::Todo;
}

ImVec4 statusColor(TaskStatus s, const ThemePalette& pal) {
    switch (s) {
        case TaskStatus::Todo: return pal.accent;
        case TaskStatus::Now: return pal.orange;
        case TaskStatus::Later: return pal.textSecondary;
        case TaskStatus::Doing: return pal.warning;
        case TaskStatus::Waiting: return pal.proViolet;
        case TaskStatus::Done: return pal.success;
        case TaskStatus::Canceled: return pal.textMuted;
    }
    return pal.textPrimary;
}

ImVec4 priorityColor(TaskPriority p, const ThemePalette& pal) {
    switch (p) {
        case TaskPriority::Urgent: return pal.danger;
        case TaskPriority::High: return pal.orange;
        case TaskPriority::Medium: return pal.warning;
        case TaskPriority::Low: return pal.aqua;
        case TaskPriority::None: return pal.textMuted;
    }
    return pal.textMuted;
}

}

void TaskPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();
    TaskEngine& engine = app.getTaskEngine();
    NoteManager& nm = app.getNoteManager();

    ImGui::SetNextWindowSize(ImVec2(900, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Tareas")) {
        ImGui::End();
        return;
    }

    if (!m_initialized) {
        engine.rebuildFromNotes(nm);
        m_initialized = true;
    }

    int todoCount = engine.countByStatus(TaskStatus::Todo) + engine.countByStatus(TaskStatus::Later);
    int doingCount = engine.countByStatus(TaskStatus::Doing) + engine.countByStatus(TaskStatus::Now);
    int doneCount = engine.countByStatus(TaskStatus::Done);
    int overdueCount = engine.countOverdue();

    float availW = ImGui::GetContentRegionAvail().x;
    float cardW = (availW - 24.0f) * 0.25f;
    ui::metricCard("##m_todo", "Pendientes", std::to_string(todoCount), pal.accent, cardW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##m_doing", "En curso", std::to_string(doingCount), pal.warning, cardW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##m_done", "Completadas", std::to_string(doneCount), pal.success, cardW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##m_overdue", "Vencidas", std::to_string(overdueCount), pal.danger, cardW);

    ImGui::Spacing();

    std::vector<std::string> viewLabels = {"Lista", "Kanban"};
    ui::segmentedControl("##task_view_mode", &m_viewMode, viewLabels, 200.0f);

    ImGui::SameLine(0.0f, 12.0f);
    static const char* statusFilters[] = {"ALL", "TODO", "DOING", "WAITING", "DONE", "NOW", "LATER", "CANCELED"};
    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("##st_filter", &m_statusFilterIdx, statusFilters, IM_ARRAYSIZE(statusFilters));

    ImGui::SameLine(0.0f, 8.0f);
    static const char* prioFilters[] = {"ALL", "Urgent", "High", "Medium", "Low", "None"};
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("##pr_filter", &m_priorityFilterIdx, prioFilters, IM_ARRAYSIZE(prioFilters));

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##task_search", "Filtrar...", m_searchBuffer, sizeof(m_searchBuffer));

    ImGui::SameLine(0.0f, 8.0f);
    if (ui::subtleButton("Actualizar")) {
        engine.rebuildFromNotes(nm);
    }

    ImGui::Spacing();
    if (ui::beginCard("##quick_create_task", ImVec2(0, 0), false)) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 360.0f);
        bool submitted = ImGui::InputTextWithHint("##new_task_title", "Nueva tarea...",
            m_newTitleBuffer, sizeof(m_newTitleBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine(0.0f, 6.0f);
        static const char* prioChoices[] = {"None", "Low", "Medium", "High", "Urgent"};
        ImGui::SetNextItemWidth(95.0f);
        ImGui::Combo("##new_task_prio", &m_newTaskPriorityIdx, prioChoices, IM_ARRAYSIZE(prioChoices));

        ImGui::SameLine(0.0f, 6.0f);
        ImGui::SetNextItemWidth(115.0f);
        ImGui::InputTextWithHint("##new_task_due", "YYYY-MM-DD", m_newDeadlineBuffer, sizeof(m_newDeadlineBuffer));

        ImGui::SameLine(0.0f, 6.0f);
        if (ui::primaryButton("+ Crear", ImVec2(115.0f, 0.0f)) || submitted) {
            std::string titleStr(m_newTitleBuffer);
            if (!titleStr.empty()) {
                TaskPriority pr = TaskEngine::stringToPriority(prioChoices[m_newTaskPriorityIdx]);
                std::string targetPage = app.selectedNoteId.empty() ? "Tareas" : app.selectedNoteId;
                std::string created = engine.createTask(nm, targetPage, titleStr, TaskStatus::Todo, pr,
                                                        "", std::string(m_newDeadlineBuffer));
                if (!created.empty()) {
                    m_newTitleBuffer[0] = '\0';
                    m_newDeadlineBuffer[0] = '\0';
                    app.graphNeedsRebuild = true;
                    ui::pushToast("Tarea creada", "success");
                }
            }
        }
    }
    ui::endCard();

    ImGui::Spacing();

    if (m_viewMode == 0) {
        renderListView(app);
    } else {
        renderKanbanView(app);
    }

    ImGui::End();
}

void TaskPanel::renderListView(App& app) {
    const ThemePalette& pal = UITheme::palette();
    TaskEngine& engine = app.getTaskEngine();
    NoteManager& nm = app.getNoteManager();

    static const char* statusFilters[] = {"ALL", "TODO", "DOING", "WAITING", "DONE", "NOW", "LATER", "CANCELED"};
    static const char* prioFilters[] = {"ALL", "Urgent", "High", "Medium", "Low", "None"};

    std::vector<TaskItem> items = engine.filterTasks(
        statusFilters[m_statusFilterIdx],
        prioFilters[m_priorityFilterIdx],
        std::string(m_searchBuffer));

    if (items.empty()) {
        ImGui::TextColored(pal.textMuted, "No hay tareas que coincidan.");
        return;
    }

    ImGui::BeginChild("##task_list_scroll", ImVec2(0, 0), false);

    if (ImGui::BeginTable("##tasks_table", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Estado", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Prioridad", ImGuiTableColumnFlags_WidthFixed, 76.0f);
        ImGui::TableSetupColumn("Tarea", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Pagina", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Fechas", ImGuiTableColumnFlags_WidthFixed, 125.0f);
        ImGui::TableSetupColumn("Tiempo", ImGuiTableColumnFlags_WidthFixed, 125.0f);
        ImGui::TableHeadersRow();

        for (const auto& task : items) {
            ImGui::TableNextRow();
            ImGui::PushID(task.uuid.c_str());

            ImGui::TableSetColumnIndex(0);
            ImVec4 stCol = statusColor(task.status, pal);
            ImGui::PushStyleColor(ImGuiCol_Button, pal.bgSurface2);
            ImGui::PushStyleColor(ImGuiCol_Text, stCol);
            if (ImGui::SmallButton(TaskEngine::statusToString(task.status).c_str())) {
                engine.setTaskStatus(nm, task.uuid, nextStatusCycle(task.status));
            }
            ImGui::PopStyleColor(2);

            ImGui::TableSetColumnIndex(1);
            if (task.priority != TaskPriority::None) {
                ui::badge(TaskEngine::priorityToString(task.priority).c_str(),
                          pal.bgSurface2, priorityColor(task.priority, pal));
            } else {
                ImGui::TextColored(pal.textMuted, "-");
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s", task.title.c_str());
            if (!task.tags.empty()) {
                for (size_t i = 0; i < task.tags.size(); ++i) {
                    if (i > 0) ImGui::SameLine(0.0f, 4.0f);
                    std::string tagStr = "#" + task.tags[i];
                    ui::badge(tagStr.c_str(), pal.accentSoft, pal.proViolet);
                }
            }

            ImGui::TableSetColumnIndex(3);
            if (ImGui::Selectable(task.pageId.c_str(), false)) {
                app.selectedNoteId = task.pageId;
            }

            ImGui::TableSetColumnIndex(4);
            if (!task.deadline.empty()) {
                ImGui::TextColored(pal.danger, "D: %s", task.deadline.c_str());
            }
            if (!task.scheduled.empty()) {
                ImGui::TextColored(pal.aqua, "S: %s", task.scheduled.c_str());
            }
            if (task.deadline.empty() && task.scheduled.empty()) {
                ImGui::TextColored(pal.textMuted, "-");
            }

            ImGui::TableSetColumnIndex(5);
            std::string dur = formatDuration(liveSeconds(task));
            ImGui::TextColored(task.clocking ? pal.success : pal.textSecondary, "%s", dur.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(task.clocking ? "Stop" : "Start")) {
                engine.toggleClock(task.uuid);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void TaskPanel::renderKanbanView(App& app) {
    const ThemePalette& pal = UITheme::palette();
    TaskEngine& engine = app.getTaskEngine();
    NoteManager& nm = app.getNoteManager();

    static const char* prioFilters[] = {"ALL", "Urgent", "High", "Medium", "Low", "None"};
    std::vector<TaskItem> items = engine.filterTasks(
        "ALL", prioFilters[m_priorityFilterIdx], std::string(m_searchBuffer));

    struct KanbanCol {
        const char* title;
        TaskStatus status;
    };
    static const KanbanCol cols[4] = {
        {"TODO", TaskStatus::Todo},
        {"DOING", TaskStatus::Doing},
        {"WAITING", TaskStatus::Waiting},
        {"DONE", TaskStatus::Done}
    };

    if (ImGui::BeginTable("##kanban_board", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
        for (int c = 0; c < 4; ++c) {
            ImGui::TableSetupColumn(cols[c].title);
        }

        ImGui::TableNextRow();
        for (int c = 0; c < 4; ++c) {
            ImGui::TableSetColumnIndex(c);

            std::vector<const TaskItem*> colItems;
            for (const auto& t : items) {
                bool belongs = (t.status == cols[c].status);
                if (c == 0 && (t.status == TaskStatus::Later || t.status == TaskStatus::Now)) {
                    belongs = true;
                }
                if (belongs) colItems.push_back(&t);
            }

            ImVec4 colAccent = statusColor(cols[c].status, pal);
            ui::statusDot(cols[c].title, colAccent, c == 1 && !colItems.empty());
            ImGui::SameLine();
            std::string countStr = std::to_string(colItems.size());
            ui::badge(countStr.c_str(), pal.bgSurface2, colAccent);
            ImGui::Separator();

            std::string childId = std::string("##kanban_col_") + cols[c].title;
            float colH = ImGui::GetContentRegionAvail().y;
            if (colH < 200.0f) colH = 200.0f;
            ImGui::BeginChild(childId.c_str(), ImVec2(0, colH), false);

            for (const TaskItem* task : colItems) {
                ImGui::PushID(task->uuid.c_str());
                if (ui::beginCard("##kcard", ImVec2(0, 0), false)) {
                    if (task->priority != TaskPriority::None) {
                        ui::badge(TaskEngine::priorityToString(task->priority).c_str(),
                                  pal.bgSurface3, priorityColor(task->priority, pal));
                        ImGui::SameLine();
                    }
                    ImGui::TextColored(pal.textMuted, "%s", task->pageId.c_str());

                    if (ImGui::Selectable(task->title.c_str(), false)) {
                        app.selectedNoteId = task->pageId;
                    }

                    if (!task->deadline.empty()) {
                        ImGui::TextColored(pal.danger, "Limite: %s", task->deadline.c_str());
                    }

                    std::string dur = formatDuration(liveSeconds(*task));
                    ImGui::TextColored(task->clocking ? pal.success : pal.textMuted, "%s", dur.c_str());

                    ImGui::Spacing();
                    if (c > 0) {
                        if (ImGui::SmallButton("<")) {
                            engine.setTaskStatus(nm, task->uuid, cols[c - 1].status);
                        }
                        ImGui::SameLine(0.0f, 4.0f);
                    }
                    if (c < 3) {
                        if (ImGui::SmallButton(">")) {
                            engine.setTaskStatus(nm, task->uuid, cols[c + 1].status);
                        }
                        ImGui::SameLine(0.0f, 4.0f);
                    }
                    if (ImGui::SmallButton(task->clocking ? "Stop" : "Clock")) {
                        engine.toggleClock(task->uuid);
                    }
                }
                ui::endCard();
                ImGui::Spacing();
                ImGui::PopID();
            }

            ImGui::EndChild();
        }
        ImGui::EndTable();
    }
}

}
