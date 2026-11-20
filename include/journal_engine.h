#pragma once

#include "note_manager.h"
#include <string>
#include <vector>

namespace nodepanda {

struct JournalEntry {
    std::string date;
    std::string noteId;
    int blockCount = 0;
    int wordCount = 0;
    int taskCount = 0;
};

class JournalEngine {
public:
    static std::string todayDateString();

    std::string ensureTodayJournal(NoteManager& nm);
    std::string openOrCreateDate(NoteManager& nm, const std::string& yyyymmdd);
    bool quickCapture(NoteManager& nm, const std::string& text);

    std::vector<JournalEntry> listJournals(const NoteManager& nm) const;
    int calculateStreak(const NoteManager& nm) const;
    std::vector<int> activeDaysInMonth(const NoteManager& nm, int year, int month) const;

private:
    static std::string normalizeDate(const std::string& raw);
    static bool isValidDateString(const std::string& dateStr);
    static bool parseDateParts(const std::string& dateStr, int& year, int& month, int& day);
    static std::string shiftDateDays(const std::string& dateStr, int deltaDays);
};

}
