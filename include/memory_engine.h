#pragma once
// ============================================================================
// MemoryEngine — Motor de Memoria Semántica basado en TF-IDF
//
// Indexa todas las notas usando Term Frequency–Inverse Document Frequency
// y permite buscar notas similares por coseno, encontrar conexiones
// sugeridas (notas semánticamente cercanas pero no enlazadas), y realizar
// búsqueda libre de texto con ranking por relevancia.
//
// No requiere APIs externas ni modelos de ML — funciona 100% local.
// ============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

namespace nodepanda {

class NoteManager;

// Resultado de búsqueda / similitud
struct MemoryResult {
    std::string noteId;
    std::string title;
    float       score;     // 0.0 – 1.0 (coseno normalizado)
};

class MemoryEngine {
public:
    // Reconstruir el índice completo a partir de todas las notas
    void reindex(const NoteManager& nm);

    // Encontrar las N notas más similares a una nota dada (excluye la propia)
    std::vector<MemoryResult> findSimilar(const std::string& noteId, int topN = 8) const;

    // Búsqueda libre: tokeniza la consulta y rankea notas por TF-IDF coseno
    std::vector<MemoryResult> search(const std::string& query, int topN = 10) const;

    // Conexiones sugeridas: notas similares que NO están enlazadas directamente
    std::vector<MemoryResult> suggestLinks(const std::string& noteId,
                                           const std::vector<std::string>& existingLinks,
                                           int topN = 5) const;

    // Estadísticas
    int indexedNotes() const { return (int)m_docVectors.size(); }
    int vocabularySize() const { return (int)m_idf.size(); }
    bool needsReindex() const { return m_dirty; }
    void markDirty() { m_dirty = true; }

private:
    // Representación sparse de un vector TF-IDF
    using SparseVec = std::unordered_map<int, float>; // termId → weight

    // Tokenizar texto en términos normalizados (lowercase, sin puntuación, sin stopwords)
    std::vector<std::string> tokenize(const std::string& text) const;

    // Calcular TF-IDF vector sparse para un documento dado sus tokens
    SparseVec computeTfIdf(const std::vector<std::string>& tokens) const;

    // Coseno entre dos vectores sparse
    static float cosineSimilarity(const SparseVec& a, const SparseVec& b);

    // Magnitud de un vector sparse
    static float magnitude(const SparseVec& v);

    // ── Índice ──────────────────────────────────────────────────────────────
    std::unordered_map<std::string, int> m_termToId;   // término → id numérico
    std::vector<std::string>             m_idToTerm;    // id → término
    std::unordered_map<int, float>       m_idf;         // termId → IDF score

    // Por cada nota: su vector TF-IDF y título cacheado
    struct DocEntry {
        SparseVec   vec;
        float       norm = 0.0f;   // magnitud pre-calculada
        std::string title;
    };
    std::unordered_map<std::string, DocEntry> m_docVectors; // noteId → DocEntry

    bool m_dirty = true;   // necesita re-indexar

    // Stopwords en español (las más comunes, para reducir ruido)
    static const std::unordered_set<std::string>& stopwords();
};

} // namespace nodepanda
