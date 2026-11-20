#include "auth_manager.h"
#include "crypto_engine.h"
#include "datalog_engine.h"
#include "math_renderer.h"
#include "mobile_bridge.h"
#include "note_manager.h"
#include "outliner_engine.h"
#include "pdf_engine.h"
#include "table_view_engine.h"
#include "task_engine.h"
#include "vector_engine.h"
#include "websocket_rtc.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace nodepanda;

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool cond, const std::string& name) {
    if (cond) {
        ++g_passed;
    } else {
        ++g_failed;
        std::cerr << "[FAIL] " << name << "\n";
    }
}

std::filesystem::path makeTempDir(const std::string& tag) {
    auto dir = std::filesystem::temp_directory_path() / ("nodepanda_test_" + tag);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void testCrypto() {
    check(CryptoEngine::sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sha256_abc");
    check(CryptoEngine::sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "sha256_empty");
    check(CryptoEngine::hmacSha256("key", "The quick brown fox jumps over the lazy dog") ==
              "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8",
          "hmac_sha256_vector");

    std::string k1 = CryptoEngine::deriveKey("secret", "salt1234", 500);
    std::string k2 = CryptoEngine::deriveKey("secret", "salt1234", 500);
    std::string k3 = CryptoEngine::deriveKey("secret", "salt9999", 500);
    check(k1.size() == 64 && CryptoEngine::secureEquals(k1, k2) && !CryptoEngine::secureEquals(k1, k3),
          "pbkdf2_derive_and_secure_equals");

    std::string b64 = CryptoEngine::base64Encode("Node Panda C++17");
    check(CryptoEngine::base64Decode(b64) == "Node Panda C++17", "base64_roundtrip");

    std::string uuid = CryptoEngine::generateUuid();
    check(uuid.size() == 36 && uuid[8] == '-' && uuid[13] == '-' && uuid[18] == '-' && uuid[23] == '-',
          "uuid_v4_format");

    std::string cipher = CryptoEngine::encryptPayload("contenido confidencial", "clave-fuerte-2026");
    bool ok = false;
    std::string plain = CryptoEngine::decryptPayload(cipher, "clave-fuerte-2026", ok);
    check(ok && plain == "contenido confidencial", "e2ee_roundtrip");

    bool wrongOk = true;
    CryptoEngine::decryptPayload(cipher, "clave-incorrecta", wrongOk);
    check(!wrongOk, "e2ee_wrong_password_rejected");

    std::string tampered = cipher;
    if (!tampered.empty()) tampered[tampered.size() - 1] = (tampered.back() == '0') ? '1' : '0';
    bool tamperOk = true;
    CryptoEngine::decryptPayload(tampered, "clave-fuerte-2026", tamperOk);
    check(!tamperOk, "e2ee_tamper_rejected");
}

void testAuth() {
    auto dir = makeTempDir("auth");
    AuthManager auth;
    auth.init(dir);
    check(!auth.hasUsers(), "auth_initially_empty");

    std::string err;
    check(auth.registerUser("admin", "admin@nodepanda.local", "Passw0rd!23", err), "auth_register");
    check(auth.hasUsers(), "auth_has_users");

    UserAccount user;
    check(!auth.authenticate("admin", "wrongpass", user, err), "auth_reject_bad_password");
    check(auth.authenticate("admin", "Passw0rd!23", user, err), "auth_valid_login");
    check(user.username == "admin", "auth_username_match");

    std::string tok = auth.createSession(user);
    AuthSession sess;
    check(!tok.empty() && auth.validateToken(tok, sess) && sess.username == "admin", "auth_session_valid");

    auth.revokeToken(tok);
    check(!auth.validateToken(tok, sess), "auth_session_revoked");

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void testKnowledgeEngines() {
    auto dir = makeTempDir("knowledge");
    auto notesDir = dir / "notas";
    std::filesystem::create_directories(notesDir);

    NoteManager nm;
    nm.setNotesDirectory(notesDir);
    auto* n1 = nm.createNote("Arquitectura");
    check(n1 != nullptr, "note_create_1");
    n1->frontmatter["tipo"] = "proyecto";
    n1->frontmatter["tags"] = "cpp, motor";
    n1->content =
        "# Arquitectura\n"
        "- Bloque principal sobre [[Vectores]]\n"
        "  - Subbloque con estado:: activo\n"
        "- TODO [#A] Implementar pruebas automatizadas\n";
    n1->saveToFile();
    n1->parseLinks();
    n1->parseTitle();

    auto* n2 = nm.createNote("Vectores");
    check(n2 != nullptr, "note_create_2");
    n2->frontmatter["tipo"] = "concepto";
    n2->frontmatter["tags"] = "ia, busqueda";
    n2->content =
        "# Vectores\n"
        "- Embeddings de 384 dimensiones conectados a [[Arquitectura]]\n"
        "- DONE [#B] Validar similitud coseno\n";
    n2->saveToFile();
    n2->parseLinks();
    n2->parseTitle();

    OutlinerEngine outliner;
    outliner.rebuildFromNotes(nm);
    check(outliner.getAllBlocks().size() >= 4, "outliner_block_count");
    auto roots = outliner.getRootBlocksForPage("Arquitectura");
    check(!roots.empty(), "outliner_root_blocks");

    TaskEngine tasks;
    tasks.rebuildFromNotes(nm);
    check(tasks.getTasks().size() == 2, "task_count");
    check(tasks.countByStatus(TaskStatus::Todo) == 1, "task_todo_count");
    check(tasks.countByStatus(TaskStatus::Done) == 1, "task_done_count");

    DatalogEngine datalog;
    datalog.rebuildIndex(nm);
    check(datalog.datomCount() > 0, "datalog_indexed");
    std::string dlErr;
    auto rows = datalog.executeQuery("[:find ?p ?t :where [?p :page/title ?t]]", dlErr);
    check(dlErr.empty() && rows.size() >= 2, "datalog_page_titles_query");

    VectorEngine vec;
    vec.init(dir);
    vec.rebuildIndex(nm, outliner);
    check(vec.documentCount() >= 2, "vector_index_populated");
    auto embA = vec.embedLocalDeterministic("embeddings coseno 384 dimensiones");
    auto embB = vec.embedLocalDeterministic("embeddings coseno 384 dimensiones");
    float sim = VectorEngine::cosineSimilarity(embA, embB);
    check(std::fabs(sim - 1.0f) < 1e-4f, "vector_self_cosine_one");
    auto hits = vec.searchHybrid("embeddings coseno", 5);
    check(!hits.empty() && hits.front().pageId == "Vectores", "vector_hybrid_ranking");

    TableViewEngine tables;
    tables.init(dir);
    check(!tables.views().empty(), "table_default_views");
    auto* activeView = tables.activeView();
    check(activeView != nullptr, "table_active_view");
    auto tRows = tables.queryRows(nm, *activeView);
    check(tRows.size() == 2, "table_query_rows");
    std::string csv = tables.exportCsv(tRows, *activeView);
    std::string md = tables.exportMarkdown(tRows, *activeView);
    check(csv.find("Arquitectura") != std::string::npos, "table_export_csv");
    check(md.find("|") != std::string::npos && md.find("Vectores") != std::string::npos, "table_export_markdown");

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void testPdfEngine() {
    auto dir = makeTempDir("pdf");
    PdfEngine pdf;
    pdf.init(dir);
    auto samplePath = dir / "demo.pdf";
    check(pdf.createSamplePdf(samplePath, "Documento de Prueba",
                              {"Linea uno de arquitectura C++17", "Linea dos de anotaciones visuales"}),
          "pdf_create_sample");

    std::string err;
    check(pdf.openPdf(samplePath, err), "pdf_open_sample");
    auto* doc = pdf.activeDocument();
    check(doc != nullptr && doc->loaded && !doc->pages.empty(), "pdf_pages_parsed");

    std::string hlId = pdf.addHighlight(0, 0.1f, 0.2f, 0.6f, 0.08f, 0,
                                        "arquitectura C++17", "Nota clave", "Arquitectura");
    check(!hlId.empty() && doc->highlights.size() == 1, "pdf_add_highlight");

    std::string citation = pdf.formatCitation(*doc, doc->highlights.front());
    check(citation.find("arquitectura C++17") != std::string::npos, "pdf_format_citation");

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void testMathAndMobileAndRtc() {
    auto ast = MathRenderer::parseLatex("x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}");
    check(!ast.children.empty(), "math_ast_parsed");
    std::string syms = MathRenderer::replaceSymbols("\\alpha + \\beta \\le \\infty");
    check(syms.find("\\alpha") == std::string::npos, "math_symbols_replaced");

    auto blocks = MathRenderer::extractMathBlocks("Texto $E = mc^2$ y bloque:\n$$\n\\sum_{i=1}^{n} i\n$$\n");
    check(blocks.size() == 2 && !blocks[0].displayMode && blocks[1].displayMode, "math_extract_blocks");

    std::string accept = WebSocketRtcServer::computeWebSocketAccept("dGhlIHNhbXBsZSBub25jZQ==");
    check(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", "rfc6455_websocket_accept_vector");

    WebSocketRtcServer rtc;
    rtc.setLocalPresence("tester", "Arquitectura", "blk-1");
    rtc.broadcastDelta("Arquitectura", "update", "contenido");
    auto st = rtc.stats();
    check(st.currentRev >= 1, "rtc_revision_incremented");

    std::string manifest = MobileBridge::renderManifestJson();
    std::string sw = MobileBridge::renderServiceWorkerJs();
    std::string html = MobileBridge::renderMobileAppHtml(8042);
    check(manifest.find("Node Panda") != std::string::npos, "mobile_manifest");
    check(sw.find("nodepanda-mobile") != std::string::npos, "mobile_service_worker");
    check(html.find("<!DOCTYPE html>") != std::string::npos && html.find("/api/notas") != std::string::npos,
          "mobile_html_shell");
}

}

int main() {
    testCrypto();
    testAuth();
    testKnowledgeEngines();
    testPdfEngine();
    testMathAndMobileAndRtc();

    std::cout << "Node Panda Test Suite: " << g_passed << " passed, " << g_failed << " failed.\n";
    return g_failed == 0 ? 0 : 1;
}
