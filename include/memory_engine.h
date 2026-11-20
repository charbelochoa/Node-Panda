#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

namespace nodepanda {

class NoteManager;

struct MemoryResult {
    std::string noteId;
    std::string title;
    float       score;
};

class MemoryEngine {
public:

    void reindex(const NoteManager& nm);

    std::vector<MemoryResult> findSimilar(const std::string& noteId, int topN = 8) const;

    std::vector<MemoryResult> search(const std::string& query, int topN = 10) const;

    std::vector<MemoryResult> suggestLinks(const std::string& noteId,
                                           const std::vector<std::string>& existingLinks,
                                           int topN = 5) const;

    int indexedNotes() const { return (int)m_docVectors.size(); }
    int vocabularySize() const { return (int)m_idf.size(); }
    bool needsReindex() const { return m_dirty; }
    void markDirty() { m_dirty = true; }

private:

    using SparseVec = std::unordered_map<int, float>;

    std::vector<std::string> tokenize(const std::string& text) const;

    SparseVec computeTfIdf(const std::vector<std::string>& tokens) const;

    static float cosineSimilarity(const SparseVec& a, const SparseVec& b);

    static float magnitude(const SparseVec& v);

    std::unordered_map<std::string, int> m_termToId;
    std::vector<std::string>             m_idToTerm;
    std::unordered_map<int, float>       m_idf;

    struct DocEntry {
        SparseVec   vec;
        float       norm = 0.0f;
        std::string title;
    };
    std::unordered_map<std::string, DocEntry> m_docVectors;

    bool m_dirty = true;

    static const std::unordered_set<std::string>& stopwords();
};

}
