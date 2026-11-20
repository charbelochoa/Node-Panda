#include "srs_engine.h"
#include "note_manager.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace nodepanda {

static std::tm parseDate(const std::string& dateStr) {
    std::tm tm = {};
    std::istringstream ss(dateStr);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    return tm;
}

static std::string formatDate(const std::tm& tm) {
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d");
    return ss.str();
}

static std::string addDaysToDate(const std::string& dateStr, int days) {
    std::tm tm = parseDate(dateStr);
    auto time = std::mktime(&tm);
    time += days * 86400;
    tm = *std::localtime(&time);
    return formatDate(tm);
}

static int compareDates(const std::string& date1, const std::string& date2) {
    if (date1 < date2) return -1;
    if (date1 > date2) return 1;
    return 0;
}

std::string SRSEngine::todayStr() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time);
    return formatDate(tm);
}

void SRSEngine::buildQueue(NoteManager& nm) {
    m_allCards.clear();
    m_dueCards.clear();

    std::string today = todayStr();
    auto& allNotes = nm.getAllNotes();

    for (auto& pair : allNotes) {
        const std::string& noteId = pair.first;
        Note& note = pair.second;

        auto it = note.frontmatter.find("srs_next");
        if (it != note.frontmatter.end()) {
            SRSCard card = readCardFromNote(noteId, nm);
            m_allCards.push_back(card);

            if (compareDates(card.nextReview, today) <= 0) {
                card.isDue = true;
                m_dueCards.push_back(card);
            }
        }
    }
}

SRSCard SRSEngine::readCardFromNote(const std::string& noteId, NoteManager& nm) const {
    SRSCard card;
    card.noteId = noteId;

    Note* note = nm.getNoteById(noteId);
    if (!note) return card;

    card.title = note->title;

    auto it = note->frontmatter.find("srs_interval");
    if (it != note->frontmatter.end()) {
        card.interval = std::stoi(it->second);
    }

    it = note->frontmatter.find("srs_ease");
    if (it != note->frontmatter.end()) {
        card.easeFactor = std::stof(it->second);
    }

    it = note->frontmatter.find("srs_reps");
    if (it != note->frontmatter.end()) {
        card.repetitions = std::stoi(it->second);
    }

    it = note->frontmatter.find("srs_next");
    if (it != note->frontmatter.end()) {
        card.nextReview = it->second;
    }

    it = note->frontmatter.find("srs_last");
    if (it != note->frontmatter.end()) {
        card.lastReview = it->second;
    }

    return card;
}

void SRSEngine::writeCardToNote(const SRSCard& card, NoteManager& nm) {
    Note* note = nm.getNoteById(card.noteId);
    if (!note) return;

    note->setFrontmatter("srs_interval", std::to_string(card.interval));
    note->setFrontmatter("srs_ease", std::to_string(card.easeFactor));
    note->setFrontmatter("srs_reps", std::to_string(card.repetitions));
    note->setFrontmatter("srs_next", card.nextReview);
    note->setFrontmatter("srs_last", card.lastReview);

    note->saveToFile();
}

void SRSEngine::applySmTwo(SRSCard& card, SRSRating rating) {
    int quality = static_cast<int>(rating);
    std::string today = todayStr();

    if (quality >= 3) {

        if (card.repetitions == 0) {
            card.interval = 1;
        } else if (card.repetitions == 1) {
            card.interval = 6;
        } else {
            card.interval = static_cast<int>(std::round(card.interval * card.easeFactor));
        }
        card.repetitions++;
    } else {

        card.repetitions = 0;
        card.interval = 1;
    }

    card.easeFactor += 0.1f - (5.0f - quality) * (0.08f + (5.0f - quality) * 0.02f);
    if (card.easeFactor < 1.3f) {
        card.easeFactor = 1.3f;
    }

    card.nextReview = addDaysToDate(today, card.interval);
    card.lastReview = today;
}

void SRSEngine::enrollNote(const std::string& noteId, NoteManager& nm) {
    Note* note = nm.getNoteById(noteId);
    if (!note) return;

    if (isEnrolled(noteId, nm)) return;

    std::string today = todayStr();

    note->setFrontmatter("srs_interval", "1");
    note->setFrontmatter("srs_ease", "2.5");
    note->setFrontmatter("srs_reps", "0");
    note->setFrontmatter("srs_next", today);
    note->setFrontmatter("srs_last", "");

    note->saveToFile();
}

void SRSEngine::unenrollNote(const std::string& noteId, NoteManager& nm) {
    Note* note = nm.getNoteById(noteId);
    if (!note) return;

    note->frontmatter.erase("srs_interval");
    note->frontmatter.erase("srs_ease");
    note->frontmatter.erase("srs_reps");
    note->frontmatter.erase("srs_next");
    note->frontmatter.erase("srs_last");

    note->saveToFile();
}

bool SRSEngine::isEnrolled(const std::string& noteId, NoteManager& nm) const {
    Note* note = nm.getNoteById(noteId);
    if (!note) return false;

    return note->frontmatter.find("srs_next") != note->frontmatter.end();
}

void SRSEngine::rateCard(const std::string& noteId, SRSRating rating, NoteManager& nm) {
    SRSCard card = readCardFromNote(noteId, nm);
    applySmTwo(card, rating);
    writeCardToNote(card, nm);
}

}
