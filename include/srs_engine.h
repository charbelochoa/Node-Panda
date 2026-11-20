#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace nodepanda {

class NoteManager;

struct SRSCard {
    std::string noteId;
    std::string title;
    int interval = 1;
    float easeFactor = 2.5f;
    int repetitions = 0;
    std::string nextReview;
    std::string lastReview;
    bool isDue = false;
};

enum class SRSRating {
    Again = 0,
    Hard  = 2,
    Good  = 3,
    Easy  = 5
};

struct ClozeCard {
    std::string noteId;
    std::string prompt;
    std::string answer;
};

class SRSEngine {
public:

    void buildQueue(NoteManager& nm);
    std::vector<ClozeCard> extractClozeCards(const NoteManager& nm) const;

    void rateCard(const std::string& noteId, SRSRating rating, NoteManager& nm);

    void enrollNote(const std::string& noteId, NoteManager& nm);

    void unenrollNote(const std::string& noteId, NoteManager& nm);

    bool isEnrolled(const std::string& noteId, NoteManager& nm) const;

    const std::vector<SRSCard>& getDueCards() const { return m_dueCards; }

    const std::vector<SRSCard>& getAllCards() const { return m_allCards; }

    int totalEnrolled() const { return (int)m_allCards.size(); }
    int dueToday() const { return (int)m_dueCards.size(); }

    static std::string todayStr();

private:
    void applySmTwo(SRSCard& card, SRSRating rating);
    SRSCard readCardFromNote(const std::string& noteId, NoteManager& nm) const;
    void writeCardToNote(const SRSCard& card, NoteManager& nm);

    std::vector<SRSCard> m_dueCards;
    std::vector<SRSCard> m_allCards;
};

}
