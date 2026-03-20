#pragma once
#include "note.h"
#include <unordered_map>
#include <functional>

namespace nodepanda {

class NoteManager {
public:
    void setNotesDirectory(const std::filesystem::path& dir);
    const std::filesystem::path& getNotesDirectory() const { return m_notesDir; }

    void scanDirectory();

    Note* createNote(const std::string& name, const std::string& subfolder = "");
    bool deleteNote(const std::string& id);
    bool renameNote(const std::string& oldId, const std::string& newId);

    bool createSubfolder(const std::string& name);
    std::vector<std::string> getSubfolders() const;

    Note* getNoteById(const std::string& id);
    const std::unordered_map<std::string, Note>& getAllNotes() const { return m_notes; }
    std::unordered_map<std::string, Note>& getAllNotes() { return m_notes; }

    std::vector<Note*> searchNotes(const std::string& query);

    void saveAllDirty();

    
    void rebuildAllLinks();
    
    std::string resolveAlias(const std::string& name) const;

    std::vector<std::string> getBacklinks(const std::string& targetNoteId) const;

    
    bool reloadNote(const std::string& relativePath);

private:
    std::filesystem::path m_notesDir;
    std::unordered_map<std::string, Note> m_notes;
    std::unordered_map<std::string, std::string> m_aliasMap; 
};

} 
