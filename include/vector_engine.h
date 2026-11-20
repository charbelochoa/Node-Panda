#pragma once

#include "note_manager.h"
#include "outliner_engine.h"
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace nodepanda {

static constexpr int kVectorDim = 384;

struct VectorDocument {
    std::string id;
    std::string kind;
    std::string pageId;
    std::string title;
    std::string snippet;
    std::array<float, 384> embedding{};
};

struct VectorHit {
    std::string id;
    std::string kind;
    std::string pageId;
    std::string title;
    std::string snippet;
    float vectorScore = 0.0f;
    float keywordScore = 0.0f;
    float hybridScore = 0.0f;
};

struct VectorConfig {
    std::string modelId = "all-MiniLM-L6-v2";
    std::string apiEndpoint = "http://127.0.0.1:8089/v1/embeddings";
    std::string apiKey;
    bool useRemoteApi = false;
    float minScore = 0.15f;
    int topK = 10;
};

class VectorEngine {
public:
    void init(const std::filesystem::path& dataDir);
    void configure(const VectorConfig& cfg);
    const VectorConfig& config() const;

    void rebuildIndex(const NoteManager& nm, const OutlinerEngine& outliner);
    void indexNote(const Note& note);

    std::array<float, 384> embedText(const std::string& text) const;
    std::array<float, 384> embedLocalDeterministic(const std::string& text) const;
    bool embedViaApi(const std::string& text, std::array<float, 384>& outVec) const;

    static float cosineSimilarity(const std::array<float, 384>& a, const std::array<float, 384>& b);

    std::vector<VectorHit> searchHybrid(const std::string& query, int topK = 10) const;
    std::vector<VectorHit> findSimilarToNote(const std::string& noteId, int topK = 8) const;

    size_t documentCount() const;
    int dimension() const { return kVectorDim; }

    bool saveIndex() const;
    bool loadIndex();

private:
    void upsertDocument(VectorDocument&& doc, std::string&& searchText);
    static float computeKeywordScore(const std::string& foldedQuery,
                                     const std::vector<std::string>& queryTokens,
                                     const std::string& foldedTitle,
                                     const std::string& foldedBody);

    VectorConfig m_config;
    std::filesystem::path m_dataDir;
    std::filesystem::path m_indexPath;
    std::vector<VectorDocument> m_documents;
    std::vector<std::string> m_docSearchTexts;
    std::vector<std::string> m_docTitleFolded;
    std::unordered_map<std::string, size_t> m_docIndex;
};

}
