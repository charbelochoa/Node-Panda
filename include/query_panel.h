#pragma once

#include "datalog_engine.h"
#include <string>
#include <utility>
#include <vector>

namespace nodepanda {

class App;

class QueryPanel {
public:
    QueryPanel() = default;
    ~QueryPanel() = default;

    void render(App& app);

private:
    int m_modeIdx = 0;
    char m_queryBuffer[1024] = "QUERY notas SORT fecha DESC LIMIT 20";
    char m_datalogBuffer[1024] = "[:find ?e ?t :where [?e :page/title ?t]]";
    bool m_hasResults = false;
    std::vector<std::pair<std::string, std::string>> m_results;
    std::vector<DatalogRow> m_datalogResults;
    std::string m_errorMsg;
    int m_resultCount = 0;
    int m_selectedResultIndex = -1;

    void executeQuery(App& app);
    void executeDatalog(App& app);
    void displayExamples() const;
    std::string formatResultLine(const std::string& id, const std::string& title) const;
};

}
