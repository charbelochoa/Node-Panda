#pragma once

#include "note_manager.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace nodepanda {

enum class TaskStatus {
    Todo,
    Doing,
    Done,
    Now,
    Later,
    Waiting,
    Canceled
};

enum class TaskPriority {
    None,
    Low,
    Medium,
    High,
    Urgent
};

struct TaskItem {
    std::string uuid;
    std::string pageId;
    std::string title;
    TaskStatus status = TaskStatus::Todo;
    TaskPriority priority = TaskPriority::None;
    std::string scheduled;
    std::string deadline;
    int64_t trackedSeconds = 0;
    bool clocking = false;
    int64_t clockStartedAt = 0;
    std::vector<std::string> tags;
};

class TaskEngine {
public:
    void rebuildFromNotes(const NoteManager& nm);
    const std::vector<TaskItem>& getTasks() const;
    std::vector<TaskItem> filterTasks(const std::string& statusFilter,
                                      const std::string& priorityFilter,
                                      const std::string& query) const;

    std::string createTask(NoteManager& nm, const std::string& pageId, const std::string& title,
                           TaskStatus status = TaskStatus::Todo,
                           TaskPriority priority = TaskPriority::Medium,
                           const std::string& scheduled = "",
                           const std::string& deadline = "");
    bool setTaskStatus(NoteManager& nm, const std::string& uuid, TaskStatus newStatus);
    bool toggleClock(const std::string& uuid);

    int countByStatus(TaskStatus status) const;
    int countOverdue() const;

    static std::string statusToString(TaskStatus s);
    static TaskStatus stringToStatus(const std::string& s);
    static std::string priorityToString(TaskPriority p);
    static TaskPriority stringToPriority(const std::string& s);

private:
    struct ClockState {
        int64_t trackedSeconds = 0;
        bool clocking = false;
        int64_t clockStartedAt = 0;
    };

    struct LineRef {
        std::string pageId;
        int lineIndex = 0;
    };

    static int64_t currentTimestamp();
    static std::string todayDate();

    std::vector<TaskItem> m_tasks;
    std::unordered_map<std::string, LineRef> m_lineRefs;
    std::unordered_map<std::string, ClockState> m_clockStates;
};

}
