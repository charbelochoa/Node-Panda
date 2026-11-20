#include "datalog_engine.h"
#include "note_manager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <regex>
#include <set>
#include <sstream>

namespace nodepanda {

namespace {

enum class TokKind {
    LBracket,
    RBracket,
    LParen,
    RParen,
    String,
    Symbol
};

struct Token {
    TokKind kind = TokKind::Symbol;
    std::string text;
};

bool tokenizeDatalog(const std::string& input, std::vector<Token>& out, std::string& errorOut) {
    out.clear();
    size_t i = 0;
    const size_t n = input.size();
    while (i < n) {
        unsigned char ch = static_cast<unsigned char>(input[i]);
        if (std::isspace(ch) || ch == ',') {
            ++i;
            continue;
        }
        if (ch == '[') {
            out.push_back({TokKind::LBracket, "["});
            ++i;
            continue;
        }
        if (ch == ']') {
            out.push_back({TokKind::RBracket, "]"});
            ++i;
            continue;
        }
        if (ch == '(') {
            out.push_back({TokKind::LParen, "("});
            ++i;
            continue;
        }
        if (ch == ')') {
            out.push_back({TokKind::RParen, ")"});
            ++i;
            continue;
        }
        if (ch == '"') {
            ++i;
            std::string val;
            bool closed = false;
            while (i < n) {
                if (input[i] == '\\' && i + 1 < n) {
                    char next = input[i + 1];
                    if (next == 'n') val.push_back('\n');
                    else if (next == 't') val.push_back('\t');
                    else val.push_back(next);
                    i += 2;
                } else if (input[i] == '"') {
                    ++i;
                    closed = true;
                    break;
                } else {
                    val.push_back(input[i]);
                    ++i;
                }
            }
            if (!closed) {
                errorOut = "Unterminated string literal in query";
                return false;
            }
            out.push_back({TokKind::String, val});
            continue;
        }

        size_t start = i;
        while (i < n) {
            unsigned char c = static_cast<unsigned char>(input[i]);
            if (std::isspace(c) || c == '[' || c == ']' || c == '(' || c == ')' || c == ',' || c == '"') {
                break;
            }
            ++i;
        }
        out.push_back({TokKind::Symbol, input.substr(start, i - start)});
    }
    return true;
}

std::string formatTimePointDate(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) {
        return "";
    }
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tmInfo{};
#ifdef _WIN32
    localtime_s(&tmInfo, &t);
#else
    localtime_r(&t, &tmInfo);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmInfo);
    return std::string(buf);
}

bool tryParseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

}

DatalogEngine::DatalogEngine() {
    registerDefaultQueries();
}

void DatalogEngine::registerDefaultQueries() {
    registerNamedQuery(
        "block-search",
        "Search block lines containing a substring",
        "[:find ?e ?text :where [?e :block/text ?text] [(contains ?text \"{{arg}}\")]]"
    );
    registerNamedQuery(
        "task-search",
        "Search task pages and lines containing a substring",
        "[:find ?e ?status ?text :where [?e :task/status ?status] [?e :block/text ?text] [(contains ?text \"{{arg}}\")]]"
    );
    registerNamedQuery(
        "recent-updated",
        "List pages ordered by most recent update timestamp",
        "[:find ?e ?title ?updated :where [?e :page/title ?title] [?e :page/updated ?updated]]"
    );
    registerNamedQuery(
        "list-status",
        "List pages matching a task status",
        "[:find ?e ?title ?status :where [?e :task/status ?status] [?e :page/title ?title] [(contains ?status \"{{arg}}\")]]"
    );
    registerNamedQuery(
        "list-priority",
        "List pages matching a task priority",
        "[:find ?e ?title ?priority :where [?e :task/priority ?priority] [?e :page/title ?title] [(contains ?priority \"{{arg}}\")]]"
    );
    registerNamedQuery(
        "all-pages",
        "List all indexed pages with title and type",
        "[:find ?e ?title ?type :where [?e :page/title ?title] [?e :page/type ?type]]"
    );
    registerNamedQuery(
        "orphan-pages",
        "List pages without incoming or outgoing links",
        "[:find ?e ?title :where [?e :page/orphan \"true\"] [?e :page/title ?title]]"
    );
}

void DatalogEngine::registerNamedQuery(const std::string& name, const std::string& desc, const std::string& query) {
    for (auto& nq : m_namedQueries) {
        if (nq.name == name) {
            nq.description = desc;
            nq.query = query;
            return;
        }
    }
    m_namedQueries.push_back({name, desc, query});
}

void DatalogEngine::rebuildLookupIndices() {
    m_byAttr.clear();
    m_byEntity.clear();
    for (size_t i = 0; i < m_datoms.size(); ++i) {
        m_byAttr[m_datoms[i].attribute].push_back(i);
        m_byEntity[m_datoms[i].entity].push_back(i);
    }
}

void DatalogEngine::assertFact(const std::string& entity, const std::string& attr, const std::string& value) {
    ++m_tx;
    size_t idx = m_datoms.size();
    m_datoms.push_back({entity, attr, value, m_tx});
    m_byAttr[attr].push_back(idx);
    m_byEntity[entity].push_back(idx);
}

void DatalogEngine::retractEntity(const std::string& entity) {
    auto it = std::remove_if(m_datoms.begin(), m_datoms.end(), [&](const Datom& d) {
        return d.entity == entity;
    });
    if (it != m_datoms.end()) {
        m_datoms.erase(it, m_datoms.end());
        ++m_tx;
        rebuildLookupIndices();
    }
}

void DatalogEngine::rebuildIndex(const NoteManager& nm) {
    m_datoms.clear();
    m_byAttr.clear();
    m_byEntity.clear();
    ++m_tx;

    const auto& notes = nm.getAllNotes();
    std::vector<std::string> sortedIds;
    sortedIds.reserve(notes.size());
    for (const auto& [id, note] : notes) {
        sortedIds.push_back(id);
    }
    std::sort(sortedIds.begin(), sortedIds.end());

    static const std::regex kInlineTagRegex(R"((?:^|[\s(])#([A-Za-z0-9_\-\/]+))");
    static const std::regex kPriorityRegex(R"(\[#([A-Ca-c])\])");

    for (const auto& id : sortedIds) {
        const Note& note = notes.at(id);
        std::string title = note.title.empty() ? note.id : note.title;
        assertFact(id, ":page/title", title);

        std::string pageType = note.getFrontmatter("tipo");
        if (pageType.empty()) pageType = note.getFrontmatter("type");
        if (pageType.empty()) pageType = "nota";
        assertFact(id, ":page/type", pageType);

        std::string updated = note.getFrontmatter("updated");
        if (updated.empty()) updated = note.getFrontmatter("fecha");
        if (updated.empty()) updated = formatTimePointDate(note.lastModified);
        assertFact(id, ":page/updated", updated);

        std::set<std::string> tags;
        auto addTagList = [&](const std::string& raw) {
            std::string cleaned = raw;
            cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '['), cleaned.end());
            cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), ']'), cleaned.end());
            std::istringstream ss(cleaned);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                std::istringstream ws(tok);
                std::string part;
                while (ws >> part) {
                    if (!part.empty() && part[0] == '#') part.erase(0, 1);
                    part = trim(part);
                    if (!part.empty()) tags.insert(part);
                }
            }
        };
        addTagList(note.getFrontmatter("tags"));
        addTagList(note.getFrontmatter("etiquetas"));

        for (auto it = std::sregex_iterator(note.content.begin(), note.content.end(), kInlineTagRegex);
             it != std::sregex_iterator(); ++it) {
            std::string t = (*it)[1].str();
            if (!t.empty()) tags.insert(t);
        }
        for (const auto& tag : tags) {
            assertFact(id, ":page/tag", tag);
        }

        for (const auto& link : note.links) {
            assertFact(id, ":page/link", link);
        }

        auto backlinks = nm.getBacklinks(id);
        if (note.links.empty() && backlinks.empty()) {
            assertFact(id, ":page/orphan", "true");
        } else {
            assertFact(id, ":page/orphan", "false");
        }

        for (const auto& [key, val] : note.frontmatter) {
            assertFact(id, ":page/prop/" + key, val);
        }

        std::set<std::string> statuses;
        std::set<std::string> priorities;

        std::string fmStatus = note.getFrontmatter("status");
        if (fmStatus.empty()) fmStatus = note.getFrontmatter("estado");
        if (!fmStatus.empty()) statuses.insert(fmStatus);

        std::string fmPrio = note.getFrontmatter("priority");
        if (fmPrio.empty()) fmPrio = note.getFrontmatter("prioridad");
        if (!fmPrio.empty()) priorities.insert(fmPrio);

        std::istringstream lineStream(note.content);
        std::string line;
        while (std::getline(lineStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::string trimmed = trim(line);
            if (trimmed.empty()) continue;

            assertFact(id, ":block/text", trimmed);

            std::string stripBullet = trimmed;
            if (stripBullet.size() >= 2 && (stripBullet[0] == '-' || stripBullet[0] == '*' || stripBullet[0] == '+') && stripBullet[1] == ' ') {
                stripBullet = trim(stripBullet.substr(2));
            }

            if (stripBullet.rfind("[ ]", 0) == 0) {
                statuses.insert("TODO");
            } else if (stripBullet.rfind("[x]", 0) == 0 || stripBullet.rfind("[X]", 0) == 0) {
                statuses.insert("DONE");
            }

            static const std::vector<std::string> kMarkers = {
                "TODO", "DOING", "DONE", "LATER", "NOW", "WAITING", "CANCELED", "CANCELLED"
            };
            for (const auto& marker : kMarkers) {
                if (stripBullet == marker ||
                    (stripBullet.size() > marker.size() &&
                     stripBullet.rfind(marker, 0) == 0 &&
                     std::isspace(static_cast<unsigned char>(stripBullet[marker.size()])))) {
                    statuses.insert(marker);
                    break;
                }
            }

            std::smatch pm;
            if (std::regex_search(trimmed, pm, kPriorityRegex)) {
                std::string p = pm[1].str();
                std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) {
                    return static_cast<char>(std::toupper(c));
                });
                priorities.insert(p);
            }
        }

        for (const auto& s : statuses) {
            assertFact(id, ":task/status", s);
        }
        for (const auto& p : priorities) {
            assertFact(id, ":task/priority", p);
        }
    }
}

bool DatalogEngine::isVariable(const std::string& token) {
    return token.size() >= 2 && token[0] == '?';
}

bool DatalogEngine::isWildcard(const std::string& token) {
    return token == "_";
}

std::string DatalogEngine::toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string DatalogEngine::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string DatalogEngine::escapeStringLiteral(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\' || c == '"') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

bool DatalogEngine::parseQuery(const std::string& queryStr, ParsedQuery& out, std::string& errorOut) const {
    out = ParsedQuery{};
    std::vector<Token> tokens;
    if (!tokenizeDatalog(queryStr, tokens, errorOut)) {
        return false;
    }
    if (tokens.empty()) {
        errorOut = "Empty Datalog query";
        return false;
    }

    size_t pos = 0;
    bool wrappedInBrackets = false;
    if (tokens[pos].kind == TokKind::LBracket) {
        wrappedInBrackets = true;
        ++pos;
    }

    bool seenFind = false;
    bool seenWhere = false;

    while (pos < tokens.size()) {
        if (wrappedInBrackets && tokens[pos].kind == TokKind::RBracket) {
            ++pos;
            break;
        }
        if (tokens[pos].kind == TokKind::Symbol && tokens[pos].text == ":find") {
            seenFind = true;
            ++pos;
            while (pos < tokens.size() && tokens[pos].kind == TokKind::Symbol &&
                   tokens[pos].text[0] != ':') {
                if (!isVariable(tokens[pos].text)) {
                    errorOut = "Expected variable in :find clause, got: " + tokens[pos].text;
                    return false;
                }
                out.findVars.push_back(tokens[pos].text);
                ++pos;
            }
            continue;
        }
        if (tokens[pos].kind == TokKind::Symbol && tokens[pos].text == ":where") {
            seenWhere = true;
            ++pos;
            while (pos < tokens.size()) {
                if (tokens[pos].kind == TokKind::RBracket) {
                    break;
                }
                if (tokens[pos].kind == TokKind::Symbol && tokens[pos].text[0] == ':') {
                    break;
                }
                if (tokens[pos].kind != TokKind::LBracket) {
                    errorOut = "Expected '[' to start :where clause";
                    return false;
                }
                ++pos;
                if (pos >= tokens.size()) {
                    errorOut = "Unexpected end of query inside :where clause";
                    return false;
                }

                if (tokens[pos].kind == TokKind::LParen) {
                    ++pos;
                    if (pos >= tokens.size() || tokens[pos].kind != TokKind::Symbol) {
                        errorOut = "Expected predicate operator after '('";
                        return false;
                    }
                    WhereClause wc;
                    wc.isPredicate = true;
                    wc.pred.op = tokens[pos].text;
                    ++pos;
                    while (pos < tokens.size() && tokens[pos].kind != TokKind::RParen) {
                        if (tokens[pos].kind == TokKind::String) {
                            wc.pred.args.push_back("\"" + tokens[pos].text + "\"");
                        } else if (tokens[pos].kind == TokKind::Symbol) {
                            wc.pred.args.push_back(tokens[pos].text);
                        } else {
                            errorOut = "Unexpected token inside predicate clause";
                            return false;
                        }
                        ++pos;
                    }
                    if (pos >= tokens.size() || tokens[pos].kind != TokKind::RParen) {
                        errorOut = "Expected ')' to close predicate";
                        return false;
                    }
                    ++pos;
                    if (pos >= tokens.size() || tokens[pos].kind != TokKind::RBracket) {
                        errorOut = "Expected ']' after predicate clause";
                        return false;
                    }
                    ++pos;
                    out.whereClauses.push_back(std::move(wc));
                } else {
                    std::vector<std::string> parts;
                    while (pos < tokens.size() && tokens[pos].kind != TokKind::RBracket) {
                        if (tokens[pos].kind == TokKind::String) {
                            parts.push_back("\"" + tokens[pos].text + "\"");
                        } else if (tokens[pos].kind == TokKind::Symbol) {
                            parts.push_back(tokens[pos].text);
                        } else {
                            errorOut = "Unexpected token in triple clause";
                            return false;
                        }
                        ++pos;
                    }
                    if (pos >= tokens.size() || tokens[pos].kind != TokKind::RBracket) {
                        errorOut = "Expected ']' to close triple clause";
                        return false;
                    }
                    ++pos;
                    if (parts.size() != 3) {
                        errorOut = "Triple clause must have 3 elements [entity attribute value]";
                        return false;
                    }
                    WhereClause wc;
                    wc.isPredicate = false;
                    wc.triple = {parts[0], parts[1], parts[2]};
                    out.whereClauses.push_back(std::move(wc));
                }
            }
            continue;
        }
        errorOut = "Unexpected token in Datalog query: " + tokens[pos].text;
        return false;
    }

    if (!seenFind || out.findVars.empty()) {
        errorOut = "Query must contain a :find clause with at least one variable";
        return false;
    }
    if (!seenWhere || out.whereClauses.empty()) {
        errorOut = "Query must contain a :where clause with at least one pattern";
        return false;
    }
    return true;
}

bool DatalogEngine::evaluatePredicate(const PredClause& pred, const std::map<std::string, std::string>& env) const {
    auto resolveArg = [&](const std::string& raw, std::string& outVal) -> bool {
        if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
            outVal = raw.substr(1, raw.size() - 2);
            return true;
        }
        if (isVariable(raw)) {
            auto it = env.find(raw);
            if (it == env.end()) return false;
            outVal = it->second;
            return true;
        }
        outVal = raw;
        return true;
    };

    const std::string op = toLower(pred.op);
    if (op == "empty?" && pred.args.size() == 1) {
        std::string a;
        if (!resolveArg(pred.args[0], a)) return false;
        return a.empty();
    }
    if (pred.args.size() < 2) return false;

    std::string lhs;
    std::string rhs;
    if (!resolveArg(pred.args[0], lhs) || !resolveArg(pred.args[1], rhs)) {
        return false;
    }

    if (op == "contains" || op == "includes?" || op == "str/includes?") {
        if (rhs.empty()) return true;
        return toLower(lhs).find(toLower(rhs)) != std::string::npos;
    }
    if (op == "starts-with" || op == "str/starts-with?") {
        std::string ll = toLower(lhs);
        std::string lr = toLower(rhs);
        return ll.rfind(lr, 0) == 0;
    }
    if (op == "=" || op == "==") {
        return lhs == rhs;
    }
    if (op == "!=" || op == "not=") {
        return lhs != rhs;
    }

    double da = 0.0;
    double db = 0.0;
    bool bothNum = tryParseDouble(lhs, da) && tryParseDouble(rhs, db);
    if (op == "<") return bothNum ? (da < db) : (lhs < rhs);
    if (op == "<=") return bothNum ? (da <= db) : (lhs <= rhs);
    if (op == ">") return bothNum ? (da > db) : (lhs > rhs);
    if (op == ">=") return bothNum ? (da >= db) : (lhs >= rhs);
    return false;
}

std::vector<DatalogRow> DatalogEngine::executeQuery(const std::string& queryStr, std::string& errorOut) const {
    errorOut.clear();
    ParsedQuery pq;
    if (!parseQuery(queryStr, pq, errorOut)) {
        return {};
    }

    auto unquote = [](const std::string& s) -> std::string {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    };

    std::vector<std::map<std::string, std::string>> envs;
    envs.emplace_back();

    for (const auto& clause : pq.whereClauses) {
        if (envs.empty()) break;

        if (clause.isPredicate) {
            std::vector<std::map<std::string, std::string>> nextEnvs;
            for (const auto& env : envs) {
                if (evaluatePredicate(clause.pred, env)) {
                    nextEnvs.push_back(env);
                }
            }
            envs = std::move(nextEnvs);
            continue;
        }

        const auto& tr = clause.triple;
        std::vector<std::map<std::string, std::string>> nextEnvs;

        for (const auto& env : envs) {
            bool eBound = false;
            bool aBound = false;
            bool vBound = false;
            std::string eVal;
            std::string aVal;
            std::string vVal;

            if (!isVariable(tr.entity) && !isWildcard(tr.entity)) {
                eBound = true;
                eVal = unquote(tr.entity);
            } else if (isVariable(tr.entity)) {
                auto it = env.find(tr.entity);
                if (it != env.end()) {
                    eBound = true;
                    eVal = it->second;
                }
            }

            if (!isVariable(tr.attribute) && !isWildcard(tr.attribute)) {
                aBound = true;
                aVal = unquote(tr.attribute);
            } else if (isVariable(tr.attribute)) {
                auto it = env.find(tr.attribute);
                if (it != env.end()) {
                    aBound = true;
                    aVal = it->second;
                }
            }

            if (!isVariable(tr.value) && !isWildcard(tr.value)) {
                vBound = true;
                vVal = unquote(tr.value);
            } else if (isVariable(tr.value)) {
                auto it = env.find(tr.value);
                if (it != env.end()) {
                    vBound = true;
                    vVal = it->second;
                }
            }

            auto tryMatch = [&](const Datom& d) {
                if (eBound && d.entity != eVal) return;
                if (aBound && d.attribute != aVal) return;
                if (vBound && d.value != vVal) return;

                std::map<std::string, std::string> extended = env;
                if (isVariable(tr.entity)) {
                    auto it = extended.find(tr.entity);
                    if (it != extended.end() && it->second != d.entity) return;
                    extended[tr.entity] = d.entity;
                }
                if (isVariable(tr.attribute)) {
                    auto it = extended.find(tr.attribute);
                    if (it != extended.end() && it->second != d.attribute) return;
                    extended[tr.attribute] = d.attribute;
                }
                if (isVariable(tr.value)) {
                    auto it = extended.find(tr.value);
                    if (it != extended.end() && it->second != d.value) return;
                    extended[tr.value] = d.value;
                }
                nextEnvs.push_back(std::move(extended));
            };

            if (eBound) {
                auto it = m_byEntity.find(eVal);
                if (it != m_byEntity.end()) {
                    for (size_t idx : it->second) {
                        tryMatch(m_datoms[idx]);
                    }
                }
            } else if (aBound) {
                auto it = m_byAttr.find(aVal);
                if (it != m_byAttr.end()) {
                    for (size_t idx : it->second) {
                        tryMatch(m_datoms[idx]);
                    }
                }
            } else {
                for (const auto& d : m_datoms) {
                    tryMatch(d);
                }
            }
        }
        envs = std::move(nextEnvs);
    }

    std::vector<DatalogRow> results;
    std::set<std::vector<std::string>> seen;
    for (const auto& env : envs) {
        DatalogRow row;
        std::vector<std::string> key;
        key.reserve(pq.findVars.size());
        for (const auto& var : pq.findVars) {
            auto it = env.find(var);
            std::string val = (it != env.end()) ? it->second : "";
            row.bindings[var] = val;
            key.push_back(val);
        }
        if (seen.insert(key).second) {
            results.push_back(std::move(row));
        }
    }
    return results;
}

std::vector<DatalogRow> DatalogEngine::runNamedQuery(const std::string& name, const std::string& arg, std::string& errorOut) const {
    const NamedQuery* found = nullptr;
    for (const auto& nq : m_namedQueries) {
        if (nq.name == name) {
            found = &nq;
            break;
        }
    }
    if (!found) {
        errorOut = "Unknown named query: " + name;
        return {};
    }

    std::string q = found->query;
    std::string escapedArg = escapeStringLiteral(arg);
    size_t pos = 0;
    while ((pos = q.find("{{arg}}", pos)) != std::string::npos) {
        q.replace(pos, 7, escapedArg);
        pos += escapedArg.size();
    }

    auto rows = executeQuery(q, errorOut);
    if (name == "recent-updated" && errorOut.empty()) {
        std::sort(rows.begin(), rows.end(), [](const DatalogRow& a, const DatalogRow& b) {
            auto ia = a.bindings.find("?updated");
            auto ib = b.bindings.find("?updated");
            std::string va = (ia != a.bindings.end()) ? ia->second : "";
            std::string vb = (ib != b.bindings.end()) ? ib->second : "";
            if (va != vb) return va > vb;
            auto ea = a.bindings.find("?e");
            auto eb = b.bindings.find("?e");
            return (ea != a.bindings.end() ? ea->second : "") < (eb != b.bindings.end() ? eb->second : "");
        });
    }
    return rows;
}

}
