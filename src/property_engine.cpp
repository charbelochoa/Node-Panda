#include "property_engine.h"
#include "note_manager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <queue>
#include <set>
#include <sstream>

namespace nodepanda {

std::string PropertyEngine::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string PropertyEngine::toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string PropertyEngine::normalizeTag(const std::string& tag) {
    std::string t = trim(tag);
    while (!t.empty() && t.front() == '#') {
        t.erase(0, 1);
    }
    return toLower(t);
}

std::string PropertyEngine::typeToString(PropertyType t) {
    switch (t) {
        case PropertyType::Text: return "Text";
        case PropertyType::Number: return "Number";
        case PropertyType::Date: return "Date";
        case PropertyType::Checkbox: return "Checkbox";
        case PropertyType::Url: return "Url";
        case PropertyType::PageRef: return "PageRef";
    }
    return "Text";
}

PropertyType PropertyEngine::stringToType(const std::string& s) {
    std::string lower = toLower(trim(s));
    if (lower == "number" || lower == "num" || lower == "float" || lower == "int") return PropertyType::Number;
    if (lower == "date" || lower == "datetime") return PropertyType::Date;
    if (lower == "checkbox" || lower == "bool" || lower == "boolean") return PropertyType::Checkbox;
    if (lower == "url" || lower == "link") return PropertyType::Url;
    if (lower == "pageref" || lower == "page" || lower == "ref") return PropertyType::PageRef;
    return PropertyType::Text;
}

void PropertyEngine::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    if (!m_dataDir.empty() && !std::filesystem::exists(m_dataDir)) {
        std::filesystem::create_directories(m_dataDir);
    }
    m_schemaPath = m_dataDir / "property_schemas.tsv";
    m_extendsPath = m_dataDir / "tag_extends.tsv";
    m_schemas.clear();
    m_tagParents.clear();

    loadSchemas();
    loadTagExtends();

    if (m_schemas.empty()) {
        m_schemas["title"] = {"title", PropertyType::Text, PropertyCardinality::One, false, true};
        m_schemas["type"] = {"type", PropertyType::Text, PropertyCardinality::One, false, true};
        m_schemas["tipo"] = {"tipo", PropertyType::Text, PropertyCardinality::One, false, true};
        m_schemas["tags"] = {"tags", PropertyType::Text, PropertyCardinality::Many, false, true};
        m_schemas["fecha"] = {"fecha", PropertyType::Date, PropertyCardinality::One, false, true};
        m_schemas["updated"] = {"updated", PropertyType::Date, PropertyCardinality::One, false, true};
        m_schemas["completed"] = {"completed", PropertyType::Checkbox, PropertyCardinality::One, false, true};
        m_schemas["url"] = {"url", PropertyType::Url, PropertyCardinality::One, false, true};
        m_schemas["related"] = {"related", PropertyType::PageRef, PropertyCardinality::Many, false, true};
        saveSchemas();
    }
}

void PropertyEngine::loadSchemas() {
    if (m_schemaPath.empty() || !std::filesystem::exists(m_schemaPath)) return;
    std::ifstream in(m_schemaPath);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string name, typeStr, cardStr, hiddenStr, pubStr;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, typeStr, '\t')) continue;
        if (!std::getline(ss, cardStr, '\t')) continue;
        if (!std::getline(ss, hiddenStr, '\t')) continue;
        std::getline(ss, pubStr, '\t');
        name = trim(name);
        if (name.empty()) continue;
        PropertySchema sc;
        sc.name = name;
        sc.type = stringToType(typeStr);
        sc.cardinality = (toLower(trim(cardStr)) == "many") ? PropertyCardinality::Many : PropertyCardinality::One;
        sc.hidden = (trim(hiddenStr) == "1" || toLower(trim(hiddenStr)) == "true");
        sc.isPublic = !(trim(pubStr) == "0" || toLower(trim(pubStr)) == "false");
        m_schemas[sc.name] = sc;
    }
}

void PropertyEngine::saveSchemas() const {
    if (m_schemaPath.empty()) return;
    std::ofstream out(m_schemaPath, std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& [name, sc] : m_schemas) {
        out << sc.name << '\t'
            << typeToString(sc.type) << '\t'
            << (sc.cardinality == PropertyCardinality::Many ? "many" : "one") << '\t'
            << (sc.hidden ? "1" : "0") << '\t'
            << (sc.isPublic ? "1" : "0") << '\n';
    }
}

void PropertyEngine::loadTagExtends() {
    if (m_extendsPath.empty() || !std::filesystem::exists(m_extendsPath)) return;
    std::ifstream in(m_extendsPath);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string child = normalizeTag(line.substr(0, tab));
        std::string parent = normalizeTag(line.substr(tab + 1));
        if (!child.empty() && !parent.empty() && child != parent) {
            auto& vec = m_tagParents[child];
            if (std::find(vec.begin(), vec.end(), parent) == vec.end()) {
                vec.push_back(parent);
            }
        }
    }
}

void PropertyEngine::saveTagExtends() const {
    if (m_extendsPath.empty()) return;
    std::ofstream out(m_extendsPath, std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& [child, parents] : m_tagParents) {
        for (const auto& parent : parents) {
            out << child << '\t' << parent << '\n';
        }
    }
}

void PropertyEngine::upsertSchema(const PropertySchema& schema) {
    if (schema.name.empty()) return;
    m_schemas[schema.name] = schema;
    saveSchemas();
}

bool PropertyEngine::removeSchema(const std::string& name) {
    auto it = m_schemas.find(name);
    if (it == m_schemas.end()) return false;
    m_schemas.erase(it);
    saveSchemas();
    return true;
}

const PropertySchema* PropertyEngine::getSchema(const std::string& name) const {
    auto it = m_schemas.find(name);
    if (it != m_schemas.end()) return &it->second;
    std::string lower = toLower(name);
    for (const auto& [k, v] : m_schemas) {
        if (toLower(k) == lower) return &v;
    }
    return nullptr;
}

std::vector<PropertySchema> PropertyEngine::listSchemas() const {
    std::vector<PropertySchema> result;
    result.reserve(m_schemas.size());
    for (const auto& [k, v] : m_schemas) {
        result.push_back(v);
    }
    return result;
}

bool PropertyEngine::validateSingleValue(PropertyType type, const std::string& raw, std::string& errorOut) const {
    std::string val = trim(raw);
    switch (type) {
        case PropertyType::Text:
            return true;
        case PropertyType::Number: {
            if (val.empty()) {
                errorOut = "Expected a numeric value";
                return false;
            }
            char* end = nullptr;
            double num = std::strtod(val.c_str(), &end);
            if (end == val.c_str() || *end != '\0' || !std::isfinite(num)) {
                errorOut = "Invalid number: " + val;
                return false;
            }
            return true;
        }
        case PropertyType::Date: {
            if (val.size() < 10 || val[4] != '-' || val[7] != '-') {
                errorOut = "Date must match YYYY-MM-DD";
                return false;
            }
            for (size_t idx : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(5), size_t(6), size_t(8), size_t(9)}) {
                if (!std::isdigit(static_cast<unsigned char>(val[idx]))) {
                    errorOut = "Date must match YYYY-MM-DD";
                    return false;
                }
            }
            int month = std::stoi(val.substr(5, 2));
            int day = std::stoi(val.substr(8, 2));
            if (month < 1 || month > 12 || day < 1 || day > 31) {
                errorOut = "Date month or day out of range";
                return false;
            }
            return true;
        }
        case PropertyType::Checkbox: {
            std::string lower = toLower(val);
            if (lower == "true" || lower == "false" || lower == "1" || lower == "0" ||
                lower == "yes" || lower == "no" || lower == "on" || lower == "off") {
                return true;
            }
            errorOut = "Checkbox must be true/false or 1/0";
            return false;
        }
        case PropertyType::Url: {
            std::string lower = toLower(val);
            static const std::vector<std::string> kSchemes = {
                "http://", "https://", "ftp://", "file://", "mailto:"
            };
            for (const auto& scheme : kSchemes) {
                if (lower.rfind(scheme, 0) == 0 && val.size() > scheme.size()) {
                    return true;
                }
            }
            errorOut = "URL must start with http://, https://, ftp://, file://, or mailto:";
            return false;
        }
        case PropertyType::PageRef: {
            if (val.empty()) {
                errorOut = "Page reference cannot be empty";
                return false;
            }
            if (val.size() >= 4 && val.rfind("[[", 0) == 0 && val.substr(val.size() - 2) == "]]") {
                std::string inner = trim(val.substr(2, val.size() - 4));
                if (inner.empty()) {
                    errorOut = "Empty wiki link in PageRef";
                    return false;
                }
                return true;
            }
            if (val.find("[[") != std::string::npos || val.find("]]") != std::string::npos) {
                errorOut = "Malformed wiki link brackets in PageRef";
                return false;
            }
            return true;
        }
    }
    return true;
}

bool PropertyEngine::validateValue(const std::string& propName, const std::string& value, std::string& errorOut) const {
    errorOut.clear();
    const PropertySchema* schema = getSchema(propName);
    if (!schema) return true;

    if (schema->cardinality == PropertyCardinality::Many) {
        std::string cleaned = trim(value);
        if (cleaned.size() >= 2 && cleaned.front() == '[' && cleaned.back() == ']') {
            cleaned = cleaned.substr(1, cleaned.size() - 2);
        }
        if (cleaned.empty()) return true;
        std::istringstream ss(cleaned);
        std::string item;
        while (std::getline(ss, item, ',')) {
            std::string trimmedItem = trim(item);
            if (trimmedItem.empty()) continue;
            if (!validateSingleValue(schema->type, trimmedItem, errorOut)) {
                return false;
            }
        }
        return true;
    }

    return validateSingleValue(schema->type, value, errorOut);
}

std::vector<std::string> PropertyEngine::extractNamespaceSegments(const NoteManager& nm, const std::string& noteId) const {
    std::vector<std::string> segs;
    const auto& notes = nm.getAllNotes();
    auto it = notes.find(noteId);
    if (it == notes.end()) return segs;
    const Note& note = it->second;

    std::string rawPath = note.getFrontmatter("namespace");
    if (rawPath.empty()) {
        if (noteId.find('/') != std::string::npos) {
            rawPath = noteId;
        } else if (note.title.find('/') != std::string::npos) {
            rawPath = note.title;
        } else if (!note.filepath.empty() && !nm.getNotesDirectory().empty()) {
            std::error_code ec;
            auto rel = std::filesystem::relative(note.filepath.parent_path(), nm.getNotesDirectory(), ec);
            if (!ec) {
                std::string relStr = rel.generic_string();
                if (!relStr.empty() && relStr != ".") {
                    rawPath = relStr;
                }
            }
        }
    }

    if (rawPath.empty()) return segs;
    std::replace(rawPath.begin(), rawPath.end(), '\\', '/');
    std::istringstream ss(rawPath);
    std::string seg;
    while (std::getline(ss, seg, '/')) {
        seg = trim(seg);
        if (!seg.empty() && seg != ".") {
            segs.push_back(seg);
        }
    }
    return segs;
}

NamespaceNode PropertyEngine::buildNamespaceTree(const NoteManager& nm) const {
    NamespaceNode root;
    root.segment = "";
    root.fullPath = "";

    const auto& notes = nm.getAllNotes();
    std::vector<std::string> sortedIds;
    sortedIds.reserve(notes.size());
    for (const auto& [id, note] : notes) {
        sortedIds.push_back(id);
    }
    std::sort(sortedIds.begin(), sortedIds.end());

    for (const auto& id : sortedIds) {
        auto segs = extractNamespaceSegments(nm, id);
        NamespaceNode* curr = &root;
        std::string accum;
        for (const auto& seg : segs) {
            if (!accum.empty()) accum += "/";
            accum += seg;
            auto& child = curr->children[seg];
            if (child.segment.empty()) {
                child.segment = seg;
                child.fullPath = accum;
            }
            curr = &child;
        }
        curr->noteIds.push_back(id);
    }
    return root;
}

std::vector<std::string> PropertyEngine::getNotesInNamespace(const NoteManager& nm, const std::string& nsPrefix) const {
    std::string prefix = trim(nsPrefix);
    std::replace(prefix.begin(), prefix.end(), '\\', '/');
    while (!prefix.empty() && prefix.front() == '/') prefix.erase(0, 1);
    while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();

    std::vector<std::string> result;
    const auto& notes = nm.getAllNotes();
    for (const auto& [id, note] : notes) {
        auto segs = extractNamespaceSegments(nm, id);
        std::string full;
        for (size_t i = 0; i < segs.size(); ++i) {
            if (i > 0) full += "/";
            full += segs[i];
        }
        if (prefix.empty()) {
            result.push_back(id);
        } else if (full == prefix || (full.size() > prefix.size() && full.rfind(prefix, 0) == 0 && full[prefix.size()] == '/')) {
            result.push_back(id);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void PropertyEngine::registerTagExtends(const std::string& childTag, const std::string& parentTag) {
    std::string child = normalizeTag(childTag);
    std::string parent = normalizeTag(parentTag);
    if (child.empty() || parent.empty() || child == parent) return;
    auto& vec = m_tagParents[child];
    if (std::find(vec.begin(), vec.end(), parent) == vec.end()) {
        vec.push_back(parent);
        saveTagExtends();
    }
}

std::vector<std::string> PropertyEngine::getParentTags(const std::string& tag) const {
    std::string start = normalizeTag(tag);
    std::vector<std::string> parents;
    if (start.empty()) return parents;

    std::set<std::string> visited;
    visited.insert(start);
    std::queue<std::string> q;
    q.push(start);

    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();
        auto it = m_tagParents.find(curr);
        if (it == m_tagParents.end()) continue;
        for (const auto& p : it->second) {
            if (visited.insert(p).second) {
                parents.push_back(p);
                q.push(p);
            }
        }
    }
    return parents;
}

}
