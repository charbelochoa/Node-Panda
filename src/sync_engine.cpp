#include "sync_engine.h"
#include "crypto_engine.h"
#include "note_manager.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>

namespace nodepanda {

std::string SyncEngine::currentIsoTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmInfo{};
#ifdef _WIN32
    localtime_s(&tmInfo, &t);
#else
    localtime_r(&t, &tmInfo);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmInfo);
    return std::string(buf);
}

std::string SyncEngine::sanitizeField(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return out;
}

void SyncEngine::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    m_syncDir = m_dataDir / "sync";
    m_configPath = m_syncDir / "sync_config.tsv";
    m_txLogPath = m_syncDir / "transactions.tsv";

    if (!std::filesystem::exists(m_syncDir)) {
        std::filesystem::create_directories(m_syncDir);
    }

    m_transactions.clear();
    m_localT = 0;
    m_remoteT = 0;
    m_pendingBatchCount = 0;

    loadConfig();
    loadTransactions();
}

void SyncEngine::loadConfig() {
    if (m_configPath.empty() || !std::filesystem::exists(m_configPath)) return;
    std::ifstream in(m_configPath);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string key = line.substr(0, tab);
        std::string val = line.substr(tab + 1);
        if (key == "httpBase") m_httpBase = val;
        else if (key == "wsUrl") m_wsUrl = val;
        else if (key == "e2eeEnabled") m_e2eeEnabled = (val == "1");
        else if (key == "e2eeSalt") m_e2eeSalt = val;
        else if (key == "e2eeVerifier") m_e2eeVerifier = val;
        else if (key == "remoteChecksum") m_remoteChecksum = val;
        else if (key == "running") m_running = (val == "1");
        else if (key == "remoteT") {
            try { m_remoteT = std::stoll(val); } catch (...) { m_remoteT = 0; }
        } else if (key == "pendingBatchCount") {
            try { m_pendingBatchCount = std::stoi(val); } catch (...) { m_pendingBatchCount = 0; }
        }
    }
}

void SyncEngine::saveConfig() const {
    if (m_configPath.empty()) return;
    if (!std::filesystem::exists(m_syncDir)) {
        std::filesystem::create_directories(m_syncDir);
    }
    std::ofstream out(m_configPath, std::ios::trunc);
    if (!out.is_open()) return;
    out << "httpBase\t" << sanitizeField(m_httpBase) << '\n';
    out << "wsUrl\t" << sanitizeField(m_wsUrl) << '\n';
    out << "e2eeEnabled\t" << (m_e2eeEnabled ? "1" : "0") << '\n';
    out << "e2eeSalt\t" << m_e2eeSalt << '\n';
    out << "e2eeVerifier\t" << m_e2eeVerifier << '\n';
    out << "remoteT\t" << m_remoteT << '\n';
    out << "remoteChecksum\t" << m_remoteChecksum << '\n';
    out << "running\t" << (m_running ? "1" : "0") << '\n';
    out << "pendingBatchCount\t" << m_pendingBatchCount << '\n';
}

void SyncEngine::loadTransactions() {
    m_transactions.clear();
    if (m_txLogPath.empty() || !std::filesystem::exists(m_txLogPath)) return;
    std::ifstream in(m_txLogPath);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tStr, txId, entityId, op, payloadHash, ts;
        if (!std::getline(ss, tStr, '\t')) continue;
        if (!std::getline(ss, txId, '\t')) continue;
        if (!std::getline(ss, entityId, '\t')) continue;
        if (!std::getline(ss, op, '\t')) continue;
        if (!std::getline(ss, payloadHash, '\t')) continue;
        std::getline(ss, ts, '\t');

        SyncTransaction tx;
        try { tx.t = std::stoll(tStr); } catch (...) { tx.t = 0; }
        tx.txId = txId;
        tx.entityId = entityId;
        tx.op = op;
        tx.payloadHash = payloadHash;
        tx.timestamp = ts;
        if (tx.t > m_localT) m_localT = tx.t;
        m_transactions.push_back(std::move(tx));
    }
}

void SyncEngine::appendTransactionToDisk(const SyncTransaction& tx) const {
    if (m_txLogPath.empty()) return;
    if (!std::filesystem::exists(m_syncDir)) {
        std::filesystem::create_directories(m_syncDir);
    }
    std::ofstream out(m_txLogPath, std::ios::app);
    if (!out.is_open()) return;
    out << tx.t << '\t'
        << tx.txId << '\t'
        << sanitizeField(tx.entityId) << '\t'
        << sanitizeField(tx.op) << '\t'
        << tx.payloadHash << '\t'
        << tx.timestamp << '\n';
}

void SyncEngine::configureTransport(const std::string& httpBase, const std::string& wsUrl) {
    m_httpBase = httpBase;
    m_wsUrl = wsUrl;
    saveConfig();
}

bool SyncEngine::enableE2EE(const std::string& password, std::string& errorOut) {
    errorOut.clear();
    if (password.size() < 4) {
        errorOut = "Password must be at least 4 characters";
        return false;
    }
    m_e2eeSalt = CryptoEngine::randomHex(16);
    std::string derived = CryptoEngine::deriveKey(password, m_e2eeSalt, 10000);
    m_e2eeVerifier = CryptoEngine::hmacSha256(derived, "nodepanda-e2ee-v1");
    m_e2eeEnabled = true;
    saveConfig();
    return true;
}

bool SyncEngine::verifyE2EEPassword(const std::string& password) const {
    if (!m_e2eeEnabled || m_e2eeSalt.empty() || m_e2eeVerifier.empty()) {
        return false;
    }
    std::string derived = CryptoEngine::deriveKey(password, m_e2eeSalt, 10000);
    std::string check = CryptoEngine::hmacSha256(derived, "nodepanda-e2ee-v1");
    return CryptoEngine::secureEquals(check, m_e2eeVerifier);
}

void SyncEngine::recordTransaction(const std::string& entityId, const std::string& op, const std::string& payload) {
    ++m_localT;
    SyncTransaction tx;
    tx.t = m_localT;
    tx.txId = CryptoEngine::generateUuid();
    tx.entityId = entityId;
    tx.op = op;
    tx.payloadHash = CryptoEngine::sha256(payload);
    tx.timestamp = currentIsoTimestamp();

    m_transactions.push_back(tx);
    if (m_running) {
        m_remoteT = m_localT;
        m_pendingBatchCount = 0;
    } else {
        ++m_pendingBatchCount;
    }
    appendTransactionToDisk(tx);
    saveConfig();
}

std::string SyncEngine::computeStateChecksum(const NoteManager& nm) const {
    const auto& notes = nm.getAllNotes();
    std::vector<std::string> ids;
    ids.reserve(notes.size());
    for (const auto& [id, note] : notes) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());

    std::ostringstream acc;
    for (const auto& id : ids) {
        const Note& note = notes.at(id);
        acc << id << '\x1f' << note.serializeFrontmatter() << note.content << '\x1e';
    }
    return CryptoEngine::sha256(acc.str());
}

bool SyncEngine::exportSnapshot(const NoteManager& nm, const std::filesystem::path& outPath, bool encrypt, const std::string& password, std::string& errorOut) const {
    errorOut.clear();
    if (outPath.empty()) {
        errorOut = "Output path is empty";
        return false;
    }
    if (encrypt && password.empty()) {
        errorOut = "Password is required for encrypted snapshot export";
        return false;
    }

    const auto& notes = nm.getAllNotes();
    std::vector<std::string> ids;
    ids.reserve(notes.size());
    for (const auto& [id, note] : notes) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());

    std::string stateHash = computeStateChecksum(nm);
    std::ostringstream ss;
    ss << "NODEPANDA_SNAPSHOT_V1\n";
    ss << "checksum\t" << stateHash << "\n";
    ss << "local_t\t" << m_localT << "\n";
    ss << "count\t" << ids.size() << "\n";

    const auto& notesDir = nm.getNotesDirectory();
    for (const auto& id : ids) {
        const Note& note = notes.at(id);
        std::string relPath = id + ".md";
        if (!note.filepath.empty() && !notesDir.empty()) {
            std::error_code ec;
            auto rel = std::filesystem::relative(note.filepath, notesDir, ec);
            if (!ec && !rel.empty()) {
                relPath = rel.generic_string();
            }
        }
        std::string fullText = note.serializeFrontmatter() + note.content;
        std::string b64 = CryptoEngine::base64Encode(fullText);
        ss << "NOTE\t" << sanitizeField(id) << '\t' << sanitizeField(relPath) << '\t' << b64 << "\n";
    }

    std::string finalPayload = ss.str();
    if (encrypt) {
        std::string encrypted = CryptoEngine::encryptPayload(finalPayload, password);
        finalPayload = "NODEPANDA_ENC_V1\n" + encrypted;
    }

    std::error_code ec;
    if (outPath.has_parent_path() && !outPath.parent_path().empty()) {
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }
    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        errorOut = "Failed to open snapshot output path";
        return false;
    }
    out << finalPayload;
    return true;
}

bool SyncEngine::importSnapshot(NoteManager& nm, const std::filesystem::path& inPath, const std::string& password, std::string& errorOut) {
    errorOut.clear();
    if (inPath.empty() || !std::filesystem::exists(inPath)) {
        errorOut = "Snapshot file does not exist";
        return false;
    }

    std::ifstream in(inPath, std::ios::binary);
    if (!in.is_open()) {
        errorOut = "Failed to open snapshot file";
        return false;
    }
    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    std::string plain = raw;
    const std::string encPrefix = "NODEPANDA_ENC_V1\n";
    if (raw.rfind(encPrefix, 0) == 0) {
        if (password.empty()) {
            errorOut = "Encrypted snapshot requires password";
            return false;
        }
        std::string encPayload = raw.substr(encPrefix.size());
        bool ok = false;
        plain = CryptoEngine::decryptPayload(encPayload, password, ok);
        if (!ok) {
            errorOut = "Failed to decrypt snapshot (invalid password or corrupted file)";
            return false;
        }
    }

    std::istringstream ss(plain);
    std::string header;
    if (!std::getline(ss, header)) {
        errorOut = "Empty snapshot file";
        return false;
    }
    if (!header.empty() && header.back() == '\r') header.pop_back();
    if (header != "NODEPANDA_SNAPSHOT_V1") {
        errorOut = "Unsupported snapshot format";
        return false;
    }

    const auto& notesDir = nm.getNotesDirectory();
    if (notesDir.empty()) {
        errorOut = "Notes directory is not set";
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::exists(notesDir, ec)) {
        std::filesystem::create_directories(notesDir, ec);
    }

    std::string snapChecksum;
    int64_t snapT = m_localT;
    struct DecodedNote {
        std::string relPath;
        std::string content;
    };
    std::vector<DecodedNote> decodedNotes;

    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ls(line);
        std::string tag;
        if (!std::getline(ls, tag, '\t')) continue;
        if (tag == "checksum") {
            std::getline(ls, snapChecksum);
        } else if (tag == "local_t") {
            std::string val;
            std::getline(ls, val);
            try { snapT = std::stoll(val); } catch (...) {}
        } else if (tag == "NOTE") {
            std::string id, relPath, b64;
            if (!std::getline(ls, id, '\t')) continue;
            if (!std::getline(ls, relPath, '\t')) continue;
            std::getline(ls, b64);
            if (relPath.find("..") != std::string::npos) continue;
            decodedNotes.push_back({relPath, CryptoEngine::base64Decode(b64)});
        }
    }

    for (const auto& dn : decodedNotes) {
        std::filesystem::path target = notesDir / dn.relPath;
        std::filesystem::create_directories(target.parent_path(), ec);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            errorOut = "Failed writing note file: " + dn.relPath;
            return false;
        }
        out << dn.content;
    }

    nm.scanDirectory();
    nm.rebuildAllLinks();

    m_remoteT = std::max(m_remoteT, snapT);
    m_remoteChecksum = snapChecksum.empty() ? computeStateChecksum(nm) : snapChecksum;
    m_pendingBatchCount = 0;
    recordTransaction("snapshot", "import", m_remoteChecksum);
    return true;
}

void SyncEngine::startSync() {
    m_running = true;
    m_remoteT = m_localT;
    m_pendingBatchCount = 0;
    saveConfig();
}

void SyncEngine::stopSync() {
    m_running = false;
    saveConfig();
}

SyncDiagnostics SyncEngine::diagnostics(const NoteManager& nm) const {
    SyncDiagnostics d;
    d.localT = m_localT;
    d.remoteT = m_remoteT;
    d.localChecksum = computeStateChecksum(nm);
    d.remoteChecksum = m_remoteChecksum.empty() ? (m_remoteT == m_localT ? d.localChecksum : "") : m_remoteChecksum;
    d.inSync = (m_localT == m_remoteT) && (m_pendingBatchCount == 0) &&
               (d.remoteChecksum.empty() || d.localChecksum == d.remoteChecksum);
    d.e2eeEnabled = m_e2eeEnabled;
    d.running = m_running;
    d.httpBase = m_httpBase;
    d.wsUrl = m_wsUrl;
    d.pendingBatchCount = m_pendingBatchCount;
    return d;
}

}
