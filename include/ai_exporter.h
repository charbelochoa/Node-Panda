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

struct VaultExportResult {
    bool        success;
    std::string outputPath;
    int         noteCount;
    size_t      totalBytes;
    std::string error;
};

VaultExportResult ExportVaultForClaude(NoteManager& noteManager);

}
