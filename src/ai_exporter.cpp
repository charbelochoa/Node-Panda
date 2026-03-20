

#include "ai_exporter.h"
#include "note_manager.h"
#include "note.h"

#include <queue>
#include <set>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace nodepanda {


std::vector<std::string> GetContextNoteIds(
    const std::string& startNoteId,
    int                depth,
    NoteManager&       noteManager)
{
    std::vector<std::string> result;
    if (startNoteId.empty()) return result;

    const std::string resolvedStart = noteManager.resolveAlias(startNoteId);
    if (!noteManager.getNoteById(resolvedStart)) return result; 

    
    std::queue<std::pair<std::string, int>> bfsQueue;
    std::set<std::string>                   visited;

    bfsQueue.push({resolvedStart, 0});
    visited.insert(resolvedStart);

    while (!bfsQueue.empty()) {
        auto [currentId, currentDepth] = bfsQueue.front();
        bfsQueue.pop();

        
        result.push_back(currentId);

        if (currentDepth >= depth) continue;

        Note* currentNote = noteManager.getNoteById(currentId);
        if (!currentNote) continue;

        for (const std::string& rawLink : currentNote->links) {
            
            const std::string resolvedLink = noteManager.resolveAlias(rawLink);

            
            if (!noteManager.getNoteById(resolvedLink)) continue;

            
            if (visited.count(resolvedLink)) continue;

            visited.insert(resolvedLink);
            bfsQueue.push({resolvedLink, currentDepth + 1});
        }
    }

    return result;
}


static std::string formatFrontmatter(const Note& note) {
    if (note.frontmatter.empty()) {
        return "(sin metadatos)\n";
    }

    std::string out;
    for (const auto& [key, value] : note.frontmatter) {
        out += key + ": " + value + "\n";
    }
    return out;
}


std::string CompileContext(
    const std::string& startNoteId,
    int                depth,
    NoteManager&       noteManager)
{
    const auto noteIds = GetContextNoteIds(startNoteId, depth, noteManager);
    if (noteIds.empty()) return {};

    std::ostringstream out;

    out << "# CONTEXTO DE NOTAS — EXPORTADO DESDE NODE PANDA\n";
    out << "# Nota inicial   : " << startNoteId << "\n";
    out << "# Profundidad    : " << depth
        << (depth == 0 ? " (solo la nota inicial)"
          : depth == 1 ? " (nota + enlaces directos)"
          : " (BFS a " + std::to_string(depth) + " saltos)")
        << "\n";
    out << "# Notas incluidas: " << noteIds.size() << "\n";
    out << "#\n";
    out << "# Los [[corchetes dobles]] son wikilinks a otras notas.\n";
    out << "# Las notas referenciadas que estén en este contexto aparecen\n";
    out << "# en secciones separadas más abajo.\n";
    out << "\n";

    for (const std::string& noteId : noteIds) {
        Note* note = noteManager.getNoteById(noteId);
        if (!note) continue; 

        out << "--- INICIO DE NOTA: " << note->id << " ---\n";

        out << "METADATOS:\n";
        out << formatFrontmatter(*note);

        out << "\nCONTENIDO:\n";
        if (note->content.empty()) {
            out << "(sin contenido)\n";
        } else {
            out << note->content;
            if (note->content.back() != '\n') {
                out << '\n';
            }
        }

        out << "--- FIN DE NOTA ---\n\n";
    }

    return out.str();
}



static std::string escapeXml(const std::string& src) {
    std::string out;
    out.reserve(src.size() + src.size() / 8);
    for (char c : src) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            default:   out += c;        break;
        }
    }
    return out;
}

VaultExportResult ExportVaultForClaude(NoteManager& noteManager) {
    VaultExportResult result{};

    const auto& allNotes = noteManager.getAllNotes();
    if (allNotes.empty()) {
        result.success = false;
        result.error   = "No hay notas cargadas en el sistema.";
        return result;
    }

    std::ostringstream xml;

    xml << "<documents>\n";

    int index = 1;
    std::vector<std::pair<std::string, const Note*>> sorted;
    sorted.reserve(allNotes.size());
    for (const auto& [id, note] : allNotes) {
        sorted.push_back({id, &note});
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (const auto& [id, notePtr] : sorted) {
        const Note& note = *notePtr;

        std::string fullContent;
        if (!note.frontmatter.empty()) {
            fullContent += "---\n";
            for (const auto& [key, value] : note.frontmatter) {
                fullContent += key + ": " + value + "\n";
            }
            fullContent += "---\n";
        }
        fullContent += note.content;

        std::string filename = note.filepath.filename().string();
        if (filename.empty()) filename = id + ".md";

        xml << "<document index=\"" << index << "\">\n";
        xml << "  <source>" << escapeXml(filename) << "</source>\n";
        xml << "  <document_content>\n";
        xml << escapeXml(fullContent);
        if (!fullContent.empty() && fullContent.back() != '\n') xml << '\n';
        xml << "  </document_content>\n";
        xml << "</document>\n";

        ++index;
    }

    xml << "</documents>\n";

    std::string xmlStr = xml.str();
    result.noteCount  = index - 1;
    result.totalBytes = xmlStr.size();

    namespace fs = std::filesystem;
    fs::path outputPath;

    fs::path notesDir = noteManager.getNotesDirectory();
    if (fs::exists(notesDir) && fs::is_directory(notesDir)) {
        outputPath = notesDir / "claude_context_export.txt";
    } else {
        outputPath = fs::current_path() / "claude_context_export.txt";
    }

    std::ofstream outFile(outputPath, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        outputPath = fs::current_path() / "claude_context_export.txt";
        outFile.open(outputPath, std::ios::out | std::ios::trunc);
    }

    if (!outFile.is_open()) {
        result.success = false;
        result.error   = "No se pudo crear el archivo de exportacion.";
        return result;
    }

    outFile.write(xmlStr.data(), static_cast<std::streamsize>(xmlStr.size()));
    outFile.close();

    result.success    = true;
    result.outputPath = outputPath.string();
    return result;
}

} 
