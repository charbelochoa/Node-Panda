#include "note_manager.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace nodepanda {

void NoteManager::setNotesDirectory(const std::filesystem::path& dir) {
    m_notesDir = dir;
    if (!std::filesystem::exists(m_notesDir)) {
        std::filesystem::create_directories(m_notesDir);
    }
}

void NoteManager::scanDirectory() {
    m_notes.clear();
    m_aliasMap.clear();
    if (!std::filesystem::exists(m_notesDir)) return;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(m_notesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            Note note;
            note.id = entry.path().stem().string();
            note.filepath = entry.path();
            note.loadFromFile();
            
            std::string aliasStr = note.getFrontmatter("aliases");
            if (!aliasStr.empty()) {
                std::istringstream as(aliasStr);
                std::string a;
                while (std::getline(as, a, ',')) {
                    size_t s = a.find_first_not_of(" \t");
                    size_t e = a.find_last_not_of(" \t");
                    if (s != std::string::npos) {
                        m_aliasMap[a.substr(s, e - s + 1)] = note.id;
                    }
                }
            }
            
            m_notes[note.id] = std::move(note);
        }
    }
}

Note* NoteManager::createNote(const std::string& name, const std::string& subfolder) {
    if (name.empty()) return nullptr;
    if (m_notes.find(name) != m_notes.end()) return nullptr; 

    Note note;
    note.id = name;
    
    if (!subfolder.empty()) {
        auto dir = m_notesDir / subfolder;
        std::filesystem::create_directories(dir);
        note.filepath = dir / (name + ".md");
    } else {
        note.filepath = m_notesDir / (name + ".md");
    }
    
    note.content = "# " + name + "\n\nEscribe aqui tu nota...\n";
    note.title = name;
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        struct tm tm_info;
#ifdef _WIN32
        localtime_s(&tm_info, &time);
#else
        localtime_r(&time, &tm_info);
#endif
        strftime(buf, sizeof(buf), "%Y-%m-%d", &tm_info);
        note.frontmatter["fecha"] = std::string(buf);
    }
    note.dirty = true;
    note.saveToFile();

    m_notes[name] = std::move(note);
    return &m_notes[name];
}

bool NoteManager::deleteNote(const std::string& id) {
    auto it = m_notes.find(id);
    if (it == m_notes.end()) return false;

    std::string aliasStr = it->second.getFrontmatter("aliases");
    if (!aliasStr.empty()) {
        std::istringstream as(aliasStr);
        std::string a;
        while (std::getline(as, a, ',')) {
            size_t s = a.find_first_not_of(" \t");
            size_t e = a.find_last_not_of(" \t");
            if (s != std::string::npos) m_aliasMap.erase(a.substr(s, e - s + 1));
        }
    }

    std::filesystem::remove(it->second.filepath);
    m_notes.erase(it);
    return true;
}

bool NoteManager::renameNote(const std::string& oldId, const std::string& newId) {
    if (newId.empty() || oldId == newId) return false;
    if (m_notes.find(newId) != m_notes.end()) return false;

    auto it = m_notes.find(oldId);
    if (it == m_notes.end()) return false;

    Note note = std::move(it->second);
    m_notes.erase(it);

    auto parentDir = note.filepath.parent_path();
    auto newPath = parentDir / (newId + ".md");
    std::filesystem::rename(note.filepath, newPath);

    note.id = newId;
    note.filepath = newPath;

    std::string oldLink = "[[" + oldId + "]]";
    std::string newLink = "[[" + newId + "]]";
    for (auto& [id, n] : m_notes) {
        size_t pos = 0;
        while ((pos = n.content.find(oldLink, pos)) != std::string::npos) {
            n.content.replace(pos, oldLink.length(), newLink);
            pos += newLink.length();
            n.dirty = true;
        }
    }

    m_notes[newId] = std::move(note);
    return true;
}

Note* NoteManager::getNoteById(const std::string& id) {
    auto it = m_notes.find(id);
    if (it != m_notes.end()) return &it->second;
    
    auto aliasIt = m_aliasMap.find(id);
    if (aliasIt != m_aliasMap.end()) {
        it = m_notes.find(aliasIt->second);
        if (it != m_notes.end()) return &it->second;
    }
    
    return nullptr;
}

std::vector<Note*> NoteManager::searchNotes(const std::string& query) {
    std::vector<Note*> results;
    if (query.empty()) {
        for (auto& [id, note] : m_notes) {
            results.push_back(&note);
        }
        return results;
    }

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (auto& [id, note] : m_notes) {
        std::string lowerId = id;
        std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
        std::string lowerContent = note.content;
        std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
        std::string lowerFrontmatter;
        for (const auto& [k, v] : note.frontmatter) {
            lowerFrontmatter += k + " " + v + " ";
        }
        std::transform(lowerFrontmatter.begin(), lowerFrontmatter.end(), lowerFrontmatter.begin(), ::tolower);

        if (lowerId.find(lowerQuery) != std::string::npos ||
            lowerContent.find(lowerQuery) != std::string::npos ||
            lowerFrontmatter.find(lowerQuery) != std::string::npos) {
            results.push_back(&note);
        }
    }
    return results;
}

bool NoteManager::createSubfolder(const std::string& name) {
    auto dir = m_notesDir / name;
    if (std::filesystem::exists(dir)) return false;
    return std::filesystem::create_directories(dir);
}

std::vector<std::string> NoteManager::getSubfolders() const {
    std::vector<std::string> folders;
    if (!std::filesystem::exists(m_notesDir)) return folders;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(m_notesDir)) {
        if (entry.is_directory()) {
            auto rel = std::filesystem::relative(entry.path(), m_notesDir);
            folders.push_back(rel.string());
        }
    }
    std::sort(folders.begin(), folders.end());
    return folders;
}

void NoteManager::saveAllDirty() {
    for (auto& [id, note] : m_notes) {
        if (note.dirty) {
            note.saveToFile();
        }
    }
}

void NoteManager::rebuildAllLinks() {
    for (auto& [id, note] : m_notes) {
        note.parseLinks();
    }
}

std::string NoteManager::resolveAlias(const std::string& name) const {
    auto it = m_aliasMap.find(name);
    if (it != m_aliasMap.end()) return it->second;
    return name;
}

std::vector<std::string> NoteManager::getBacklinks(const std::string& targetNoteId) const {
    std::vector<std::string> result;
    if (targetNoteId.empty()) return result;

    for (const auto& [noteId, note] : m_notes) {
        if (noteId == targetNoteId) continue;

        for (const auto& link : note.links) {
            std::string resolved = resolveAlias(link);
            if (resolved == targetNoteId || link == targetNoteId) {
                result.push_back(noteId);
                break; 
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

bool NoteManager::reloadNote(const std::string& relativePath) {
    std::filesystem::path fullPath = m_notesDir / relativePath;

    if (!std::filesystem::exists(fullPath)) {
        std::string noteId = fullPath.stem().string();
        auto it = m_notes.find(noteId);
        if (it != m_notes.end()) {
            m_notes.erase(it);
        }
        return true; 
    }

    std::string noteId = fullPath.stem().string();

    auto it = m_notes.find(noteId);
    if (it != m_notes.end()) {
        if (!it->second.dirty) {
            it->second.loadFromFile();
            return true;
        }
        return false; 
    } else {
        Note note;
        note.id = noteId;
        note.filepath = fullPath;
        if (note.loadFromFile()) {
            std::string aliasStr = note.getFrontmatter("aliases");
            if (!aliasStr.empty()) {
                std::istringstream as(aliasStr);
                std::string a;
                while (std::getline(as, a, ',')) {
                    size_t s = a.find_first_not_of(" \t");
                    size_t e = a.find_last_not_of(" \t");
                    if (s != std::string::npos) {
                        m_aliasMap[a.substr(s, e - s + 1)] = noteId;
                    }
                }
            }
            m_notes[noteId] = std::move(note);
            return true;
        }
        return false;
    }
}

} 
