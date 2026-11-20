#include "table_view_engine.h"
#include "note_manager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace nodepanda {

std::string TableViewEngine::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string TableViewEngine::toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string TableViewEngine::escapeField(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '\t') out += "\\t";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out.push_back(c);
    }
    return out;
}

std::string TableViewEngine::unescapeField(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            if (next == '\\') { out.push_back('\\'); ++i; }
            else if (next == 't') { out.push_back('\t'); ++i; }
            else if (next == 'n') { out.push_back('\n'); ++i; }
            else if (next == 'r') { out.push_back('\r'); ++i; }
            else out.push_back(s[i]);
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string TableViewEngine::escapeCsvCell(const std::string& s) {
    bool needsQuotes = false;
    for (char c : s) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return s;
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string TableViewEngine::escapeMarkdownCell(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '|') out += "\\|";
        else if (c == '\n' || c == '\r') out.push_back(' ');
        else out.push_back(c);
    }
    return out;
}

bool TableViewEngine::tryParseNumber(const std::string& s, double& out) {
    std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    double val = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || *end != '\0' || !std::isfinite(val)) {
        return false;
    }
    out = val;
    return true;
}

bool TableViewEngine::parseCheckboxValue(const std::string& value) {
    std::string low = toLower(trim(value));
    return low == "true" || low == "1" || low == "yes" || low == "si" || low == "on" || low == "x";
}

bool TableViewEngine::isComputedColumn(const std::string& columnKey) {
    std::string k = toLower(trim(columnKey));
    return k == "id" || k == "palabras" || k == "words" ||
           k == "caracteres" || k == "chars" ||
           k == "enlaces" || k == "links";
}

std::vector<std::string> TableViewEngine::splitCommaList(const std::string& s) {
    std::vector<std::string> items;
    std::string cleaned = trim(s);
    if (cleaned.size() >= 2 && cleaned.front() == '[' && cleaned.back() == ']') {
        cleaned = cleaned.substr(1, cleaned.size() - 2);
    }
    std::istringstream ss(cleaned);
    std::string token;
    while (std::getline(ss, token, ',')) {
        std::string t = trim(token);
        if (!t.empty()) {
            items.push_back(t);
        }
    }
    return items;
}

std::vector<std::string> TableViewEngine::extractNoteTags(const NoteManager& nm, const std::string& noteId) {
    std::vector<std::string> tags;
    std::set<std::string> seen;
    const auto& allNotes = nm.getAllNotes();
    auto it = allNotes.find(noteId);
    if (it == allNotes.end()) return tags;
    const Note& note = it->second;

    auto addTag = [&](const std::string& raw) {
        std::string t = trim(raw);
        while (!t.empty() && (t.front() == '#' || t.front() == '"' || t.front() == '\'')) {
            t.erase(0, 1);
        }
        while (!t.empty() && (t.back() == '"' || t.back() == '\'')) {
            t.pop_back();
        }
        std::string low = toLower(trim(t));
        if (!low.empty() && seen.insert(low).second) {
            tags.push_back(low);
        }
    };

    for (const auto& [k, v] : note.frontmatter) {
        std::string lk = toLower(k);
        if (lk == "tags" || lk == "tag") {
            for (const auto& item : splitCommaList(v)) {
                std::istringstream ws(item);
                std::string part;
                while (ws >> part) {
                    addTag(part);
                }
            }
        }
    }

    const std::string& content = note.content;
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '#') {
            bool validBoundary = (i == 0 || std::isspace(static_cast<unsigned char>(content[i - 1])) ||
                                  content[i - 1] == '(' || content[i - 1] == '[' || content[i - 1] == ',');
            if (!validBoundary) continue;
            if (i + 1 < content.size() && content[i + 1] == '#') continue;
            if (i + 1 < content.size() && std::isspace(static_cast<unsigned char>(content[i + 1]))) continue;
            size_t j = i + 1;
            while (j < content.size()) {
                unsigned char ch = static_cast<unsigned char>(content[j]);
                if (std::isalnum(ch) || ch == '_' || ch == '-' || ch == '/') {
                    ++j;
                } else {
                    break;
                }
            }
            if (j > i + 1) {
                addTag(content.substr(i + 1, j - i - 1));
                i = j - 1;
            }
        }
    }

    return tags;
}

std::string TableViewEngine::extractNamespace(const NoteManager& nm, const std::string& noteId) {
    const auto& allNotes = nm.getAllNotes();
    auto it = allNotes.find(noteId);
    if (it == allNotes.end()) return "";
    const Note& note = it->second;

    std::string raw = note.getFrontmatter("namespace");
    if (raw.empty()) {
        for (const auto& [k, v] : note.frontmatter) {
            if (toLower(k) == "namespace") {
                raw = v;
                break;
            }
        }
    }

    if (raw.empty()) {
        if (noteId.find('/') != std::string::npos) {
            size_t pos = noteId.rfind('/');
            raw = noteId.substr(0, pos);
        } else if (!note.filepath.empty() && !nm.getNotesDirectory().empty()) {
            std::error_code ec;
            auto rel = std::filesystem::relative(note.filepath.parent_path(), nm.getNotesDirectory(), ec);
            if (!ec) {
                std::string relStr = rel.generic_string();
                if (!relStr.empty() && relStr != ".") {
                    raw = relStr;
                }
            }
        }
    }

    raw = trim(raw);
    std::replace(raw.begin(), raw.end(), '\\', '/');
    while (!raw.empty() && raw.front() == '/') raw.erase(0, 1);
    while (!raw.empty() && raw.back() == '/') raw.pop_back();
    return raw;
}

bool TableViewEngine::matchesTagFilter(const NoteManager& nm, const std::string& noteId, const std::string& filterTag) {
    std::string f = trim(filterTag);
    if (f.empty()) return true;

    std::vector<std::string> required = splitCommaList(f);
    if (required.empty()) return true;

    std::vector<std::string> noteTags = extractNoteTags(nm, noteId);
    for (const auto& reqRaw : required) {
        std::string req = trim(reqRaw);
        while (!req.empty() && req.front() == '#') req.erase(0, 1);
        req = toLower(trim(req));
        if (req.empty()) continue;

        bool matched = false;
        for (const auto& nt : noteTags) {
            if (nt == req || nt.rfind(req + "/", 0) == 0) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }
    return true;
}

bool TableViewEngine::matchesTypeFilter(const NoteManager& nm, const std::string& noteId, const std::string& filterType) {
    std::string f = trim(filterType);
    if (f.empty()) return true;

    const auto& allNotes = nm.getAllNotes();
    auto it = allNotes.find(noteId);
    if (it == allNotes.end()) return false;
    const Note& note = it->second;

    std::string noteType;
    for (const auto& [k, v] : note.frontmatter) {
        std::string lk = toLower(k);
        if (lk == "tipo" || lk == "type") {
            noteType = toLower(trim(v));
            if (!noteType.empty()) break;
        }
    }
    if (noteType.empty()) return false;

    std::vector<std::string> accepted = splitCommaList(f);
    for (const auto& acc : accepted) {
        std::string lowAcc = toLower(trim(acc));
        if (!lowAcc.empty() && (noteType == lowAcc || noteType.find(lowAcc) != std::string::npos)) {
            return true;
        }
    }
    return false;
}

bool TableViewEngine::matchesNamespaceFilter(const NoteManager& nm, const std::string& noteId, const std::string& filterNs) {
    std::string f = trim(filterNs);
    if (f.empty()) return true;

    std::replace(f.begin(), f.end(), '\\', '/');
    while (!f.empty() && f.front() == '/') f.erase(0, 1);
    while (!f.empty() && f.back() == '/') f.pop_back();
    std::string lowF = toLower(f);

    std::string ns = toLower(extractNamespace(nm, noteId));
    if (ns.empty()) return false;
    return ns == lowF || (ns.size() > lowF.size() && ns.rfind(lowF, 0) == 0 && ns[lowF.size()] == '/');
}

std::string TableViewEngine::resolveCellValue(const NoteManager& nm, const std::string& noteId, const std::string& columnKey) {
    const auto& allNotes = nm.getAllNotes();
    auto it = allNotes.find(noteId);
    if (it == allNotes.end()) return "";
    const Note& note = it->second;

    std::string lk = toLower(trim(columnKey));
    if (lk == "titulo" || lk == "title") {
        return note.title.empty() ? note.id : note.title;
    }
    if (lk == "id") {
        return note.id;
    }
    if (lk == "namespace") {
        return extractNamespace(nm, noteId);
    }
    if (lk == "palabras" || lk == "words") {
        std::istringstream iss(note.content);
        std::string w;
        int count = 0;
        while (iss >> w) ++count;
        return std::to_string(count);
    }
    if (lk == "caracteres" || lk == "chars") {
        return std::to_string(note.content.size());
    }
    if (lk == "enlaces" || lk == "links") {
        return std::to_string(note.links.size());
    }

    auto exact = note.frontmatter.find(columnKey);
    if (exact != note.frontmatter.end()) {
        return exact->second;
    }
    for (const auto& [k, v] : note.frontmatter) {
        if (toLower(k) == lk) {
            return v;
        }
    }

    if (lk == "tipo" || lk == "type") {
        for (const auto& [k, v] : note.frontmatter) {
            std::string fk = toLower(k);
            if (fk == "tipo" || fk == "type") {
                return v;
            }
        }
        return "";
    }

    if (lk == "tags" || lk == "tag") {
        std::vector<std::string> tags = extractNoteTags(nm, noteId);
        std::string joined;
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) joined += ", ";
            joined += tags[i];
        }
        return joined;
    }

    if (lk == "fecha" || lk == "date") {
        for (const auto& [k, v] : note.frontmatter) {
            std::string fk = toLower(k);
            if (fk == "fecha" || fk == "date") {
                return v;
            }
        }
        if (note.lastModified != std::chrono::system_clock::time_point{}) {
            std::time_t t = std::chrono::system_clock::to_time_t(note.lastModified);
            std::tm tmInfo = {};
#ifdef _WIN32
            localtime_s(&tmInfo, &t);
#else
            localtime_r(&t, &tmInfo);
#endif
            char buf[16];
            if (std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmInfo) > 0) {
                return std::string(buf);
            }
        }
        return "";
    }

    return "";
}

void TableViewEngine::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    if (!m_dataDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(m_dataDir, ec);
    }
    m_viewsPath = m_dataDir / "table_views.dat";
    m_views.clear();
    m_activeViewId.clear();

    loadViews();
    if (m_views.empty()) {
        ensureDefaultViews();
        saveViews();
    }

    bool activeFound = false;
    for (const auto& v : m_views) {
        if (v.id == m_activeViewId) {
            activeFound = true;
            break;
        }
    }
    if (!activeFound && !m_views.empty()) {
        m_activeViewId = m_views.front().id;
    }
}

void TableViewEngine::ensureDefaultViews() {
    m_views.clear();

    TableViewConfig allNotes;
    allNotes.id = "todas_las_notas";
    allNotes.name = "Todas las Notas";
    allNotes.sortColumn = "titulo";
    allNotes.sortAscending = true;
    allNotes.columns = {
        {"titulo", "Titulo", "text", true},
        {"tipo", "Tipo", "text", true},
        {"tags", "Tags", "text", true},
        {"estado", "Estado", "text", true},
        {"fecha", "Fecha", "date", true},
        {"enlaces", "Enlaces", "number", true},
        {"palabras", "Palabras", "number", true}
    };
    m_views.push_back(allNotes);

    TableViewConfig projects;
    projects.id = "proyectos";
    projects.name = "Proyectos";
    projects.filterType = "proyecto";
    projects.sortColumn = "prioridad";
    projects.sortAscending = true;
    projects.columns = {
        {"titulo", "Titulo", "text", true},
        {"estado", "Estado", "text", true},
        {"prioridad", "Prioridad", "text", true},
        {"progreso", "Progreso", "number", true},
        {"fecha", "Fecha", "date", true},
        {"completado", "Completado", "checkbox", true},
        {"tags", "Tags", "text", true}
    };
    m_views.push_back(projects);

    TableViewConfig concepts;
    concepts.id = "conceptos";
    concepts.name = "Conceptos";
    concepts.filterType = "concepto";
    concepts.sortColumn = "titulo";
    concepts.sortAscending = true;
    concepts.columns = {
        {"titulo", "Titulo", "text", true},
        {"categoria", "Categoria", "text", true},
        {"tags", "Tags", "text", true},
        {"enlaces", "Enlaces", "number", true},
        {"palabras", "Palabras", "number", true},
        {"fecha", "Fecha", "date", true}
    };
    m_views.push_back(concepts);

    TableViewConfig tasksAndJournal;
    tasksAndJournal.id = "tareas_y_diario";
    tasksAndJournal.name = "Tareas y Diario";
    tasksAndJournal.filterType = "tarea,journal,diario";
    tasksAndJournal.sortColumn = "fecha";
    tasksAndJournal.sortAscending = false;
    tasksAndJournal.columns = {
        {"titulo", "Titulo", "text", true},
        {"tipo", "Tipo", "text", true},
        {"estado", "Estado", "text", true},
        {"prioridad", "Prioridad", "text", true},
        {"fecha", "Fecha", "date", true},
        {"completado", "Completado", "checkbox", true},
        {"tags", "Tags", "text", true}
    };
    m_views.push_back(tasksAndJournal);

    m_activeViewId = m_views.front().id;
}

std::string TableViewEngine::generateViewId(const std::string& name) const {
    std::string base;
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            base.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == ' ' || c == '-' || c == '_') {
            if (!base.empty() && base.back() != '_') {
                base.push_back('_');
            }
        }
    }
    while (!base.empty() && base.back() == '_') base.pop_back();
    if (base.empty()) base = "vista";

    std::string candidate = base;
    int suffix = 2;
    while (true) {
        bool exists = false;
        for (const auto& v : m_views) {
            if (v.id == candidate) {
                exists = true;
                break;
            }
        }
        if (!exists) return candidate;
        candidate = base + "_" + std::to_string(suffix++);
    }
}

const std::vector<TableViewConfig>& TableViewEngine::views() const {
    return m_views;
}

TableViewConfig* TableViewEngine::activeView() {
    if (m_views.empty()) return nullptr;
    for (auto& v : m_views) {
        if (v.id == m_activeViewId) {
            return &v;
        }
    }
    m_activeViewId = m_views.front().id;
    return &m_views.front();
}

void TableViewEngine::setActiveView(const std::string& viewId) {
    for (const auto& v : m_views) {
        if (v.id == viewId) {
            m_activeViewId = viewId;
            saveViews();
            return;
        }
    }
}

TableViewConfig& TableViewEngine::createView(const std::string& name, const std::string& filterTag, const std::string& filterType) {
    std::string cleanName = trim(name);
    if (cleanName.empty()) cleanName = "Nueva Vista";

    TableViewConfig cfg;
    cfg.id = generateViewId(cleanName);
    cfg.name = cleanName;
    cfg.filterTag = trim(filterTag);
    cfg.filterType = trim(filterType);
    cfg.sortColumn = "titulo";
    cfg.sortAscending = true;
    cfg.columns = {
        {"titulo", "Titulo", "text", true},
        {"tipo", "Tipo", "text", true},
        {"tags", "Tags", "text", true},
        {"estado", "Estado", "text", true},
        {"fecha", "Fecha", "date", true},
        {"completado", "Completado", "checkbox", true}
    };

    m_views.push_back(cfg);
    m_activeViewId = m_views.back().id;
    saveViews();
    return m_views.back();
}

bool TableViewEngine::deleteView(const std::string& viewId) {
    for (auto it = m_views.begin(); it != m_views.end(); ++it) {
        if (it->id == viewId) {
            m_views.erase(it);
            if (m_activeViewId == viewId) {
                m_activeViewId = m_views.empty() ? "" : m_views.front().id;
            }
            saveViews();
            return true;
        }
    }
    return false;
}

std::vector<TableRowData> TableViewEngine::queryRows(const NoteManager& nm, const TableViewConfig& cfg) const {
    std::vector<TableRowData> rows;
    const auto& allNotes = nm.getAllNotes();
    std::string searchLower = toLower(trim(cfg.searchText));
    std::string sortCol = cfg.sortColumn.empty() ? "titulo" : cfg.sortColumn;

    for (const auto& [noteId, note] : allNotes) {
        if (!matchesTagFilter(nm, noteId, cfg.filterTag)) continue;
        if (!matchesTypeFilter(nm, noteId, cfg.filterType)) continue;
        if (!matchesNamespaceFilter(nm, noteId, cfg.filterNamespace)) continue;

        TableRowData row;
        row.noteId = noteId;
        row.title = note.title.empty() ? noteId : note.title;

        for (const auto& col : cfg.columns) {
            row.cells[col.key] = resolveCellValue(nm, noteId, col.key);
        }
        if (row.cells.find(sortCol) == row.cells.end()) {
            row.cells[sortCol] = resolveCellValue(nm, noteId, sortCol);
        }

        if (!searchLower.empty()) {
            bool found = false;
            if (toLower(row.noteId).find(searchLower) != std::string::npos ||
                toLower(row.title).find(searchLower) != std::string::npos ||
                toLower(note.content).find(searchLower) != std::string::npos) {
                found = true;
            }
            if (!found) {
                for (const auto& [ck, cv] : row.cells) {
                    if (toLower(cv).find(searchLower) != std::string::npos) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                for (const auto& [fk, fv] : note.frontmatter) {
                    if (toLower(fv).find(searchLower) != std::string::npos) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found) continue;
        }

        rows.push_back(std::move(row));
    }

    std::string sortType = "text";
    for (const auto& col : cfg.columns) {
        if (col.key == sortCol) {
            sortType = toLower(col.type);
            break;
        }
    }

    bool numericSort = (sortType == "number");
    if (!numericSort && sortType != "checkbox") {
        bool hasAnyNonEmpty = false;
        bool allNumeric = true;
        for (const auto& row : rows) {
            auto it = row.cells.find(sortCol);
            std::string val = (it != row.cells.end()) ? trim(it->second) : "";
            if (val.empty()) continue;
            hasAnyNonEmpty = true;
            double dummy = 0.0;
            if (!tryParseNumber(val, dummy)) {
                allNumeric = false;
                break;
            }
        }
        if (hasAnyNonEmpty && allNumeric) {
            numericSort = true;
        }
    }

    std::sort(rows.begin(), rows.end(), [&](const TableRowData& a, const TableRowData& b) {
        auto itA = a.cells.find(sortCol);
        auto itB = b.cells.find(sortCol);
        const std::string valA = (itA != a.cells.end()) ? itA->second : "";
        const std::string valB = (itB != b.cells.end()) ? itB->second : "";

        if (sortType == "checkbox") {
            bool ba = parseCheckboxValue(valA);
            bool bb = parseCheckboxValue(valB);
            if (ba != bb) {
                return cfg.sortAscending ? (!ba && bb) : (ba && !bb);
            }
            return a.noteId < b.noteId;
        }

        if (numericSort) {
            double na = 0.0;
            double nb = 0.0;
            bool okA = tryParseNumber(valA, na);
            bool okB = tryParseNumber(valB, nb);
            if (okA != okB) {
                return okA;
            }
            if (okA && okB && na != nb) {
                return cfg.sortAscending ? (na < nb) : (na > nb);
            }
            return a.noteId < b.noteId;
        }

        std::string la = toLower(trim(valA));
        std::string lb = toLower(trim(valB));
        if (la != lb) {
            if (la.empty() != lb.empty()) {
                return !la.empty();
            }
            return cfg.sortAscending ? (la < lb) : (la > lb);
        }
        return a.noteId < b.noteId;
    });

    return rows;
}

std::vector<TableColumnAggregate> TableViewEngine::computeAggregates(const std::vector<TableRowData>& rows, const TableViewConfig& cfg) const {
    std::vector<TableColumnAggregate> result;
    result.reserve(cfg.columns.size());

    for (const auto& col : cfg.columns) {
        TableColumnAggregate agg;
        agg.key = col.key;
        agg.count = static_cast<int>(rows.size());

        int numCount = 0;
        double numSum = 0.0;
        for (const auto& row : rows) {
            auto it = row.cells.find(col.key);
            if (it == row.cells.end()) continue;
            std::string val = trim(it->second);
            if (val.empty()) continue;
            agg.nonEmpty++;
            double num = 0.0;
            if (tryParseNumber(val, num)) {
                numSum += num;
                numCount++;
            }
        }

        std::string lowType = toLower(col.type);
        if (lowType == "number" || (numCount > 0 && numCount == agg.nonEmpty)) {
            agg.numeric = true;
            agg.sum = numSum;
            agg.avg = (numCount > 0) ? (numSum / static_cast<double>(numCount)) : 0.0;
        }
        result.push_back(agg);
    }

    return result;
}

bool TableViewEngine::updateCell(NoteManager& nm, const std::string& noteId, const std::string& columnKey, const std::string& newValue) {
    Note* note = nm.getNoteById(noteId);
    if (!note) return false;

    std::string cleanKey = trim(columnKey);
    if (cleanKey.empty() || isComputedColumn(cleanKey)) return false;

    std::string lk = toLower(cleanKey);
    if (lk == "titulo" || lk == "title") {
        std::string trimmedTitle = trim(newValue);
        if (trimmedTitle.empty()) trimmedTitle = note->id;
        note->title = trimmedTitle;

        std::istringstream iss(note->content);
        std::ostringstream oss;
        std::string line;
        bool replacedHeading = false;
        bool first = true;
        while (std::getline(iss, line)) {
            if (!first) oss << '\n';
            first = false;
            std::string stripped = line;
            if (!stripped.empty() && stripped.back() == '\r') stripped.pop_back();
            if (!replacedHeading && stripped.size() >= 2 && stripped[0] == '#' && stripped[1] == ' ') {
                oss << "# " << trimmedTitle;
                replacedHeading = true;
            } else {
                oss << stripped;
            }
        }
        if (replacedHeading) {
            if (!note->content.empty() && note->content.back() == '\n') {
                oss << '\n';
            }
            note->content = oss.str();
        } else {
            if (note->content.empty()) {
                note->content = "# " + trimmedTitle + "\n";
            } else {
                note->content = "# " + trimmedTitle + "\n\n" + note->content;
            }
        }
        note->dirty = true;
    } else {
        std::string targetKey = cleanKey;
        for (const auto& [k, v] : note->frontmatter) {
            if (toLower(k) == lk) {
                targetKey = k;
                break;
            }
        }
        if (trim(newValue).empty()) {
            note->frontmatter.erase(targetKey);
            note->dirty = true;
        } else {
            note->setFrontmatter(targetKey, trim(newValue));
        }
    }

    if (!note->filepath.empty()) {
        return note->saveToFile();
    }
    return true;
}

std::string TableViewEngine::exportCsv(const std::vector<TableRowData>& rows, const TableViewConfig& cfg) const {
    std::vector<const TableColumnSpec*> visibleCols;
    for (const auto& col : cfg.columns) {
        if (col.visible) visibleCols.push_back(&col);
    }
    if (visibleCols.empty()) return "";

    std::ostringstream oss;
    for (size_t i = 0; i < visibleCols.size(); ++i) {
        if (i > 0) oss << ',';
        const std::string& header = visibleCols[i]->label.empty() ? visibleCols[i]->key : visibleCols[i]->label;
        oss << escapeCsvCell(header);
    }
    oss << '\n';

    for (const auto& row : rows) {
        for (size_t i = 0; i < visibleCols.size(); ++i) {
            if (i > 0) oss << ',';
            auto it = row.cells.find(visibleCols[i]->key);
            std::string val = (it != row.cells.end()) ? it->second : "";
            oss << escapeCsvCell(val);
        }
        oss << '\n';
    }

    return oss.str();
}

std::string TableViewEngine::exportMarkdown(const std::vector<TableRowData>& rows, const TableViewConfig& cfg) const {
    std::vector<const TableColumnSpec*> visibleCols;
    for (const auto& col : cfg.columns) {
        if (col.visible) visibleCols.push_back(&col);
    }
    if (visibleCols.empty()) return "";

    std::ostringstream oss;
    oss << "| ";
    for (size_t i = 0; i < visibleCols.size(); ++i) {
        if (i > 0) oss << " | ";
        const std::string& header = visibleCols[i]->label.empty() ? visibleCols[i]->key : visibleCols[i]->label;
        oss << escapeMarkdownCell(header);
    }
    oss << " |\n| ";

    for (size_t i = 0; i < visibleCols.size(); ++i) {
        if (i > 0) oss << " | ";
        if (toLower(visibleCols[i]->type) == "number") oss << "---:";
        else oss << "---";
    }
    oss << " |\n";

    for (const auto& row : rows) {
        oss << "| ";
        for (size_t i = 0; i < visibleCols.size(); ++i) {
            if (i > 0) oss << " | ";
            auto it = row.cells.find(visibleCols[i]->key);
            std::string val = (it != row.cells.end()) ? it->second : "";
            oss << escapeMarkdownCell(val);
        }
        oss << " |\n";
    }

    return oss.str();
}

void TableViewEngine::saveViews() const {
    if (m_viewsPath.empty()) return;
    if (!m_dataDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(m_dataDir, ec);
    }
    std::ofstream out(m_viewsPath, std::ios::trunc);
    if (!out.is_open()) return;

    out << "ACTIVE\t" << escapeField(m_activeViewId) << '\n';
    for (const auto& v : m_views) {
        out << "VIEW\t"
            << escapeField(v.id) << '\t'
            << escapeField(v.name) << '\t'
            << escapeField(v.filterTag) << '\t'
            << escapeField(v.filterType) << '\t'
            << escapeField(v.filterNamespace) << '\t'
            << escapeField(v.searchText) << '\t'
            << escapeField(v.sortColumn) << '\t'
            << (v.sortAscending ? "1" : "0") << '\n';
        for (const auto& col : v.columns) {
            out << "COL\t"
                << escapeField(col.key) << '\t'
                << escapeField(col.label) << '\t'
                << escapeField(col.type) << '\t'
                << (col.visible ? "1" : "0") << '\n';
        }
    }
}

void TableViewEngine::loadViews() {
    if (m_viewsPath.empty() || !std::filesystem::exists(m_viewsPath)) return;
    std::ifstream in(m_viewsPath);
    if (!in.is_open()) return;

    m_views.clear();
    m_activeViewId.clear();

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> fields;
        std::istringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '\t')) {
            fields.push_back(unescapeField(field));
        }
        if (fields.empty()) continue;

        if (fields[0] == "ACTIVE" && fields.size() >= 2) {
            m_activeViewId = fields[1];
        } else if (fields[0] == "VIEW" && fields.size() >= 9) {
            TableViewConfig cfg;
            cfg.id = fields[1];
            cfg.name = fields[2];
            cfg.filterTag = fields[3];
            cfg.filterType = fields[4];
            cfg.filterNamespace = fields[5];
            cfg.searchText = fields[6];
            cfg.sortColumn = fields[7].empty() ? "titulo" : fields[7];
            cfg.sortAscending = (fields[8] != "0");
            if (!cfg.id.empty()) {
                m_views.push_back(std::move(cfg));
            }
        } else if (fields[0] == "COL" && fields.size() >= 5 && !m_views.empty()) {
            TableColumnSpec col;
            col.key = fields[1];
            col.label = fields[2].empty() ? fields[1] : fields[2];
            col.type = fields[3].empty() ? "text" : fields[3];
            col.visible = (fields[4] != "0");
            if (!col.key.empty()) {
                m_views.back().columns.push_back(std::move(col));
            }
        }
    }
}

}
