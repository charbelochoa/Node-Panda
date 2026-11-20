#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace nodepanda {

class NoteManager;

enum class PropertyType {
    Text,
    Number,
    Date,
    Checkbox,
    Url,
    PageRef
};

enum class PropertyCardinality {
    One,
    Many
};

struct PropertySchema {
    std::string name;
    PropertyType type = PropertyType::Text;
    PropertyCardinality cardinality = PropertyCardinality::One;
    bool hidden = false;
    bool isPublic = true;
};

struct NamespaceNode {
    std::string segment;
    std::string fullPath;
    std::vector<std::string> noteIds;
    std::map<std::string, NamespaceNode> children;
};

class PropertyEngine {
public:
    void init(const std::filesystem::path& dataDir);
    void upsertSchema(const PropertySchema& schema);
    bool removeSchema(const std::string& name);
    const PropertySchema* getSchema(const std::string& name) const;
    std::vector<PropertySchema> listSchemas() const;

    bool validateValue(const std::string& propName, const std::string& value, std::string& errorOut) const;

    NamespaceNode buildNamespaceTree(const NoteManager& nm) const;
    std::vector<std::string> getNotesInNamespace(const NoteManager& nm, const std::string& nsPrefix) const;

    void registerTagExtends(const std::string& childTag, const std::string& parentTag);
    std::vector<std::string> getParentTags(const std::string& tag) const;

    static std::string typeToString(PropertyType t);
    static PropertyType stringToType(const std::string& s);

private:
    bool validateSingleValue(PropertyType type, const std::string& value, std::string& errorOut) const;
    void loadSchemas();
    void saveSchemas() const;
    void loadTagExtends();
    void saveTagExtends() const;
    std::vector<std::string> extractNamespaceSegments(const NoteManager& nm, const std::string& noteId) const;

    static std::string trim(const std::string& s);
    static std::string toLower(const std::string& s);
    static std::string normalizeTag(const std::string& tag);

    std::filesystem::path m_dataDir;
    std::filesystem::path m_schemaPath;
    std::filesystem::path m_extendsPath;
    std::map<std::string, PropertySchema> m_schemas;
    std::map<std::string, std::vector<std::string>> m_tagParents;
};

}
