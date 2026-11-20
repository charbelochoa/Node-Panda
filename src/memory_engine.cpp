#include "memory_engine.h"
#include "note_manager.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <numeric>

namespace nodepanda {

const std::unordered_set<std::string>& MemoryEngine::stopwords() {
    static const std::unordered_set<std::string> sw = {

        "a", "al", "algo", "como", "con", "de", "del", "el", "ella", "ellos",
        "en", "es", "esa", "ese", "eso", "esta", "este", "esto", "hay", "la",
        "las", "le", "les", "lo", "los", "mas", "me", "mi", "muy", "no", "nos",
        "o", "otra", "otro", "para", "pero", "por", "que", "se", "ser", "si",
        "sin", "sobre", "su", "sus", "te", "ti", "tu", "tus", "un", "una",
        "uno", "unas", "unos", "y", "ya", "yo",

        "a", "an", "and", "are", "as", "at", "be", "by", "do", "for", "from",
        "has", "have", "he", "her", "him", "his", "how", "i", "if", "in", "is",
        "it", "its", "my", "no", "not", "of", "on", "or", "our", "she", "so",
        "that", "the", "them", "then", "there", "these", "they", "this", "to",
        "up", "us", "was", "we", "what", "when", "who", "will", "with", "you",

        "md", "http", "https", "www", "com"
    };
    return sw;
}

std::vector<std::string> MemoryEngine::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    const auto& sw = stopwords();

    std::string current;
    current.reserve(32);

    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            current += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        } else {
            if (current.size() >= 2 && sw.find(current) == sw.end()) {
                tokens.push_back(std::move(current));
            }
            current.clear();
        }
    }

    if (current.size() >= 2 && sw.find(current) == sw.end()) {
        tokens.push_back(std::move(current));
    }

    return tokens;
}

void MemoryEngine::reindex(const NoteManager& nm) {
    m_termToId.clear();
    m_idToTerm.clear();
    m_idf.clear();
    m_docVectors.clear();

    const auto& allNotes = nm.getAllNotes();
    if (allNotes.empty()) {
        m_dirty = false;
        return;
    }

    struct DocTokens {
        std::string noteId;
        std::string title;
        std::vector<std::string> tokens;
    };
    std::vector<DocTokens> docs;
    docs.reserve(allNotes.size());

    std::unordered_map<std::string, int> docFreq;

    for (const auto& [id, note] : allNotes) {
        DocTokens dt;
        dt.noteId = id;
        dt.title  = note.title;

        std::string fullText = note.title + " " + note.title + " " + note.content;
        for (const auto& [k, v] : note.frontmatter) {
            fullText += " " + k + " " + v;
        }

        dt.tokens = tokenize(fullText);

        std::unordered_set<std::string> uniqueTerms(dt.tokens.begin(), dt.tokens.end());
        for (const auto& term : uniqueTerms) {
            docFreq[term]++;
        }

        docs.push_back(std::move(dt));
    }

    const float N = static_cast<float>(docs.size());
    int nextId = 0;

    for (const auto& [term, df] : docFreq) {

        if (df < 1 || static_cast<float>(df) > N * 0.80f) continue;

        int termId = nextId++;
        m_termToId[term] = termId;
        m_idToTerm.push_back(term);

        m_idf[termId] = std::log(1.0f + N / static_cast<float>(df));
    }

    for (const auto& dt : docs) {
        SparseVec vec = computeTfIdf(dt.tokens);
        float norm = magnitude(vec);

        DocEntry entry;
        entry.vec   = std::move(vec);
        entry.norm  = norm;
        entry.title = dt.title;
        m_docVectors[dt.noteId] = std::move(entry);
    }

    m_dirty = false;
}

MemoryEngine::SparseVec MemoryEngine::computeTfIdf(const std::vector<std::string>& tokens) const {

    std::unordered_map<int, int> tf;
    for (const auto& token : tokens) {
        auto it = m_termToId.find(token);
        if (it != m_termToId.end()) {
            tf[it->second]++;
        }
    }

    SparseVec vec;
    for (const auto& [termId, count] : tf) {
        auto idfIt = m_idf.find(termId);
        if (idfIt != m_idf.end()) {
            float tfWeight = 1.0f + std::log(static_cast<float>(count));
            vec[termId] = tfWeight * idfIt->second;
        }
    }

    return vec;
}

float MemoryEngine::cosineSimilarity(const SparseVec& a, const SparseVec& b) {
    float dot = 0.0f;

    const auto& smaller = (a.size() <= b.size()) ? a : b;
    const auto& larger  = (a.size() <= b.size()) ? b : a;

    for (const auto& [termId, weight] : smaller) {
        auto it = larger.find(termId);
        if (it != larger.end()) {
            dot += weight * it->second;
        }
    }

    return dot;
}

float MemoryEngine::magnitude(const SparseVec& v) {
    float sum = 0.0f;
    for (const auto& [_, w] : v) {
        sum += w * w;
    }
    return std::sqrt(sum);
}

std::vector<MemoryResult> MemoryEngine::findSimilar(const std::string& noteId, int topN) const {
    std::vector<MemoryResult> results;

    auto targetIt = m_docVectors.find(noteId);
    if (targetIt == m_docVectors.end()) return results;

    const auto& target = targetIt->second;
    if (target.norm < 1e-6f) return results;

    for (const auto& [id, entry] : m_docVectors) {
        if (id == noteId) continue;
        if (entry.norm < 1e-6f) continue;

        float dot   = cosineSimilarity(target.vec, entry.vec);
        float score = dot / (target.norm * entry.norm);

        if (score > 0.01f) {
            results.push_back({id, entry.title, score});
        }
    }

    std::sort(results.begin(), results.end(),
              [](const MemoryResult& a, const MemoryResult& b) {
                  return a.score > b.score;
              });

    if ((int)results.size() > topN)
        results.resize(topN);

    return results;
}

std::vector<MemoryResult> MemoryEngine::search(const std::string& query, int topN) const {
    std::vector<MemoryResult> results;

    auto queryTokens = tokenize(query);
    if (queryTokens.empty()) return results;

    SparseVec queryVec = computeTfIdf(queryTokens);
    float queryNorm = magnitude(queryVec);
    if (queryNorm < 1e-6f) return results;

    for (const auto& [id, entry] : m_docVectors) {
        if (entry.norm < 1e-6f) continue;

        float dot   = cosineSimilarity(queryVec, entry.vec);
        float score = dot / (queryNorm * entry.norm);

        if (score > 0.005f) {
            results.push_back({id, entry.title, score});
        }
    }

    std::sort(results.begin(), results.end(),
              [](const MemoryResult& a, const MemoryResult& b) {
                  return a.score > b.score;
              });

    if ((int)results.size() > topN)
        results.resize(topN);

    return results;
}

std::vector<MemoryResult> MemoryEngine::suggestLinks(
    const std::string& noteId,
    const std::vector<std::string>& existingLinks,
    int topN) const
{

    auto similar = findSimilar(noteId, topN + (int)existingLinks.size() + 1);

    std::unordered_set<std::string> linked(existingLinks.begin(), existingLinks.end());

    std::vector<MemoryResult> suggestions;
    for (const auto& r : similar) {
        if (linked.find(r.noteId) == linked.end()) {
            suggestions.push_back(r);
            if ((int)suggestions.size() >= topN) break;
        }
    }

    return suggestions;
}

}
