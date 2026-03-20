#pragma once


#include <string>
#include <vector>

namespace nodepanda {

class NoteManager;


std::vector<std::string> GetContextNoteIds(
    const std::string& startNoteId,
    int                depth,
    NoteManager&       noteManager
);


std::string CompileContext(
    const std::string& startNoteId,
    int                depth,
    NoteManager&       noteManager
);

// ── Exportación masiva de bóveda en formato XML para Claude ─────────────
struct VaultExportResult {
    bool        success;
    std::string outputPath;   // ruta completa del archivo generado
    int         noteCount;    // número de notas exportadas
    size_t      totalBytes;   // tamaño total en bytes
    std::string error;        // mensaje de error si !success
};

VaultExportResult ExportVaultForClaude(NoteManager& noteManager);

} 
