#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace nodepanda {

class NoteManager;

struct Datom {
    std::string entity;
    std::string attribute;
    std::string value;
    int64_t tx = 0;
};

struct DatalogRow {
    std::map<std::string, std::string> bindings;
};

struct NamedQuery {
    std::string name;
    std::string description;
    std::string query;
};

class DatalogEngine {
public:
    DatalogEngine();

    void rebuildIndex(const NoteManager& nm);
    void assertFact(const std::string& entity, const std::string& attr, const std::string& value);
    void retractEntity(const std::string& entity);

    std::vector<DatalogRow> executeQuery(const std::string& queryStr, std::string& errorOut) const;
    std::vector<DatalogRow> runNamedQuery(const std::string& name, const std::string& arg, std::string& errorOut) const;

    const std::vector<NamedQuery>& getNamedQueries() const { return m_namedQueries; }
    void registerNamedQuery(const std::string& name, const std::string& desc, const std::string& query);

    size_t datomCount() const { return m_datoms.size(); }
    int64_t currentTx() const { return m_tx; }

private:
    struct TripleClause {
        std::string entity;
        std::string attribute;
        std::string value;
    };

    struct PredClause {
        std::string op;
        std::vector<std::string> args;
    };

    struct WhereClause {
        bool isPredicate = false;
        TripleClause triple;
        PredClause pred;
    };

    struct ParsedQuery {
        std::vector<std::string> findVars;
        std::vector<WhereClause> whereClauses;
    };

    void rebuildLookupIndices();
    void registerDefaultQueries();
    bool parseQuery(const std::string& queryStr, ParsedQuery& out, std::string& errorOut) const;
    bool evaluatePredicate(const PredClause& pred, const std::map<std::string, std::string>& env) const;

    static bool isVariable(const std::string& token);
    static bool isWildcard(const std::string& token);
    static std::string toLower(const std::string& s);
    static std::string trim(const std::string& s);
    static std::string escapeStringLiteral(const std::string& s);

    std::vector<Datom> m_datoms;
    std::unordered_map<std::string, std::vector<size_t>> m_byAttr;
    std::unordered_map<std::string, std::vector<size_t>> m_byEntity;
    std::vector<NamedQuery> m_namedQueries;
    int64_t m_tx = 0;
};

}
