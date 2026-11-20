#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace nodepanda {

class NoteManager;
class Graph;

struct QueryCondition {
    std::string field;
    std::string op;
    std::string value;
};

struct QuerySort {
    std::string field;
    bool ascending = true;
};

struct QueryResult {
    std::string noteId;
    std::string title;
    std::map<std::string, std::string> values;
};

struct QueryDefinition {
    std::vector<QueryCondition> conditions;
    std::string sortField;
    bool sortAscending = true;
    int limit = -1;
};

class QueryEngine {
public:
    QueryEngine() = default;
    ~QueryEngine() = default;

    std::vector<QueryResult> execute(
        const QueryDefinition& query,
        const NoteManager& nm,
        const Graph& graph
    ) const;

    QueryDefinition parse(const std::string& queryStr) const;

private:

    bool evaluateCondition(
        const QueryCondition& condition,
        const std::string& fieldValue
    ) const;

    std::string getFieldValue(
        const std::string& fieldName,
        const std::string& noteId,
        const NoteManager& nm,
        const Graph& graph
    ) const;

    int countWords(const std::string& text) const;

    std::string trim(const std::string& str) const;

    std::vector<std::string> split(
        const std::string& str,
        char delimiter
    ) const;

    std::string toUpper(const std::string& str) const;
};

}
