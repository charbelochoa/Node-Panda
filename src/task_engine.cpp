#include "task_engine.h"
#include "crypto_engine.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <sstream>

namespace nodepanda {

namespace {

std::string trimTaskStr(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string toUpperTaskStr(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return out;
}

std::string toLowerTaskStr(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool matchStatusToken(const std::string& trimmed, TaskStatus& outStatus, size_t& tokenPos, size_t& tokenLen) {
    std::string work = trimmed;
    size_t offset = 0;
    if (work.size() >= 2 && (work[0] == '-' || work[0] == '*' || work[0] == '+') && work[1] == ' ') {
        offset = 2;
        while (offset < work.size() && (work[offset] == ' ' || work[offset] == '\t')) {
            ++offset;
        }
    }

    static const std::pair<const char*, TaskStatus> keywords[] = {
        {"TODO", TaskStatus::Todo},
        {"DOING", TaskStatus::Doing},
        {"DONE", TaskStatus::Done},
        {"NOW", TaskStatus::Now},
        {"LATER", TaskStatus::Later},
        {"WAITING", TaskStatus::Waiting},
        {"CANCELED", TaskStatus::Canceled}
    };

    for (const auto& [kw, st] : keywords) {
        size_t kwLen = std::char_traits<char>::length(kw);
        if (offset + kwLen <= work.size() && work.compare(offset, kwLen, kw) == 0) {
            if (offset + kwLen == work.size() || std::isspace(static_cast<unsigned char>(work[offset + kwLen]))) {
                outStatus = st;
                tokenPos = offset;
                tokenLen = kwLen;
                return true;
            }
        }
    }

    if (offset + 3 <= work.size() && work[offset] == '[' && work[offset + 2] == ']') {
        char mark = work[offset + 1];
        if (mark == ' ' || mark == 'x' || mark == 'X') {
            outStatus = (mark == ' ') ? TaskStatus::Todo : TaskStatus::Done;
            tokenPos = offset;
            tokenLen = 3;
            return true;
        }
    }

    return false;
}

std::string extractDateMarker(std::string& text, const std::string& marker) {
    size_t pos = text.find(marker);
    if (pos == std::string::npos) return "";

    size_t valStart = pos + marker.size();
    while (valStart < text.size() && text[valStart] == ' ') ++valStart;

    std::string extracted;
    size_t eraseEnd = valStart;

    if (valStart < text.size() && text[valStart] == '<') {
        size_t closePos = text.find('>', valStart + 1);
        if (closePos != std::string::npos) {
            std::string inner = trimTaskStr(text.substr(valStart + 1, closePos - (valStart + 1)));
            if (inner.size() >= 10) {
                extracted = inner.substr(0, 10);
            } else {
                extracted = inner;
            }
            eraseEnd = closePos + 1;
        }
    } else if (valStart + 10 <= text.size()) {
        std::string candidate = text.substr(valStart, 10);
        if (std::isdigit(static_cast<unsigned char>(candidate[0])) &&
            candidate[4] == '-' && candidate[7] == '-') {
            extracted = candidate;
            eraseEnd = valStart + 10;
        }
    }

    if (!extracted.empty()) {
        text.erase(pos, eraseEnd - pos);
    }
    return extracted;
}

TaskPriority extractPriorityMarker(std::string& text) {
    struct Entry {
        const char* token;
        TaskPriority prio;
    };
    static const Entry entries[] = {
        {"[#U]", TaskPriority::Urgent},
        {"[#A]", TaskPriority::High},
        {"[#B]", TaskPriority::Medium},
        {"[#C]", TaskPriority::Low}
    };
    for (const auto& e : entries) {
        size_t pos = text.find(e.token);
        if (pos != std::string::npos) {
            text.erase(pos, 4);
            return e.prio;
        }
    }
    return TaskPriority::None;
}

std::string priorityToMarker(TaskPriority p) {
    switch (p) {
        case TaskPriority::Urgent: return "[#U]";
        case TaskPriority::High: return "[#A]";
        case TaskPriority::Medium: return "[#B]";
        case TaskPriority::Low: return "[#C]";
        case TaskPriority::None: return "";
    }
    return "";
}

std::vector<std::string> extractTaskTags(const std::string& text) {
    std::vector<std::string> tags;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '#') {
            if (i > 0 && (std::isalnum(static_cast<unsigned char>(text[i - 1])) || text[i - 1] == '[')) {
                continue;
            }
            size_t j = i + 1;
            while (j < text.size()) {
                unsigned char ch = static_cast<unsigned char>(text[j]);
                if (std::isalnum(ch) || ch == '_' || ch == '-' || ch == '/') {
                    ++j;
                } else {
                    break;
                }
            }
            if (j > i + 1) {
                std::string tag = text.substr(i + 1, j - (i + 1));
                if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
                    tags.push_back(tag);
                }
                i = j - 1;
            }
        }
    }
    return tags;
}

}

int64_t TaskEngine::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

std::string TaskEngine::todayDate() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm tmInfo;
#ifdef _WIN32
    localtime_s(&tmInfo, &tt);
#else
    localtime_r(&tt, &tmInfo);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmInfo);
    return std::string(buf);
}

void TaskEngine::rebuildFromNotes(const NoteManager& nm) {
    for (const auto& t : m_tasks) {
        ClockState cs;
        cs.trackedSeconds = t.trackedSeconds;
        cs.clocking = t.clocking;
        cs.clockStartedAt = t.clockStartedAt;
        m_clockStates[t.uuid] = cs;
    }

    m_tasks.clear();
    m_lineRefs.clear();

    for (const auto& [noteId, note] : nm.getAllNotes()) {
        std::istringstream iss(note.content);
        std::string line;
        int lineIdx = 0;

        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            int currentIdx = lineIdx++;
            std::string trimmed = trimTaskStr(line);
            if (trimmed.empty()) continue;

            TaskStatus status = TaskStatus::Todo;
            size_t tokPos = 0;
            size_t tokLen = 0;
            if (!matchStatusToken(trimmed, status, tokPos, tokLen)) {
                continue;
            }

            std::string body = trimTaskStr(trimmed.substr(tokPos + tokLen));
            TaskPriority priority = extractPriorityMarker(body);
            std::string scheduled = extractDateMarker(body, "SCHEDULED:");
            std::string deadline = extractDateMarker(body, "DEADLINE:");
            std::vector<std::string> tags = extractTaskTags(body);

            std::string cleanTitle;
            bool lastSpace = false;
            for (char c : trimTaskStr(body)) {
                if (std::isspace(static_cast<unsigned char>(c))) {
                    if (!lastSpace) {
                        cleanTitle.push_back(' ');
                        lastSpace = true;
                    }
                } else {
                    cleanTitle.push_back(c);
                    lastSpace = false;
                }
            }

            std::string seed = noteId + ":" + std::to_string(currentIdx) + ":" + cleanTitle;
            std::string uuid = CryptoEngine::sha256(seed).substr(0, 12);

            TaskItem item;
            item.uuid = uuid;
            item.pageId = noteId;
            item.title = cleanTitle.empty() ? "(Sin titulo)" : cleanTitle;
            item.status = status;
            item.priority = priority;
            item.scheduled = scheduled;
            item.deadline = deadline;
            item.tags = std::move(tags);

            auto csIt = m_clockStates.find(uuid);
            if (csIt != m_clockStates.end()) {
                item.trackedSeconds = csIt->second.trackedSeconds;
                item.clocking = csIt->second.clocking;
                item.clockStartedAt = csIt->second.clockStartedAt;
            }

            m_lineRefs[uuid] = LineRef{noteId, currentIdx};
            m_tasks.push_back(std::move(item));
        }
    }

    std::sort(m_tasks.begin(), m_tasks.end(), [](const TaskItem& a, const TaskItem& b) {
        if (a.status != b.status) return static_cast<int>(a.status) < static_cast<int>(b.status);
        if (a.priority != b.priority) return static_cast<int>(a.priority) > static_cast<int>(b.priority);
        if (a.deadline != b.deadline) {
            if (a.deadline.empty()) return false;
            if (b.deadline.empty()) return true;
            return a.deadline < b.deadline;
        }
        return a.title < b.title;
    });
}

const std::vector<TaskItem>& TaskEngine::getTasks() const {
    return m_tasks;
}

std::vector<TaskItem> TaskEngine::filterTasks(const std::string& statusFilter,
                                              const std::string& priorityFilter,
                                              const std::string& query) const {
    std::vector<TaskItem> out;
    std::string stUp = toUpperTaskStr(trimTaskStr(statusFilter));
    std::string prUp = toUpperTaskStr(trimTaskStr(priorityFilter));
    std::string qLow = toLowerTaskStr(trimTaskStr(query));

    bool filterStatus = !stUp.empty() && stUp != "ALL" && stUp != "TODAS";
    bool filterPrio = !prUp.empty() && prUp != "ALL" && prUp != "TODAS";

    for (const auto& t : m_tasks) {
        if (filterStatus && toUpperTaskStr(statusToString(t.status)) != stUp) {
            continue;
        }
        if (filterPrio && toUpperTaskStr(priorityToString(t.priority)) != prUp) {
            continue;
        }
        if (!qLow.empty()) {
            bool found = toLowerTaskStr(t.title).find(qLow) != std::string::npos ||
                         toLowerTaskStr(t.pageId).find(qLow) != std::string::npos;
            if (!found) {
                for (const auto& tag : t.tags) {
                    if (toLowerTaskStr(tag).find(qLow) != std::string::npos) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found) continue;
        }
        out.push_back(t);
    }
    return out;
}

std::string TaskEngine::createTask(NoteManager& nm, const std::string& pageId, const std::string& title,
                                   TaskStatus status, TaskPriority priority,
                                   const std::string& scheduled, const std::string& deadline) {
    std::string cleanTitle = trimTaskStr(title);
    if (cleanTitle.empty()) return "";

    std::string targetPage = pageId.empty() ? "Tareas" : pageId;
    Note* note = nm.getNoteById(targetPage);
    if (!note) {
        note = nm.createNote(targetPage);
        if (!note) return "";
    }

    std::ostringstream line;
    line << "- " << statusToString(status);
    std::string pMarker = priorityToMarker(priority);
    if (!pMarker.empty()) {
        line << " " << pMarker;
    }
    line << " " << cleanTitle;
    if (!scheduled.empty()) {
        line << " SCHEDULED: <" << scheduled << ">";
    }
    if (!deadline.empty()) {
        line << " DEADLINE: <" << deadline << ">";
    }

    if (!note->content.empty() && note->content.back() != '\n') {
        note->content.push_back('\n');
    }
    note->content += line.str() + "\n";
    note->parseLinks();
    note->dirty = true;
    note->saveToFile();

    rebuildFromNotes(nm);

    for (auto it = m_tasks.rbegin(); it != m_tasks.rend(); ++it) {
        if (it->pageId == targetPage && it->title == cleanTitle && it->status == status) {
            return it->uuid;
        }
    }
    return m_tasks.empty() ? "" : m_tasks.back().uuid;
}

bool TaskEngine::setTaskStatus(NoteManager& nm, const std::string& uuid, TaskStatus newStatus) {
    auto refIt = m_lineRefs.find(uuid);
    if (refIt == m_lineRefs.end()) return false;

    Note* note = nm.getNoteById(refIt->second.pageId);
    if (!note) return false;

    std::vector<std::string> lines;
    std::istringstream iss(note->content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    int idx = refIt->second.lineIndex;
    if (idx < 0 || idx >= static_cast<int>(lines.size())) return false;

    std::string& targetLine = lines[static_cast<size_t>(idx)];
    size_t leadSpaces = 0;
    while (leadSpaces < targetLine.size() && (targetLine[leadSpaces] == ' ' || targetLine[leadSpaces] == '\t')) {
        ++leadSpaces;
    }

    std::string trimmed = targetLine.substr(leadSpaces);
    TaskStatus oldStatus = TaskStatus::Todo;
    size_t tokPos = 0;
    size_t tokLen = 0;
    if (!matchStatusToken(trimmed, oldStatus, tokPos, tokLen)) {
        return false;
    }

    std::string replacement = statusToString(newStatus);
    targetLine.replace(leadSpaces + tokPos, tokLen, replacement);

    std::ostringstream out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out << lines[i] << "\n";
    }

    note->content = out.str();
    note->dirty = true;
    note->saveToFile();

    for (auto& t : m_tasks) {
        if (t.uuid == uuid) {
            t.status = newStatus;
            if (newStatus == TaskStatus::Done && t.clocking) {
                int64_t now = currentTimestamp();
                if (now > t.clockStartedAt) {
                    t.trackedSeconds += (now - t.clockStartedAt);
                }
                t.clocking = false;
                t.clockStartedAt = 0;
                m_clockStates[uuid] = ClockState{t.trackedSeconds, false, 0};
            }
            break;
        }
    }

    return true;
}

bool TaskEngine::toggleClock(const std::string& uuid) {
    for (auto& t : m_tasks) {
        if (t.uuid == uuid) {
            int64_t now = currentTimestamp();
            if (t.clocking) {
                if (now > t.clockStartedAt) {
                    t.trackedSeconds += (now - t.clockStartedAt);
                }
                t.clocking = false;
                t.clockStartedAt = 0;
            } else {
                t.clocking = true;
                t.clockStartedAt = now;
            }
            m_clockStates[uuid] = ClockState{t.trackedSeconds, t.clocking, t.clockStartedAt};
            return true;
        }
    }
    return false;
}

int TaskEngine::countByStatus(TaskStatus status) const {
    int count = 0;
    for (const auto& t : m_tasks) {
        if (t.status == status) ++count;
    }
    return count;
}

int TaskEngine::countOverdue() const {
    std::string today = todayDate();
    int count = 0;
    for (const auto& t : m_tasks) {
        if (t.status == TaskStatus::Done || t.status == TaskStatus::Canceled) continue;
        if (!t.deadline.empty() && t.deadline < today) {
            ++count;
        }
    }
    return count;
}

std::string TaskEngine::statusToString(TaskStatus s) {
    switch (s) {
        case TaskStatus::Todo: return "TODO";
        case TaskStatus::Doing: return "DOING";
        case TaskStatus::Done: return "DONE";
        case TaskStatus::Now: return "NOW";
        case TaskStatus::Later: return "LATER";
        case TaskStatus::Waiting: return "WAITING";
        case TaskStatus::Canceled: return "CANCELED";
    }
    return "TODO";
}

TaskStatus TaskEngine::stringToStatus(const std::string& s) {
    std::string up = toUpperTaskStr(trimTaskStr(s));
    if (up == "DOING") return TaskStatus::Doing;
    if (up == "DONE") return TaskStatus::Done;
    if (up == "NOW") return TaskStatus::Now;
    if (up == "LATER") return TaskStatus::Later;
    if (up == "WAITING") return TaskStatus::Waiting;
    if (up == "CANCELED" || up == "CANCELLED") return TaskStatus::Canceled;
    return TaskStatus::Todo;
}

std::string TaskEngine::priorityToString(TaskPriority p) {
    switch (p) {
        case TaskPriority::None: return "None";
        case TaskPriority::Low: return "Low";
        case TaskPriority::Medium: return "Medium";
        case TaskPriority::High: return "High";
        case TaskPriority::Urgent: return "Urgent";
    }
    return "None";
}

TaskPriority TaskEngine::stringToPriority(const std::string& s) {
    std::string up = toUpperTaskStr(trimTaskStr(s));
    if (up == "URGENT" || up == "U") return TaskPriority::Urgent;
    if (up == "HIGH" || up == "A") return TaskPriority::High;
    if (up == "MEDIUM" || up == "B") return TaskPriority::Medium;
    if (up == "LOW" || up == "C") return TaskPriority::Low;
    return TaskPriority::None;
}

}
