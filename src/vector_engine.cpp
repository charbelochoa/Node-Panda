#include "vector_engine.h"
#include "ai_provider.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

namespace nodepanda {

namespace {

constexpr uint32_t kBinaryVersion = 1;
constexpr uint32_t kMaxDocs = 1000000;
constexpr uint32_t kMaxFieldBytes = 1048576;

uint64_t fnv1a64(const char* data, size_t len, uint64_t seed = 14695981039346656037ULL) {
    uint64_t h = seed;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint8_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

uint64_t splitMix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

std::string foldText(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c == 0xC3 && i + 1 < input.size()) {
            unsigned char n = static_cast<unsigned char>(input[i + 1]);
            char mapped = 0;
            if ((n >= 0x80 && n <= 0x85) || (n >= 0xA0 && n <= 0xA5)) mapped = 'a';
            else if ((n >= 0x88 && n <= 0x8B) || (n >= 0xA8 && n <= 0xAB)) mapped = 'e';
            else if ((n >= 0x8C && n <= 0x8F) || (n >= 0xAC && n <= 0xAF)) mapped = 'i';
            else if (n == 0x91 || n == 0xB1) mapped = 'n';
            else if ((n >= 0x92 && n <= 0x96) || (n >= 0xB2 && n <= 0xB6)) mapped = 'o';
            else if ((n >= 0x99 && n <= 0x9C) || (n >= 0xB9 && n <= 0xBC)) mapped = 'u';
            else if (n == 0x87 || n == 0xA7) mapped = 'c';
            if (mapped != 0) {
                out.push_back(mapped);
                ++i;
                continue;
            }
        }
        if (c < 128) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

std::vector<std::string> tokenizeFolded(const std::string& folded) {
    std::vector<std::string> tokens;
    std::string cur;
    for (unsigned char c : folded) {
        if (std::isalnum(c) || c == '_' || c >= 128) {
            cur.push_back(static_cast<char>(c));
        } else {
            if (cur.size() >= 2) {
                tokens.push_back(std::move(cur));
            }
            cur.clear();
        }
    }
    if (cur.size() >= 2) {
        tokens.push_back(std::move(cur));
    }
    return tokens;
}

void projectFeature(std::array<float, kVectorDim>& vec, uint64_t hashVal, float weight, int projections) {
    uint64_t state = hashVal;
    for (int k = 0; k < projections; ++k) {
        state = splitMix64(state + static_cast<uint64_t>(k) * 0x517CC1B727220A95ULL);
        size_t idx = static_cast<size_t>(state % static_cast<uint64_t>(kVectorDim));
        float sign = ((state >> 63) & 1ULL) ? -1.0f : 1.0f;
        vec[idx] += sign * weight;
    }
}

bool normalizeL2(std::array<float, kVectorDim>& vec) {
    double sumSq = 0.0;
    for (float v : vec) {
        sumSq += static_cast<double>(v) * static_cast<double>(v);
    }
    if (sumSq <= 1e-12) {
        vec.fill(0.0f);
        return false;
    }
    float invNorm = static_cast<float>(1.0 / std::sqrt(sumSq));
    for (float& v : vec) {
        v *= invNorm;
    }
    return true;
}

std::string makeSnippet(const std::string& raw, size_t maxChars) {
    size_t start = 0;
    if (raw.size() >= 3 && raw[0] == '-' && raw[1] == '-' && raw[2] == '-') {
        size_t endFm = raw.find("\n---", 3);
        if (endFm != std::string::npos) {
            start = endFm + 4;
        }
    }

    std::string out;
    out.reserve(std::min(raw.size(), maxChars + 8));
    bool prevSpace = true;
    for (size_t i = start; i < raw.size() && out.size() < maxChars; ++i) {
        unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
            if (!prevSpace) {
                out.push_back(' ');
                prevSpace = true;
            }
        } else {
            out.push_back(static_cast<char>(c));
            prevSpace = false;
        }
    }

    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }

    while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80) {
        out.pop_back();
    }
    if (!out.empty() && (static_cast<unsigned char>(out.back()) & 0x80) != 0) {
        unsigned char lead = static_cast<unsigned char>(out.back());
        if ((lead & 0xE0) == 0xC0 || (lead & 0xF0) == 0xE0 || (lead & 0xF8) == 0xF0) {
            out.pop_back();
        }
    }

    if (raw.size() - start > out.size() + 3 && !out.empty()) {
        out += "...";
    }
    return out;
}

bool parseEmbeddingArray(const std::string& json, std::vector<float>& outRaw) {
    outRaw.clear();
    size_t keyPos = json.find("\"embedding\"");
    if (keyPos == std::string::npos) {
        keyPos = json.find("\"embeddings\"");
    }
    if (keyPos == std::string::npos) return false;

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return false;

    size_t pos = json.find('[', colonPos);
    if (pos == std::string::npos) return false;
    ++pos;

    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos < json.size() && json[pos] == '[') {
        ++pos;
    }

    const char* base = json.c_str();
    size_t len = json.size();
    while (pos < len) {
        while (pos < len && (std::isspace(static_cast<unsigned char>(base[pos])) || base[pos] == ',')) {
            ++pos;
        }
        if (pos >= len || base[pos] == ']') break;

        char* endPtr = nullptr;
        double val = std::strtod(base + pos, &endPtr);
        if (endPtr == base + pos) {
            break;
        }
        if (std::isfinite(val)) {
            outRaw.push_back(static_cast<float>(val));
        }
        pos = static_cast<size_t>(endPtr - base);
    }

    return !outRaw.empty();
}

void writeString(std::ofstream& ofs, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) {
        ofs.write(s.data(), static_cast<std::streamsize>(len));
    }
}

bool readString(std::ifstream& ifs, std::string& out) {
    uint32_t len = 0;
    if (!ifs.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
    if (len > kMaxFieldBytes) return false;
    out.resize(len);
    if (len > 0) {
        if (!ifs.read(out.data(), static_cast<std::streamsize>(len))) return false;
    }
    return true;
}

}

void VectorEngine::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    m_indexPath = dataDir / ".nodepanda" / "vectors.bin";
    std::error_code ec;
    std::filesystem::create_directories(m_indexPath.parent_path(), ec);
    loadIndex();
}

void VectorEngine::configure(const VectorConfig& cfg) {
    m_config = cfg;
    if (m_config.topK < 1) m_config.topK = 1;
    if (m_config.topK > 100) m_config.topK = 100;
    if (m_config.minScore < 0.0f) m_config.minScore = 0.0f;
    if (m_config.minScore > 1.0f) m_config.minScore = 1.0f;
}

const VectorConfig& VectorEngine::config() const {
    return m_config;
}

void VectorEngine::upsertDocument(VectorDocument&& doc, std::string&& searchText) {
    std::string foldedTitle = foldText(doc.title);
    auto it = m_docIndex.find(doc.id);
    if (it != m_docIndex.end()) {
        size_t idx = it->second;
        m_documents[idx] = std::move(doc);
        m_docSearchTexts[idx] = std::move(searchText);
        m_docTitleFolded[idx] = std::move(foldedTitle);
    } else {
        size_t idx = m_documents.size();
        m_docIndex[doc.id] = idx;
        m_documents.push_back(std::move(doc));
        m_docSearchTexts.push_back(std::move(searchText));
        m_docTitleFolded.push_back(std::move(foldedTitle));
    }
}

void VectorEngine::indexNote(const Note& note) {
    std::ostringstream full;
    if (!note.title.empty()) {
        full << note.title << " " << note.title << "\n";
    } else {
        full << note.id << "\n";
    }
    for (const auto& kv : note.frontmatter) {
        full << kv.first << " " << kv.second << "\n";
    }
    full << note.content;

    std::string fullStr = full.str();
    VectorDocument doc;
    doc.id = note.id;
    doc.kind = "note";
    doc.pageId = note.id;
    doc.title = note.title.empty() ? note.id : note.title;
    doc.snippet = makeSnippet(note.content, 220);
    doc.embedding = embedText(fullStr);

    upsertDocument(std::move(doc), foldText(fullStr));
}

void VectorEngine::rebuildIndex(const NoteManager& nm, const OutlinerEngine& outliner) {
    m_documents.clear();
    m_docSearchTexts.clear();
    m_docTitleFolded.clear();
    m_docIndex.clear();

    const auto& allNotes = nm.getAllNotes();
    std::unordered_map<std::string, std::string> pageTitles;
    pageTitles.reserve(allNotes.size());
    for (const auto& kv : allNotes) {
        const Note& note = kv.second;
        pageTitles[note.id] = note.title.empty() ? note.id : note.title;
        indexNote(note);
    }

    const auto& allBlocks = outliner.getAllBlocks();
    for (const auto& kv : allBlocks) {
        const BlockNode& block = kv.second;
        std::string trimmedSnippet = makeSnippet(block.content, 200);
        if (trimmedSnippet.size() < 12) continue;

        std::string pageTitle = block.pageId;
        auto titleIt = pageTitles.find(block.pageId);
        if (titleIt != pageTitles.end()) {
            pageTitle = titleIt->second;
        }

        std::ostringstream blockText;
        blockText << pageTitle << " " << block.content;
        for (const auto& tag : block.tags) {
            blockText << " " << tag;
        }
        std::string textStr = blockText.str();

        VectorDocument doc;
        doc.id = "block:" + block.uuid;
        doc.kind = "block";
        doc.pageId = block.pageId;
        doc.title = pageTitle;
        doc.snippet = std::move(trimmedSnippet);
        doc.embedding = embedText(textStr);

        upsertDocument(std::move(doc), foldText(textStr));
    }

    if (!m_indexPath.empty()) {
        saveIndex();
    }
}

std::array<float, 384> VectorEngine::embedText(const std::string& text) const {
    if (m_config.useRemoteApi) {
        std::array<float, 384> remoteVec{};
        if (embedViaApi(text, remoteVec)) {
            return remoteVec;
        }
    }
    return embedLocalDeterministic(text);
}

std::array<float, 384> VectorEngine::embedLocalDeterministic(const std::string& text) const {
    std::array<float, 384> vec{};
    if (text.empty()) return vec;

    std::string folded = foldText(text);
    std::vector<std::string> tokens = tokenizeFolded(folded);
    if (tokens.empty()) return vec;

    std::unordered_map<std::string, int> tokenCounts;
    tokenCounts.reserve(tokens.size());
    for (const auto& tok : tokens) {
        tokenCounts[tok] += 1;
    }

    for (const auto& kv : tokenCounts) {
        const std::string& tok = kv.first;
        float tfWeight = 1.0f + std::log(static_cast<float>(kv.second));

        uint64_t tokHash = fnv1a64(tok.data(), tok.size(), 0xCBF29CE484222325ULL);
        projectFeature(vec, tokHash, tfWeight * 2.2f, 4);

        std::string padded;
        padded.reserve(tok.size() + 2);
        padded.push_back('^');
        padded.append(tok);
        padded.push_back('$');

        if (padded.size() >= 3) {
            size_t ngramCount = padded.size() - 2;
            float ngramWeight = (tfWeight * 1.1f) / std::sqrt(static_cast<float>(ngramCount));
            for (size_t i = 0; i + 3 <= padded.size(); ++i) {
                uint64_t ngHash = fnv1a64(padded.data() + i, 3, 0x9E3779B97F4A7C15ULL);
                projectFeature(vec, ngHash, ngramWeight, 2);
            }
        }
    }

    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        std::string bigram = tokens[i] + " " + tokens[i + 1];
        uint64_t bgHash = fnv1a64(bigram.data(), bigram.size(), 0xD6E8FEB86659FD93ULL);
        projectFeature(vec, bgHash, 1.35f, 3);
    }

    normalizeL2(vec);
    return vec;
}

bool VectorEngine::embedViaApi(const std::string& text, std::array<float, 384>& outVec) const {
    outVec.fill(0.0f);
    if (m_config.apiEndpoint.empty() || text.empty()) return false;

    std::string payloadText = text;
    if (payloadText.size() > 8000) {
        payloadText.resize(8000);
    }

    std::ostringstream body;
    body << "{\"model\":\"" << AiProviderClient::jsonEscape(m_config.modelId) << "\","
         << "\"input\":\"" << AiProviderClient::jsonEscape(payloadText) << "\"}";

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    if (!m_config.apiKey.empty()) {
        headers["Authorization"] = "Bearer " + m_config.apiKey;
    }

    CompletionResult resp = AiProviderClient::httpPostJson(m_config.apiEndpoint, headers, body.str(), 15000);
    if (!resp.ok || resp.text.empty()) return false;

    std::vector<float> raw;
    if (!parseEmbeddingArray(resp.text, raw)) return false;

    if (raw.size() == static_cast<size_t>(kVectorDim)) {
        for (int i = 0; i < kVectorDim; ++i) {
            outVec[static_cast<size_t>(i)] = raw[static_cast<size_t>(i)];
        }
    } else {
        for (size_t i = 0; i < raw.size(); ++i) {
            float val = raw[i];
            if (val == 0.0f) continue;
            uint64_t seed = splitMix64(static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL + 0xA0761D6478BD642FULL);
            projectFeature(outVec, seed, val, 4);
        }
    }

    return normalizeL2(outVec);
}

float VectorEngine::cosineSimilarity(const std::array<float, 384>& a, const std::array<float, 384>& b) {
    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    for (size_t i = 0; i < static_cast<size_t>(kVectorDim); ++i) {
        double av = static_cast<double>(a[i]);
        double bv = static_cast<double>(b[i]);
        dot += av * bv;
        normA += av * av;
        normB += bv * bv;
    }
    if (normA <= 1e-12 || normB <= 1e-12) return 0.0f;
    double sim = dot / (std::sqrt(normA) * std::sqrt(normB));
    if (sim > 1.0) sim = 1.0;
    if (sim < -1.0) sim = -1.0;
    return static_cast<float>(sim);
}

float VectorEngine::computeKeywordScore(const std::string& foldedQuery,
                                        const std::vector<std::string>& queryTokens,
                                        const std::string& foldedTitle,
                                        const std::string& foldedBody) {
    if (foldedQuery.empty() || queryTokens.empty()) return 0.0f;

    float score = 0.0f;
    if (foldedTitle.find(foldedQuery) != std::string::npos) {
        score += 0.45f;
    } else if (foldedBody.find(foldedQuery) != std::string::npos) {
        score += 0.25f;
    }

    float tokenHits = 0.0f;
    for (const auto& tok : queryTokens) {
        if (foldedTitle.find(tok) != std::string::npos) {
            tokenHits += 1.0f;
        } else if (foldedBody.find(tok) != std::string::npos) {
            tokenHits += 0.72f;
        }
    }

    score += 0.65f * (tokenHits / static_cast<float>(queryTokens.size()));
    return std::min(1.0f, std::max(0.0f, score));
}

std::vector<VectorHit> VectorEngine::searchHybrid(const std::string& query, int topK) const {
    std::vector<VectorHit> results;
    if (query.empty() || m_documents.empty()) return results;

    std::string foldedQuery = foldText(query);
    std::vector<std::string> queryTokens = tokenizeFolded(foldedQuery);
    if (queryTokens.empty()) return results;

    int limit = topK > 0 ? topK : m_config.topK;
    std::array<float, 384> queryVec = embedText(query);

    results.reserve(m_documents.size());
    for (size_t i = 0; i < m_documents.size(); ++i) {
        const VectorDocument& doc = m_documents[i];
        float vecScore = std::max(0.0f, cosineSimilarity(queryVec, doc.embedding));
        float kwScore = computeKeywordScore(foldedQuery, queryTokens, m_docTitleFolded[i], m_docSearchTexts[i]);
        float hybrid = 0.65f * vecScore + 0.35f * kwScore;

        if (hybrid >= m_config.minScore) {
            VectorHit hit;
            hit.id = doc.id;
            hit.kind = doc.kind;
            hit.pageId = doc.pageId;
            hit.title = doc.title;
            hit.snippet = doc.snippet;
            hit.vectorScore = vecScore;
            hit.keywordScore = kwScore;
            hit.hybridScore = std::min(1.0f, hybrid);
            results.push_back(std::move(hit));
        }
    }

    std::sort(results.begin(), results.end(), [](const VectorHit& a, const VectorHit& b) {
        if (a.hybridScore != b.hybridScore) return a.hybridScore > b.hybridScore;
        if (a.vectorScore != b.vectorScore) return a.vectorScore > b.vectorScore;
        return a.title < b.title;
    });

    if (static_cast<int>(results.size()) > limit) {
        results.resize(static_cast<size_t>(limit));
    }
    return results;
}

std::vector<VectorHit> VectorEngine::findSimilarToNote(const std::string& noteId, int topK) const {
    std::vector<VectorHit> results;
    if (noteId.empty() || m_documents.empty()) return results;

    const VectorDocument* sourceDoc = nullptr;
    auto it = m_docIndex.find(noteId);
    if (it != m_docIndex.end()) {
        sourceDoc = &m_documents[it->second];
    } else {
        for (const auto& doc : m_documents) {
            if (doc.kind == "note" && doc.pageId == noteId) {
                sourceDoc = &doc;
                break;
            }
        }
    }
    if (!sourceDoc) return results;

    int limit = topK > 0 ? topK : 8;
    std::unordered_map<std::string, VectorHit> bestPerPage;

    for (const auto& doc : m_documents) {
        if (doc.pageId == noteId || doc.id == noteId) continue;
        float vecScore = std::max(0.0f, cosineSimilarity(sourceDoc->embedding, doc.embedding));
        if (vecScore <= 0.01f) continue;

        auto existing = bestPerPage.find(doc.pageId);
        bool shouldReplace = (existing == bestPerPage.end()) ||
                             (vecScore > existing->second.vectorScore) ||
                             (vecScore == existing->second.vectorScore && doc.kind == "note" && existing->second.kind != "note");
        if (shouldReplace) {
            VectorHit hit;
            hit.id = doc.id;
            hit.kind = doc.kind;
            hit.pageId = doc.pageId;
            hit.title = doc.title;
            hit.snippet = doc.snippet;
            hit.vectorScore = vecScore;
            hit.keywordScore = 0.0f;
            hit.hybridScore = vecScore;
            bestPerPage[doc.pageId] = std::move(hit);
        }
    }

    results.reserve(bestPerPage.size());
    for (auto& kv : bestPerPage) {
        results.push_back(std::move(kv.second));
    }

    std::sort(results.begin(), results.end(), [](const VectorHit& a, const VectorHit& b) {
        if (a.hybridScore != b.hybridScore) return a.hybridScore > b.hybridScore;
        return a.title < b.title;
    });

    if (static_cast<int>(results.size()) > limit) {
        results.resize(static_cast<size_t>(limit));
    }
    return results;
}

size_t VectorEngine::documentCount() const {
    return m_documents.size();
}

bool VectorEngine::saveIndex() const {
    if (m_indexPath.empty()) return false;

    std::error_code ec;
    std::filesystem::create_directories(m_indexPath.parent_path(), ec);

    std::ofstream ofs(m_indexPath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return false;

    const char magic[4] = {'N', 'P', 'V', '1'};
    uint32_t version = kBinaryVersion;
    uint32_t dim = static_cast<uint32_t>(kVectorDim);
    uint32_t count = static_cast<uint32_t>(m_documents.size());

    ofs.write(magic, 4);
    ofs.write(reinterpret_cast<const char*>(&version), sizeof(version));
    ofs.write(reinterpret_cast<const char*>(&dim), sizeof(dim));
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& doc : m_documents) {
        writeString(ofs, doc.id);
        writeString(ofs, doc.kind);
        writeString(ofs, doc.pageId);
        writeString(ofs, doc.title);
        writeString(ofs, doc.snippet);
        ofs.write(reinterpret_cast<const char*>(doc.embedding.data()),
                  static_cast<std::streamsize>(sizeof(float) * static_cast<size_t>(kVectorDim)));
    }

    return ofs.good();
}

bool VectorEngine::loadIndex() {
    if (m_indexPath.empty() || !std::filesystem::exists(m_indexPath)) return false;

    std::ifstream ifs(m_indexPath, std::ios::binary);
    if (!ifs.is_open()) return false;

    char magic[4] = {};
    uint32_t version = 0;
    uint32_t dim = 0;
    uint32_t count = 0;

    if (!ifs.read(magic, 4)) return false;
    if (std::memcmp(magic, "NPV1", 4) != 0) return false;
    if (!ifs.read(reinterpret_cast<char*>(&version), sizeof(version))) return false;
    if (!ifs.read(reinterpret_cast<char*>(&dim), sizeof(dim))) return false;
    if (!ifs.read(reinterpret_cast<char*>(&count), sizeof(count))) return false;

    if (version != kBinaryVersion || dim != static_cast<uint32_t>(kVectorDim) || count > kMaxDocs) {
        return false;
    }

    std::vector<VectorDocument> loadedDocs;
    std::vector<std::string> loadedTexts;
    std::vector<std::string> loadedTitles;
    std::unordered_map<std::string, size_t> loadedIndex;

    loadedDocs.reserve(count);
    loadedTexts.reserve(count);
    loadedTitles.reserve(count);
    loadedIndex.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        VectorDocument doc;
        if (!readString(ifs, doc.id)) return false;
        if (!readString(ifs, doc.kind)) return false;
        if (!readString(ifs, doc.pageId)) return false;
        if (!readString(ifs, doc.title)) return false;
        if (!readString(ifs, doc.snippet)) return false;
        if (!ifs.read(reinterpret_cast<char*>(doc.embedding.data()),
                      static_cast<std::streamsize>(sizeof(float) * static_cast<size_t>(kVectorDim)))) {
            return false;
        }

        std::string combined = doc.title + " " + doc.snippet + " " + doc.pageId;
        loadedIndex[doc.id] = loadedDocs.size();
        loadedTitles.push_back(foldText(doc.title));
        loadedTexts.push_back(foldText(combined));
        loadedDocs.push_back(std::move(doc));
    }

    m_documents = std::move(loadedDocs);
    m_docSearchTexts = std::move(loadedTexts);
    m_docTitleFolded = std::move(loadedTitles);
    m_docIndex = std::move(loadedIndex);
    return true;
}

}
