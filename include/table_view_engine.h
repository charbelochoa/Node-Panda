#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace nodepanda {

class NoteManager;

struct TableColumnSpec {
    std::string key;
    std::string label;
    std::string type = "text";
    bool visible = true;
};

struct TableViewConfig {
    std::string id;
    std::string name;
    std::string filterTag;
    std::string filterType;
    std::string filterNamespace;
    std::string searchText;
    std::string sortColumn = "titulo";
    bool sortAscending = true;
    std::vector<TableColumnSpec> columns;
};

struct TableRowData {
    std::string noteId;
    std::string title;
    std::map<std::string, std::string> cells;
};

struct TableColumnAggregate {
    std::string key;
    int count = 0;
    int nonEmpty = 0;
    double sum = 0.0;
    double avg = 0.0;
    bool numeric = false;
};

class TableViewEngine {
public:
    void init(const std::filesystem::path& dataDir);
    const std::vector<TableViewConfig>& views() const;
    TableViewConfig* activeView();
    void setActiveView(const std::string& viewId);
    TableViewConfig& createView(const std::string& name, const std::string& filterTag = "", const std::string& filterType = "");
    bool deleteView(const std::string& viewId);

    std::vector<TableRowData> queryRows(const NoteManager& nm, const TableViewConfig& cfg) const;
    std::vector<TableColumnAggregate> computeAggregates(const std::vector<TableRowData>& rows, const TableViewConfig& cfg) const;
    bool updateCell(NoteManager& nm, const std::string& noteId, const std::string& columnKey, const std::string& newValue);

    std::string exportCsv(const std::vector<TableRowData>& rows, const TableViewConfig& cfg) const;
    std::string exportMarkdown(const std::vector<TableRowData>& rows, const TableViewConfig& cfg) const;

    void saveViews() const;
    void loadViews();

    static bool isComputedColumn(const std::string& columnKey);
    static bool parseCheckboxValue(const std::string& value);

private:
    void ensureDefaultViews();
    std::string generateViewId(const std::string& name) const;

    static std::string trim(const std::string& s);
    static std::string toLower(const std::string& s);
    static std::string escapeField(const std::string& s);
    static std::string unescapeField(const std::string& s);
    static std::string escapeCsvCell(const std::string& s);
    static std::string escapeMarkdownCell(const std::string& s);
    static bool tryParseNumber(const std::string& s, double& out);
    static std::vector<std::string> splitCommaList(const std::string& s);
    static std::vector<std::string> extractNoteTags(const NoteManager& nm, const std::string& noteId);
    static std::string extractNamespace(const NoteManager& nm, const std::string& noteId);
    static bool matchesTagFilter(const NoteManager& nm, const std::string& noteId, const std::string& filterTag);
    static bool matchesTypeFilter(const NoteManager& nm, const std::string& noteId, const std::string& filterType);
    static bool matchesNamespaceFilter(const NoteManager& nm, const std::string& noteId, const std::string& filterNs);
    static std::string resolveCellValue(const NoteManager& nm, const std::string& noteId, const std::string& columnKey);

    std::filesystem::path m_dataDir;
    std::filesystem::path m_viewsPath;
    std::vector<TableViewConfig> m_views;
    std::string m_activeViewId;
};

}
