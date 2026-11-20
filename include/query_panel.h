#pragma once

#include <string>
#include <vector>
#include <utility>

namespace nodepanda {

class App;

class QueryPanel {
public:
    QueryPanel() = default;
    ~QueryPanel() = default;

    void render(App& app);

private:
    char m_queryBuffer[1024] = {};
    bool m_hasResults = false;
    std::vector<std::pair<std::string, std::string>> m_results;
    std::string m_errorMsg;
    int m_resultCount = 0;
    int m_selectedResultIndex = -1;

    void executeQuery(App& app);

    void displayExamples() const;

    std::string formatResultLine(const std::string& id, const std::string& title) const;
};

}
