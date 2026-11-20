#include "context_brain.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace nodepanda {

namespace {

std::string toLowerAscii(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::vector<std::string> tokenizeQuery(const std::string& query) {
    std::vector<std::string> terms;
    std::string cur;
    for (unsigned char c : query) {
        if (std::isalnum(c) || c == '_' || c == '-') {
            cur.push_back(static_cast<char>(std::tolower(c)));
        } else {
            if (cur.size() >= 2) terms.push_back(cur);
            cur.clear();
        }
    }
    if (cur.size() >= 2) terms.push_back(cur);

    std::sort(terms.begin(), terms.end());
    terms.erase(std::unique(terms.begin(), terms.end()), terms.end());
    return terms;
}

int countOccurrences(const std::string& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) return 0;
    int count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

std::string buildSnippet(const std::string& content, const std::vector<std::string>& terms, size_t maxChars) {
    if (content.size() <= maxChars) return content;
    std::string low = toLowerAscii(content);
    size_t bestPos = 0;
    for (const auto& t : terms) {
        size_t p = low.find(t);
        if (p != std::string::npos) {
            bestPos = (p > 80) ? (p - 80) : 0;
            break;
        }
    }
    size_t len = std::min(maxChars, content.size() - bestPos);
    std::string snippet = content.substr(bestPos, len);
    if (bestPos > 0) snippet = "..." + snippet;
    if (bestPos + len < content.size()) snippet += "...";
    return snippet;
}

std::string escapeXmlAttr(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

}

std::vector<std::string> ContextBrain::extractWikiLinks(const std::string& text) const {
    std::vector<std::string> links;
    std::set<std::string> seen;
    size_t pos = 0;
    while ((pos = text.find("[[", pos)) != std::string::npos) {
        size_t end = text.find("]]", pos + 2);
        if (end == std::string::npos) break;
        std::string inner = text.substr(pos + 2, end - (pos + 2));
        size_t pipe = inner.find('|');
        if (pipe != std::string::npos) {
            inner = inner.substr(0, pipe);
        }
        size_t hash = inner.find('#');
        if (hash != std::string::npos) {
            inner = inner.substr(0, hash);
        }
        size_t a = inner.find_first_not_of(" \t\r\n");
        size_t b = inner.find_last_not_of(" \t\r\n");
        if (a != std::string::npos && b != std::string::npos) {
            std::string cleaned = inner.substr(a, b - a + 1);
            if (!cleaned.empty() && seen.insert(cleaned).second) {
                links.push_back(cleaned);
            }
        }
        pos = end + 2;
    }
    return links;
}

std::vector<std::string> ContextBrain::extractTags(const std::string& text) const {
    std::vector<std::string> tags;
    std::set<std::string> seen;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '#') continue;
        if (i > 0 && (std::isalnum(static_cast<unsigned char>(text[i - 1])) || text[i - 1] == '#')) {
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
            if (seen.insert(tag).second) {
                tags.push_back(tag);
            }
        }
        i = j;
    }
    return tags;
}

std::vector<std::string> ContextBrain::getBacklinks(const NoteManager& nm, const std::string& title) const {
    std::vector<std::string> result;
    if (title.empty()) return result;
    std::string targetLow = toLowerAscii(title);

    for (const auto& kv : nm.getAllNotes()) {
        const Note& note = kv.second;
        bool matched = false;
        for (const auto& l : note.links) {
            if (toLowerAscii(l) == targetLow) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            auto wikiLinks = extractWikiLinks(note.content);
            for (const auto& wl : wikiLinks) {
                if (toLowerAscii(wl) == targetLow) {
                    matched = true;
                    break;
                }
            }
        }
        if (matched && toLowerAscii(note.id) != targetLow) {
            result.push_back(note.id);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

ContextRetrieval ContextBrain::retrieve(const NoteManager& nm, const std::string& query, int maxTokens) const {
    ContextRetrieval out;
    out.maxTokens = std::max(128, maxTokens);
    out.method = "lexical-datalog";

    auto terms = tokenizeQuery(query);
    std::string queryLow = toLowerAscii(query);

    struct ScoredNote {
        const Note* note = nullptr;
        int score = 0;
    };
    std::vector<ScoredNote> candidates;

    const auto& allNotes = nm.getAllNotes();
    candidates.reserve(allNotes.size());

    for (const auto& kv : allNotes) {
        const Note& note = kv.second;
        std::string titleLow = toLowerAscii(note.title.empty() ? note.id : note.title);
        std::string idLow = toLowerAscii(note.id);
        std::string bodyLow = toLowerAscii(note.content);

        int score = 0;
        if (!queryLow.empty()) {
            if (titleLow == queryLow || idLow == queryLow) score += 80;
            else if (titleLow.find(queryLow) != std::string::npos) score += 40;
            if (bodyLow.find(queryLow) != std::string::npos) score += 20;
        }

        auto tags = extractTags(note.content);
        auto links = extractWikiLinks(note.content);

        for (const auto& t : terms) {
            if (titleLow.find(t) != std::string::npos) score += 16;
            if (idLow.find(t) != std::string::npos) score += 12;
            int bodyHits = countOccurrences(bodyLow, t);
            score += std::min(bodyHits, 10) * 4;
            for (const auto& tag : tags) {
                if (toLowerAscii(tag).find(t) != std::string::npos) score += 10;
            }
            for (const auto& link : links) {
                if (toLowerAscii(link).find(t) != std::string::npos) score += 6;
            }
        }

        if (terms.empty() && !note.content.empty()) {
            score = 1 + static_cast<int>(note.links.size());
        }

        if (score > 0) {
            auto backlinks = getBacklinks(nm, note.id);
            score += std::min<int>(static_cast<int>(backlinks.size()) * 2, 12);
            candidates.push_back({ &note, score });
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const ScoredNote& a, const ScoredNote& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.note->id < b.note->id;
    });

    std::ostringstream xml;
    xml << "<vault_context method=\"" << out.method << "\" max_tokens=\"" << out.maxTokens << "\">\n";

    int usedTokens = 16;
    for (const auto& c : candidates) {
        if (usedTokens >= out.maxTokens) break;
        int remainingChars = std::max(120, (out.maxTokens - usedTokens) * 4);
        size_t maxExcerptChars = static_cast<size_t>(std::min(remainingChars, 900));

        ContextHit hit;
        hit.noteId = c.note->id;
        hit.title = c.note->title.empty() ? c.note->id : c.note->title;
        hit.relevance = c.score;
        hit.excerpt = buildSnippet(c.note->content, terms, maxExcerptChars);
        hit.estimatedTokens = std::max(1, static_cast<int>((hit.excerpt.size() + hit.title.size() + 32) / 4));

        if (!out.hits.empty() && usedTokens + hit.estimatedTokens > out.maxTokens) {
            break;
        }

        usedTokens += hit.estimatedTokens;
        xml << "  <note id=\"" << escapeXmlAttr(hit.noteId)
            << "\" title=\"" << escapeXmlAttr(hit.title)
            << "\" relevance=\"" << hit.relevance << "\">\n"
            << hit.excerpt << "\n  </note>\n";
        out.hits.push_back(std::move(hit));
    }

    xml << "</vault_context>";
    out.estimatedTokens = usedTokens;
    out.formattedXml = xml.str();
    return out;
}

}
