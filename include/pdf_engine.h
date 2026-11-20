#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nodepanda {

class NoteManager;

struct PdfHighlight {
    std::string id;
    int pageIndex = 0;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.2f;
    float h = 0.05f;
    int colorIndex = 0;
    std::string quotedText;
    std::string comment;
    std::string linkedNoteId;
    std::string createdAt;
};

struct PdfPage {
    int pageNumber = 1;
    float widthPt = 612.0f;
    float heightPt = 792.0f;
    std::vector<std::string> textLines;
};

struct PdfDocument {
    std::string id;
    std::string title;
    std::filesystem::path filePath;
    std::string sha256;
    std::vector<PdfPage> pages;
    std::vector<PdfHighlight> highlights;
    bool loaded = false;
};

class PdfEngine {
public:
    void init(const std::filesystem::path& dataDir);
    bool openPdf(const std::filesystem::path& filePath, std::string& errorOut);
    bool createSamplePdf(const std::filesystem::path& outPath, const std::string& title, const std::vector<std::string>& paragraphs);

    const std::vector<PdfDocument>& documents() const;
    PdfDocument* activeDocument();
    const PdfDocument* activeDocument() const;
    bool setActiveDocument(const std::string& docId);

    std::string addHighlight(int pageIndex, float x, float y, float w, float h, int colorIndex,
                             const std::string& quotedText, const std::string& comment,
                             const std::string& linkedNoteId);
    bool removeHighlight(const std::string& highlightId);

    std::string formatCitation(const PdfDocument& doc, const PdfHighlight& hl) const;
    bool exportCitationToNote(NoteManager& nm, const std::string& noteId, const std::string& highlightId);

    void saveAnnotations(const PdfDocument& doc) const;
    void loadAnnotations(PdfDocument& doc);

    const std::filesystem::path& dataDir() const { return m_dataDir; }
    const std::filesystem::path& annotationsDir() const { return m_annotationsDir; }

private:
    std::filesystem::path annotationFilePath(const std::string& docId) const;
    bool parsePdfBuffer(const std::string& raw, PdfDocument& doc, std::string& errorOut) const;

    static std::string currentIsoTimestamp();
    static std::string sanitizeId(const std::string& raw);
    static std::string escapeTsv(const std::string& value);
    static std::string unescapeTsv(const std::string& value);
    static std::string escapePdfString(const std::string& value);

    std::filesystem::path m_dataDir;
    std::filesystem::path m_annotationsDir;
    std::vector<PdfDocument> m_documents;
    int m_activeIndex = -1;
};

}
