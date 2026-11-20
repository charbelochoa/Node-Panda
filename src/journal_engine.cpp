#include "journal_engine.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <set>
#include <sstream>

namespace nodepanda {

namespace {

std::string trimJournalStr(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool lineContainsTask(const std::string& trimmed) {
    std::string s = trimmed;
    if (s.size() >= 2 && (s[0] == '-' || s[0] == '*' || s[0] == '+') && s[1] == ' ') {
        s = trimJournalStr(s.substr(2));
    }
    static const char* tokens[] = {"TODO", "DOING", "DONE", "NOW", "LATER", "WAITING", "CANCELED"};
    for (const char* tok : tokens) {
        size_t len = std::char_traits<char>::length(tok);
        if (s.size() >= len && s.compare(0, len, tok) == 0 &&
            (s.size() == len || std::isspace(static_cast<unsigned char>(s[len])))) {
            return true;
        }
    }
    if (s.size() >= 3 && s[0] == '[' && s[2] == ']' && (s[1] == ' ' || s[1] == 'x' || s[1] == 'X')) {
        return true;
    }
    return false;
}

std::string currentTimeHHMM() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm tmInfo;
#ifdef _WIN32
    localtime_s(&tmInfo, &tt);
#else
    localtime_r(&tt, &tmInfo);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M", &tmInfo);
    return std::string(buf);
}

}

std::string JournalEngine::todayDateString() {
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

bool JournalEngine::parseDateParts(const std::string& dateStr, int& year, int& month, int& day) {
    if (dateStr.size() != 10) return false;
    if (dateStr[4] != '-' || dateStr[7] != '-') return false;
    for (size_t i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(dateStr[i]))) return false;
    }
    year = std::stoi(dateStr.substr(0, 4));
    month = std::stoi(dateStr.substr(5, 2));
    day = std::stoi(dateStr.substr(8, 2));
    if (year < 1970 || year > 2199 || month < 1 || month > 12 || day < 1 || day > 31) return false;
    return true;
}

bool JournalEngine::isValidDateString(const std::string& dateStr) {
    int y = 0;
    int m = 0;
    int d = 0;
    return parseDateParts(dateStr, y, m, d);
}

std::string JournalEngine::normalizeDate(const std::string& raw) {
    std::string trimmed = trimJournalStr(raw);
    if (isValidDateString(trimmed)) return trimmed;
    if (trimmed.size() == 8) {
        bool allDigits = true;
        for (unsigned char c : trimmed) {
            if (!std::isdigit(c)) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            std::string formatted = trimmed.substr(0, 4) + "-" + trimmed.substr(4, 2) + "-" + trimmed.substr(6, 2);
            if (isValidDateString(formatted)) return formatted;
        }
    }
    return "";
}

std::string JournalEngine::shiftDateDays(const std::string& dateStr, int deltaDays) {
    int y = 0;
    int m = 0;
    int d = 0;
    if (!parseDateParts(dateStr, y, m, d)) return "";

    struct tm tmInfo = {};
    tmInfo.tm_year = y - 1900;
    tmInfo.tm_mon = m - 1;
    tmInfo.tm_mday = d + deltaDays;
    tmInfo.tm_hour = 12;
    std::time_t tt = std::mktime(&tmInfo);
    if (tt == static_cast<std::time_t>(-1)) return "";

    struct tm outTm = {};
#ifdef _WIN32
    localtime_s(&outTm, &tt);
#else
    localtime_r(&tt, &outTm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &outTm);
    return std::string(buf);
}

std::string JournalEngine::ensureTodayJournal(NoteManager& nm) {
    return openOrCreateDate(nm, todayDateString());
}

std::string JournalEngine::openOrCreateDate(NoteManager& nm, const std::string& yyyymmdd) {
    std::string dateStr = normalizeDate(yyyymmdd);
    if (dateStr.empty()) return "";

    Note* existing = nm.getNoteById(dateStr);
    if (existing) {
        return existing->id;
    }

    for (const auto& [id, note] : nm.getAllNotes()) {
        if (note.getFrontmatter("tipo") == "journal" && note.getFrontmatter("fecha") == dateStr) {
            return id;
        }
    }

    Note* created = nm.createNote(dateStr, "journals");
    if (!created) return "";

    created->frontmatter["tipo"] = "journal";
    created->frontmatter["fecha"] = dateStr;
    created->content = "# " + dateStr + "\n\n";
    created->title = dateStr;
    created->dirty = true;
    created->saveToFile();
    return created->id;
}

bool JournalEngine::quickCapture(NoteManager& nm, const std::string& text) {
    std::string trimmed = trimJournalStr(text);
    if (trimmed.empty()) return false;

    std::string noteId = ensureTodayJournal(nm);
    if (noteId.empty()) return false;

    Note* note = nm.getNoteById(noteId);
    if (!note) return false;

    if (!note->content.empty() && note->content.back() != '\n') {
        note->content.push_back('\n');
    }
    note->content += "- " + currentTimeHHMM() + " " + trimmed + "\n";
    note->parseLinks();
    note->dirty = true;
    note->saveToFile();
    return true;
}

std::vector<JournalEntry> JournalEngine::listJournals(const NoteManager& nm) const {
    std::vector<JournalEntry> entries;

    for (const auto& [id, note] : nm.getAllNotes()) {
        std::string dateStr;
        if (isValidDateString(id)) {
            dateStr = id;
        } else if (note.getFrontmatter("tipo") == "journal") {
            std::string fmDate = normalizeDate(note.getFrontmatter("fecha"));
            if (!fmDate.empty()) {
                dateStr = fmDate;
            }
        }
        if (dateStr.empty()) continue;

        JournalEntry entry;
        entry.date = dateStr;
        entry.noteId = id;

        std::istringstream iss(note.content);
        std::string line;
        while (std::getline(iss, line)) {
            std::string trimmed = trimJournalStr(line);
            if (trimmed.empty()) continue;
            if (trimmed[0] != '#') {
                ++entry.blockCount;
            }
            if (lineContainsTask(trimmed)) {
                ++entry.taskCount;
            }
            std::istringstream ws(trimmed);
            std::string word;
            while (ws >> word) {
                ++entry.wordCount;
            }
        }

        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const JournalEntry& a, const JournalEntry& b) {
        return a.date > b.date;
    });
    return entries;
}

int JournalEngine::calculateStreak(const NoteManager& nm) const {
    std::vector<JournalEntry> entries = listJournals(nm);
    if (entries.empty()) return 0;

    std::set<std::string> dates;
    for (const auto& e : entries) {
        dates.insert(e.date);
    }

    std::string cursor = todayDateString();
    if (dates.find(cursor) == dates.end()) {
        cursor = shiftDateDays(cursor, -1);
        if (cursor.empty() || dates.find(cursor) == dates.end()) {
            return 0;
        }
    }

    int streak = 0;
    while (!cursor.empty() && dates.find(cursor) != dates.end()) {
        ++streak;
        cursor = shiftDateDays(cursor, -1);
    }
    return streak;
}

std::vector<int> JournalEngine::activeDaysInMonth(const NoteManager& nm, int year, int month) const {
    std::set<int> daySet;
    std::vector<JournalEntry> entries = listJournals(nm);
    for (const auto& e : entries) {
        int y = 0;
        int m = 0;
        int d = 0;
        if (parseDateParts(e.date, y, m, d) && y == year && m == month) {
            daySet.insert(d);
        }
    }
    return std::vector<int>(daySet.begin(), daySet.end());
}

}
