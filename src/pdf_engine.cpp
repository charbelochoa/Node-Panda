#include "pdf_engine.h"
#include "crypto_engine.h"
#include "note_manager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace nodepanda {

namespace {

bool isPdfWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\0';
}

bool isPdfDelimiter(char c) {
    return isPdfWhitespace(c) || c == '(' || c == ')' || c == '<' || c == '>' ||
           c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%';
}

std::string trimCopy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

size_t findNameToken(const std::string& text, const std::string& token, size_t startPos = 0) {
    size_t pos = startPos;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        size_t after = pos + token.size();
        if (after >= text.size() || isPdfDelimiter(text[after])) {
            return pos;
        }
        pos += token.size();
    }
    return std::string::npos;
}

std::string decodePdfLiteralString(const std::string& data, size_t& pos) {
    std::string out;
    if (pos >= data.size() || data[pos] != '(') return out;
    ++pos;
    int depth = 1;
    while (pos < data.size() && depth > 0) {
        char c = data[pos++];
        if (c == '\\') {
            if (pos >= data.size()) break;
            char esc = data[pos++];
            switch (esc) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case '(': out.push_back('('); break;
                case ')': out.push_back(')'); break;
                case '\\': out.push_back('\\'); break;
                case '\r':
                    if (pos < data.size() && data[pos] == '\n') ++pos;
                    break;
                case '\n':
                    break;
                default:
                    if (esc >= '0' && esc <= '7') {
                        int val = esc - '0';
                        for (int k = 0; k < 2 && pos < data.size(); ++k) {
                            if (data[pos] >= '0' && data[pos] <= '7') {
                                val = val * 8 + (data[pos] - '0');
                                ++pos;
                            } else {
                                break;
                            }
                        }
                        out.push_back(static_cast<char>(val & 0xFF));
                    } else {
                        out.push_back(esc);
                    }
                    break;
            }
        } else if (c == '(') {
            ++depth;
            out.push_back(c);
        } else if (c == ')') {
            --depth;
            if (depth > 0) out.push_back(c);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

std::string decodePdfHexString(const std::string& data, size_t& pos) {
    std::string bytes;
    if (pos >= data.size() || data[pos] != '<') return bytes;
    ++pos;
    int hi = -1;
    while (pos < data.size() && data[pos] != '>') {
        char c = data[pos++];
        int v = hexVal(c);
        if (v < 0) continue;
        if (hi < 0) {
            hi = v;
        } else {
            bytes.push_back(static_cast<char>((hi << 4) | v));
            hi = -1;
        }
    }
    if (pos < data.size() && data[pos] == '>') ++pos;
    if (hi >= 0) {
        bytes.push_back(static_cast<char>(hi << 4));
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        std::string ascii;
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            unsigned char b0 = static_cast<unsigned char>(bytes[i]);
            unsigned char b1 = static_cast<unsigned char>(bytes[i + 1]);
            if (b0 == 0 && b1 >= 32 && b1 < 127) {
                ascii.push_back(static_cast<char>(b1));
            } else if (b0 == 0 && (b1 == '\n' || b1 == '\t' || b1 == ' ')) {
                ascii.push_back(' ');
            }
        }
        return ascii;
    }

    std::string clean;
    for (char c : bytes) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 32 && uc < 127) {
            clean.push_back(c);
        } else if (uc == '\t' || uc == '\n' || uc == '\r') {
            clean.push_back(' ');
        }
    }
    return clean;
}

std::string sanitizeExtractedLine(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    bool prevSpace = false;
    for (unsigned char c : raw) {
        if (c == '\r' || c == '\n' || c == '\t') {
            if (!prevSpace && !out.empty()) {
                out.push_back(' ');
                prevSpace = true;
            }
        } else if (c >= 32 && c != 127) {
            if (c == ' ') {
                if (!prevSpace && !out.empty()) {
                    out.push_back(' ');
                    prevSpace = true;
                }
            } else {
                out.push_back(static_cast<char>(c));
                prevSpace = false;
            }
        }
    }
    return trimCopy(out);
}

bool parseMediaBox(const std::string& dict, float& widthOut, float& heightOut) {
    size_t mb = findNameToken(dict, "/MediaBox");
    if (mb == std::string::npos) return false;
    size_t lb = dict.find('[', mb);
    if (lb == std::string::npos) return false;
    size_t rb = dict.find(']', lb);
    if (rb == std::string::npos) return false;
    std::istringstream iss(dict.substr(lb + 1, rb - lb - 1));
    float x0 = 0.0f, y0 = 0.0f, x1 = 612.0f, y1 = 792.0f;
    if (iss >> x0 >> y0 >> x1 >> y1) {
        float w = std::fabs(x1 - x0);
        float h = std::fabs(y1 - y0);
        if (w > 20.0f && h > 20.0f) {
            widthOut = w;
            heightOut = h;
            return true;
        }
    }
    return false;
}

bool parseSingleRef(const std::string& text, size_t startPos, int& objNumOut) {
    size_t pos = startPos;
    while (pos < text.size() && isPdfWhitespace(text[pos])) ++pos;
    if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) return false;
    size_t numEnd = pos;
    while (numEnd < text.size() && std::isdigit(static_cast<unsigned char>(text[numEnd]))) ++numEnd;
    int objNum = 0;
    try {
        objNum = std::stoi(text.substr(pos, numEnd - pos));
    } catch (...) {
        return false;
    }
    pos = numEnd;
    while (pos < text.size() && isPdfWhitespace(text[pos])) ++pos;
    if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) return false;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
    while (pos < text.size() && isPdfWhitespace(text[pos])) ++pos;
    if (pos < text.size() && text[pos] == 'R') {
        objNumOut = objNum;
        return true;
    }
    return false;
}

std::vector<int> parseRefArray(const std::string& text, size_t arrayStart) {
    std::vector<int> refs;
    size_t rb = text.find(']', arrayStart);
    if (rb == std::string::npos) return refs;
    std::string inner = text.substr(arrayStart + 1, rb - arrayStart - 1);
    std::istringstream iss(inner);
    std::vector<std::string> toks;
    std::string tok;
    while (iss >> tok) toks.push_back(tok);
    for (size_t i = 0; i + 2 < toks.size(); ++i) {
        if (toks[i + 2] == "R") {
            try {
                int objNum = std::stoi(toks[i]);
                refs.push_back(objNum);
                i += 2;
            } catch (...) {
            }
        }
    }
    return refs;
}

std::vector<int> parseContentsRefs(const std::string& dict) {
    std::vector<int> refs;
    size_t cpos = findNameToken(dict, "/Contents");
    if (cpos == std::string::npos) return refs;
    size_t pos = cpos + 9;
    while (pos < dict.size() && isPdfWhitespace(dict[pos])) ++pos;
    if (pos >= dict.size()) return refs;
    if (dict[pos] == '[') {
        return parseRefArray(dict, pos);
    }
    int single = -1;
    if (parseSingleRef(dict, pos, single)) {
        refs.push_back(single);
    }
    return refs;
}

std::string extractStreamBytes(const std::string& objBody, bool& isFlateOut) {
    isFlateOut = (findNameToken(objBody, "/FlateDecode") != std::string::npos);
    size_t sPos = objBody.find("stream");
    if (sPos == std::string::npos) return {};
    size_t dataStart = sPos + 6;
    if (dataStart < objBody.size() && objBody[dataStart] == '\r') ++dataStart;
    if (dataStart < objBody.size() && objBody[dataStart] == '\n') ++dataStart;
    size_t ePos = objBody.rfind("endstream");
    if (ePos == std::string::npos || ePos <= dataStart) return {};
    while (ePos > dataStart && (objBody[ePos - 1] == '\r' || objBody[ePos - 1] == '\n')) {
        --ePos;
    }
    return objBody.substr(dataStart, ePos - dataStart);
}

void parseTextStream(const std::string& streamData, std::vector<std::string>& linesOut) {
    size_t pos = 0;
    while (pos < streamData.size()) {
        size_t bt = streamData.find("BT", pos);
        if (bt == std::string::npos) break;
        bool validBefore = (bt == 0 || isPdfDelimiter(streamData[bt - 1]));
        bool validAfter = (bt + 2 >= streamData.size() || isPdfDelimiter(streamData[bt + 2]));
        if (!validBefore || !validAfter) {
            pos = bt + 2;
            continue;
        }

        size_t et = bt + 2;
        size_t foundEt = std::string::npos;
        while (et < streamData.size()) {
            if (streamData[et] == '(') {
                decodePdfLiteralString(streamData, et);
                continue;
            }
            if (streamData[et] == '<' && (et + 1 >= streamData.size() || streamData[et + 1] != '<')) {
                decodePdfHexString(streamData, et);
                continue;
            }
            if (et + 1 < streamData.size() && streamData[et] == 'E' && streamData[et + 1] == 'T') {
                bool eb = (et == 0 || isPdfDelimiter(streamData[et - 1]));
                bool ea = (et + 2 >= streamData.size() || isPdfDelimiter(streamData[et + 2]));
                if (eb && ea) {
                    foundEt = et;
                    break;
                }
            }
            ++et;
        }
        if (foundEt == std::string::npos) break;

        std::string block = streamData.substr(bt + 2, foundEt - (bt + 2));
        pos = foundEt + 2;

        std::string currentLine;
        std::string lastString;
        std::vector<double> numericStack;
        bool hasLastTmY = false;
        double lastTmY = 0.0;

        auto flushLine = [&]() {
            std::string cleaned = sanitizeExtractedLine(currentLine);
            if (!cleaned.empty()) {
                linesOut.push_back(cleaned);
            }
            currentLine.clear();
        };

        size_t i = 0;
        while (i < block.size()) {
            while (i < block.size() && isPdfWhitespace(block[i])) ++i;
            if (i >= block.size()) break;

            if (block[i] == '%') {
                while (i < block.size() && block[i] != '\n' && block[i] != '\r') ++i;
                continue;
            }

            if (block[i] == '(') {
                lastString = decodePdfLiteralString(block, i);
                continue;
            }

            if (block[i] == '<') {
                if (i + 1 < block.size() && block[i + 1] == '<') {
                    i += 2;
                } else {
                    lastString = decodePdfHexString(block, i);
                }
                continue;
            }

            if (block[i] == '[') {
                ++i;
                std::string assembled;
                while (i < block.size() && block[i] != ']') {
                    while (i < block.size() && isPdfWhitespace(block[i])) ++i;
                    if (i >= block.size() || block[i] == ']') break;
                    if (block[i] == '(') {
                        assembled += decodePdfLiteralString(block, i);
                    } else if (block[i] == '<') {
                        assembled += decodePdfHexString(block, i);
                    } else {
                        size_t numStart = i;
                        while (i < block.size() && !isPdfDelimiter(block[i])) ++i;
                        std::string numTok = block.substr(numStart, i - numStart);
                        try {
                            double kern = std::stod(numTok);
                            if (kern < -150.0 && !assembled.empty() && assembled.back() != ' ') {
                                assembled.push_back(' ');
                            }
                        } catch (...) {
                        }
                    }
                }
                if (i < block.size() && block[i] == ']') ++i;
                lastString = assembled;
                continue;
            }

            if (block[i] == '/') {
                ++i;
                while (i < block.size() && !isPdfDelimiter(block[i])) ++i;
                continue;
            }

            if (block[i] == '\'' || block[i] == '"') {
                ++i;
                flushLine();
                currentLine += lastString;
                lastString.clear();
                numericStack.clear();
                continue;
            }

            size_t tokStart = i;
            while (i < block.size() && !isPdfDelimiter(block[i])) ++i;
            if (i == tokStart) {
                ++i;
                continue;
            }
            std::string tok = block.substr(tokStart, i - tokStart);

            if (tok == "Tj" || tok == "TJ") {
                currentLine += lastString;
                lastString.clear();
                numericStack.clear();
            } else if (tok == "T*") {
                flushLine();
                numericStack.clear();
            } else if (tok == "Td" || tok == "TD") {
                if (numericStack.size() >= 2) {
                    double ty = numericStack[numericStack.size() - 1];
                    if (std::fabs(ty) > 0.5 && !currentLine.empty()) {
                        flushLine();
                    }
                }
                numericStack.clear();
            } else if (tok == "Tm") {
                if (numericStack.size() >= 6) {
                    double ty = numericStack[numericStack.size() - 1];
                    if (hasLastTmY && std::fabs(ty - lastTmY) > 1.5 && !currentLine.empty()) {
                        flushLine();
                    }
                    lastTmY = ty;
                    hasLastTmY = true;
                }
                numericStack.clear();
            } else {
                char* endPtr = nullptr;
                double val = std::strtod(tok.c_str(), &endPtr);
                if (endPtr && *endPtr == '\0') {
                    numericStack.push_back(val);
                    if (numericStack.size() > 16) {
                        numericStack.erase(numericStack.begin());
                    }
                } else {
                    numericStack.clear();
                }
            }
        }

        flushLine();
    }
}

std::vector<std::string> extractFallbackReadableLines(const std::string& raw) {
    std::vector<std::string> lines;
    std::string run;
    auto considerRun = [&]() {
        std::string trimmed = sanitizeExtractedLine(run);
        run.clear();
        if (trimmed.size() < 8) return;
        if (trimmed.rfind("%PDF", 0) == 0 || trimmed == "endobj" || trimmed == "endstream" ||
            trimmed == "startxref" || trimmed == "trailer") {
            return;
        }
        if (trimmed.find("/Type") != std::string::npos ||
            trimmed.find("/Font") != std::string::npos ||
            trimmed.find("/Length") != std::string::npos ||
            trimmed.find("/Filter") != std::string::npos ||
            trimmed.find("00000 n") != std::string::npos) {
            return;
        }
        int alphaCount = 0;
        for (unsigned char c : trimmed) {
            if (std::isalpha(c)) ++alphaCount;
        }
        if (alphaCount < 5) return;
        lines.push_back(trimmed);
    };

    for (unsigned char c : raw) {
        if (c >= 32 && c < 127) {
            run.push_back(static_cast<char>(c));
        } else {
            if (!run.empty()) considerRun();
            if (lines.size() >= 60) break;
        }
    }
    if (!run.empty() && lines.size() < 60) considerRun();
    return lines;
}

std::map<int, std::string> parseIndirectObjects(const std::string& raw) {
    std::map<int, std::string> objects;
    size_t pos = 0;
    const size_t n = raw.size();
    while (pos < n) {
        size_t objKw = raw.find("obj", pos);
        if (objKw == std::string::npos) break;

        bool afterOk = (objKw + 3 >= n || isPdfDelimiter(raw[objKw + 3]));
        bool beforeOk = (objKw > 0 && isPdfWhitespace(raw[objKw - 1]));
        if (!beforeOk || !afterOk) {
            pos = objKw + 3;
            continue;
        }

        size_t p = objKw;
        while (p > 0 && isPdfWhitespace(raw[p - 1])) --p;
        size_t genEnd = p;
        while (p > 0 && std::isdigit(static_cast<unsigned char>(raw[p - 1]))) --p;
        size_t genStart = p;
        if (genStart == genEnd) {
            pos = objKw + 3;
            continue;
        }

        while (p > 0 && isPdfWhitespace(raw[p - 1])) --p;
        size_t numEnd = p;
        while (p > 0 && std::isdigit(static_cast<unsigned char>(raw[p - 1]))) --p;
        size_t numStart = p;
        if (numStart == numEnd) {
            pos = objKw + 3;
            continue;
        }
        if (numStart > 0 && !isPdfDelimiter(raw[numStart - 1])) {
            pos = objKw + 3;
            continue;
        }

        int objNum = 0;
        try {
            objNum = std::stoi(raw.substr(numStart, numEnd - numStart));
        } catch (...) {
            pos = objKw + 3;
            continue;
        }

        size_t bodyStart = objKw + 3;
        size_t scan = bodyStart;
        size_t bodyEnd = std::string::npos;

        while (scan < n) {
            size_t nextStream = raw.find("stream", scan);
            size_t nextEndObj = raw.find("endobj", scan);
            if (nextEndObj == std::string::npos) break;

            if (nextStream != std::string::npos && nextStream < nextEndObj) {
                bool sb = (nextStream == 0 || isPdfDelimiter(raw[nextStream - 1]));
                bool sa = (nextStream + 6 < n && (raw[nextStream + 6] == '\r' || raw[nextStream + 6] == '\n'));
                if (sb && sa) {
                    size_t endStream = raw.find("endstream", nextStream + 6);
                    if (endStream != std::string::npos) {
                        scan = endStream + 9;
                        continue;
                    }
                }
                scan = nextStream + 6;
                continue;
            }

            bool eb = (nextEndObj == 0 || isPdfDelimiter(raw[nextEndObj - 1]));
            bool ea = (nextEndObj + 6 >= n || isPdfDelimiter(raw[nextEndObj + 6]));
            if (eb && ea) {
                bodyEnd = nextEndObj;
                break;
            }
            scan = nextEndObj + 6;
        }

        if (bodyEnd == std::string::npos) {
            pos = objKw + 3;
            continue;
        }

        objects[objNum] = raw.substr(bodyStart, bodyEnd - bodyStart);
        pos = bodyEnd + 6;
    }
    return objects;
}

void collectPageOrder(const std::map<int, std::string>& objects, int nodeObj,
                      float inheritedW, float inheritedH,
                      std::vector<std::tuple<int, float, float>>& orderedPages,
                      std::set<int>& visited) {
    if (visited.count(nodeObj)) return;
    visited.insert(nodeObj);

    auto it = objects.find(nodeObj);
    if (it == objects.end()) return;
    const std::string& body = it->second;

    float w = inheritedW;
    float h = inheritedH;
    parseMediaBox(body, w, h);

    bool isPagesNode = (findNameToken(body, "/Pages") != std::string::npos &&
                        findNameToken(body, "/Kids") != std::string::npos);
    bool isPageLeaf = (findNameToken(body, "/Page") != std::string::npos && !isPagesNode);

    if (isPagesNode) {
        size_t kidsPos = findNameToken(body, "/Kids");
        size_t lb = body.find('[', kidsPos);
        if (lb != std::string::npos) {
            std::vector<int> kids = parseRefArray(body, lb);
            for (int kid : kids) {
                collectPageOrder(objects, kid, w, h, orderedPages, visited);
            }
        }
    } else if (isPageLeaf) {
        orderedPages.emplace_back(nodeObj, w, h);
    }
}

std::vector<std::string> wrapParagraph(const std::string& paragraph, size_t maxChars) {
    std::vector<std::string> wrapped;
    std::istringstream lineStream(paragraph);
    std::string rawLine;
    while (std::getline(lineStream, rawLine)) {
        if (!rawLine.empty() && rawLine.back() == '\r') rawLine.pop_back();
        if (rawLine.empty()) {
            wrapped.push_back("");
            continue;
        }
        std::istringstream words(rawLine);
        std::string word;
        std::string current;
        while (words >> word) {
            if (current.empty()) {
                current = word;
            } else if (current.size() + 1 + word.size() <= maxChars) {
                current.push_back(' ');
                current += word;
            } else {
                wrapped.push_back(current);
                current = word;
            }
        }
        if (!current.empty()) wrapped.push_back(current);
    }
    return wrapped;
}

}

std::string PdfEngine::currentIsoTimestamp() {
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

std::string PdfEngine::sanitizeId(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw) {
        if (std::isalnum(c) || c == '_' || c == '-') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ' || c == '.') {
            out.push_back('_');
        }
    }
    return out.empty() ? "pdf_doc" : out;
}

std::string PdfEngine::escapeTsv(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        if (c == '\\') out += "\\\\";
        else if (c == '\t') out += "\\t";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out.push_back(c);
    }
    return out;
}

std::string PdfEngine::unescapeTsv(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            char next = value[++i];
            if (next == 't') out.push_back('\t');
            else if (next == 'n') out.push_back('\n');
            else if (next == 'r') out.push_back('\r');
            else if (next == '\\') out.push_back('\\');
            else out.push_back(next);
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

std::string PdfEngine::escapePdfString(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value) {
        if (c == '\\' || c == '(' || c == ')') {
            out.push_back('\\');
            out.push_back(static_cast<char>(c));
        } else if (c >= 32 && c < 127) {
            out.push_back(static_cast<char>(c));
        } else if (c == '\t') {
            out.push_back(' ');
        } else {
            out.push_back('?');
        }
    }
    return out;
}

void PdfEngine::init(const std::filesystem::path& dataDir) {
    m_dataDir = dataDir;
    m_annotationsDir = m_dataDir / "assets" / "pdf_annotations";
    std::error_code ec;
    if (!std::filesystem::exists(m_annotationsDir, ec)) {
        std::filesystem::create_directories(m_annotationsDir, ec);
    }
}

std::filesystem::path PdfEngine::annotationFilePath(const std::string& docId) const {
    std::filesystem::path baseDir = m_annotationsDir.empty()
        ? (std::filesystem::path("data") / "assets" / "pdf_annotations")
        : m_annotationsDir;
    return baseDir / (sanitizeId(docId) + ".tsv");
}

bool PdfEngine::parsePdfBuffer(const std::string& raw, PdfDocument& doc, std::string& errorOut) const {
    size_t headerPos = raw.find("%PDF-");
    if (headerPos == std::string::npos || headerPos > 1024) {
        errorOut = "Archivo no es un PDF valido (encabezado %PDF- ausente).";
        return false;
    }

    std::map<int, std::string> objects = parseIndirectObjects(raw);

    for (const auto& kv : objects) {
        size_t titlePos = findNameToken(kv.second, "/Title");
        if (titlePos != std::string::npos) {
            size_t p = titlePos + 6;
            while (p < kv.second.size() && isPdfWhitespace(kv.second[p])) ++p;
            std::string parsedTitle;
            if (p < kv.second.size() && kv.second[p] == '(') {
                parsedTitle = sanitizeExtractedLine(decodePdfLiteralString(kv.second, p));
            } else if (p < kv.second.size() && kv.second[p] == '<') {
                parsedTitle = sanitizeExtractedLine(decodePdfHexString(kv.second, p));
            }
            if (!parsedTitle.empty()) {
                doc.title = parsedTitle;
                break;
            }
        }
    }

    int pagesRootObj = -1;
    for (const auto& kv : objects) {
        if (findNameToken(kv.second, "/Catalog") != std::string::npos) {
            size_t pagesPos = findNameToken(kv.second, "/Pages");
            if (pagesPos != std::string::npos) {
                if (parseSingleRef(kv.second, pagesPos + 6, pagesRootObj)) {
                    break;
                }
            }
        }
    }

    std::vector<std::tuple<int, float, float>> orderedPages;
    if (pagesRootObj >= 0) {
        std::set<int> visited;
        collectPageOrder(objects, pagesRootObj, 612.0f, 792.0f, orderedPages, visited);
    }

    if (orderedPages.empty()) {
        float defaultW = 612.0f;
        float defaultH = 792.0f;
        for (const auto& kv : objects) {
            if (findNameToken(kv.second, "/Pages") != std::string::npos) {
                parseMediaBox(kv.second, defaultW, defaultH);
            }
        }
        for (const auto& kv : objects) {
            bool isPagesNode = (findNameToken(kv.second, "/Pages") != std::string::npos &&
                                findNameToken(kv.second, "/Kids") != std::string::npos);
            if (!isPagesNode && findNameToken(kv.second, "/Page") != std::string::npos) {
                float w = defaultW;
                float h = defaultH;
                parseMediaBox(kv.second, w, h);
                orderedPages.emplace_back(kv.first, w, h);
            }
        }
    }

    doc.pages.clear();
    int pageNum = 1;
    for (const auto& entry : orderedPages) {
        int pageObj = std::get<0>(entry);
        float w = std::get<1>(entry);
        float h = std::get<2>(entry);

        PdfPage page;
        page.pageNumber = pageNum++;
        page.widthPt = w;
        page.heightPt = h;

        auto pageIt = objects.find(pageObj);
        bool hadFlate = false;
        if (pageIt != objects.end()) {
            std::vector<int> contentRefs = parseContentsRefs(pageIt->second);
            for (int cref : contentRefs) {
                auto cit = objects.find(cref);
                if (cit == objects.end()) continue;
                bool flate = false;
                std::string streamBytes = extractStreamBytes(cit->second, flate);
                if (flate) {
                    hadFlate = true;
                } else if (!streamBytes.empty()) {
                    parseTextStream(streamBytes, page.textLines);
                }
            }
        }

        if (page.textLines.empty() && hadFlate) {
            page.textLines.push_back("[Flujo comprimido FlateDecode - Pagina " + std::to_string(page.pageNumber) + "]");
        }
        doc.pages.push_back(std::move(page));
    }

    if (doc.pages.empty()) {
        PdfPage fallbackPage;
        fallbackPage.pageNumber = 1;
        parseMediaBox(raw, fallbackPage.widthPt, fallbackPage.heightPt);
        parseTextStream(raw, fallbackPage.textLines);
        if (fallbackPage.textLines.empty()) {
            fallbackPage.textLines = extractFallbackReadableLines(raw);
        }
        if (fallbackPage.textLines.empty()) {
            fallbackPage.textLines.push_back("[Documento PDF cargado sin texto plano extraible]");
        }
        doc.pages.push_back(std::move(fallbackPage));
    } else {
        bool anyExtracted = false;
        for (const auto& p : doc.pages) {
            if (!p.textLines.empty()) {
                anyExtracted = true;
                break;
            }
        }
        if (!anyExtracted) {
            std::vector<std::string> fallback = extractFallbackReadableLines(raw);
            if (!fallback.empty()) {
                doc.pages[0].textLines = std::move(fallback);
            } else {
                doc.pages[0].textLines.push_back("[Documento PDF sin flujos de texto plano]");
            }
        }
    }

    return true;
}

bool PdfEngine::openPdf(const std::filesystem::path& filePath, std::string& errorOut) {
    errorOut.clear();
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec) || !std::filesystem::is_regular_file(filePath, ec)) {
        errorOut = "No se encontro el archivo PDF: " + filePath.string();
        return false;
    }

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) {
        errorOut = "No se pudo abrir el archivo PDF.";
        return false;
    }

    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (raw.empty()) {
        errorOut = "El archivo PDF esta vacio.";
        return false;
    }

    PdfDocument doc;
    doc.filePath = filePath;
    doc.title = filePath.stem().string();
    doc.sha256 = CryptoEngine::sha256(raw);
    doc.id = sanitizeId(filePath.stem().string()) + "_" + doc.sha256.substr(0, 8);

    if (!parsePdfBuffer(raw, doc, errorOut)) {
        return false;
    }

    doc.loaded = true;
    loadAnnotations(doc);

    for (size_t i = 0; i < m_documents.size(); ++i) {
        if (m_documents[i].id == doc.id) {
            m_documents[i] = std::move(doc);
            m_activeIndex = static_cast<int>(i);
            return true;
        }
    }

    m_documents.push_back(std::move(doc));
    m_activeIndex = static_cast<int>(m_documents.size()) - 1;
    return true;
}

bool PdfEngine::createSamplePdf(const std::filesystem::path& outPath, const std::string& title,
                                const std::vector<std::string>& paragraphs) {
    std::error_code ec;
    if (outPath.has_parent_path() && !std::filesystem::exists(outPath.parent_path(), ec)) {
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }

    std::vector<std::string> allLines;
    for (const auto& para : paragraphs) {
        std::vector<std::string> w = wrapParagraph(para, 76);
        for (const auto& l : w) {
            allLines.push_back(l);
        }
        allLines.push_back("");
    }
    if (allLines.empty()) {
        allLines.push_back("Documento de demostracion.");
    }

    constexpr size_t kLinesPerFirstPage = 32;
    constexpr size_t kLinesPerNextPage = 38;
    std::vector<std::vector<std::string>> pageLines;
    size_t idx = 0;
    while (idx < allLines.size()) {
        size_t limit = pageLines.empty() ? kLinesPerFirstPage : kLinesPerNextPage;
        std::vector<std::string> chunk;
        while (idx < allLines.size() && chunk.size() < limit) {
            chunk.push_back(allLines[idx++]);
        }
        pageLines.push_back(std::move(chunk));
    }
    if (pageLines.empty()) {
        pageLines.push_back({title});
    }

    const int pageCount = static_cast<int>(pageLines.size());
    const int firstPageObj = 6;
    const int totalObjects = 5 + pageCount * 2;

    std::ostringstream pdf;
    pdf << "%PDF-1.4\n";

    std::vector<size_t> offsets(static_cast<size_t>(totalObjects + 1), 0);

    auto beginObj = [&](int objNum) {
        offsets[static_cast<size_t>(objNum)] = static_cast<size_t>(pdf.tellp());
        pdf << objNum << " 0 obj\n";
    };

    beginObj(1);
    pdf << "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    beginObj(2);
    pdf << "<< /Type /Pages /Kids [ ";
    for (int p = 0; p < pageCount; ++p) {
        pdf << (firstPageObj + p * 2) << " 0 R ";
    }
    pdf << "] /Count " << pageCount << " /MediaBox [0 0 612 792] >>\nendobj\n";

    beginObj(3);
    pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n";

    beginObj(4);
    pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>\nendobj\n";

    beginObj(5);
    pdf << "<< /Title (" << escapePdfString(title) << ") >>\nendobj\n";

    for (int p = 0; p < pageCount; ++p) {
        int pageObj = firstPageObj + p * 2;
        int contentObj = pageObj + 1;

        std::ostringstream stream;
        stream << "BT\n";
        int cursorY = 736;
        if (p == 0 && !title.empty()) {
            stream << "/F2 17 Tf\n";
            stream << "1 0 0 1 54 " << cursorY << " Tm\n";
            stream << "(" << escapePdfString(title) << ") Tj\n";
            cursorY -= 32;
        }
        stream << "/F1 11 Tf\n";
        for (const auto& line : pageLines[static_cast<size_t>(p)]) {
            if (!line.empty()) {
                stream << "1 0 0 1 54 " << cursorY << " Tm\n";
                stream << "(" << escapePdfString(line) << ") Tj\n";
            }
            cursorY -= 16;
            if (cursorY < 60) break;
        }
        stream << "1 0 0 1 280 34 Tm\n";
        stream << "/F1 9 Tf\n";
        stream << "(Pagina " << (p + 1) << " de " << pageCount << ") Tj\n";
        stream << "ET\n";

        std::string streamStr = stream.str();

        beginObj(pageObj);
        pdf << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
            << "/Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> "
            << "/Contents " << contentObj << " 0 R >>\nendobj\n";

        beginObj(contentObj);
        pdf << "<< /Length " << streamStr.size() << " >>\nstream\n"
            << streamStr << "endstream\nendobj\n";
    }

    size_t xrefOffset = static_cast<size_t>(pdf.tellp());
    pdf << "xref\n0 " << (totalObjects + 1) << "\n";
    pdf << "0000000000 65535 f \n";
    for (int i = 1; i <= totalObjects; ++i) {
        char lineBuf[32];
        std::snprintf(lineBuf, sizeof(lineBuf), "%010zu 00000 n \n", offsets[static_cast<size_t>(i)]);
        pdf << lineBuf;
    }

    pdf << "trailer\n<< /Size " << (totalObjects + 1) << " /Root 1 0 R /Info 5 0 R >>\n";
    pdf << "startxref\n" << xrefOffset << "\n%%EOF\n";

    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    std::string content = pdf.str();
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
}

const std::vector<PdfDocument>& PdfEngine::documents() const {
    return m_documents;
}

PdfDocument* PdfEngine::activeDocument() {
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_documents.size())) {
        return nullptr;
    }
    return &m_documents[static_cast<size_t>(m_activeIndex)];
}

const PdfDocument* PdfEngine::activeDocument() const {
    if (m_activeIndex < 0 || m_activeIndex >= static_cast<int>(m_documents.size())) {
        return nullptr;
    }
    return &m_documents[static_cast<size_t>(m_activeIndex)];
}

bool PdfEngine::setActiveDocument(const std::string& docId) {
    for (size_t i = 0; i < m_documents.size(); ++i) {
        if (m_documents[i].id == docId) {
            m_activeIndex = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

std::string PdfEngine::addHighlight(int pageIndex, float x, float y, float w, float h, int colorIndex,
                                    const std::string& quotedText, const std::string& comment,
                                    const std::string& linkedNoteId) {
    PdfDocument* doc = activeDocument();
    if (!doc) return {};

    int maxPage = std::max(0, static_cast<int>(doc->pages.size()) - 1);
    PdfHighlight hl;
    hl.id = "hl_" + CryptoEngine::randomHex(6);
    hl.pageIndex = std::clamp(pageIndex, 0, maxPage);
    hl.x = std::clamp(x, 0.0f, 0.995f);
    hl.y = std::clamp(y, 0.0f, 0.995f);
    hl.w = std::clamp(w, 0.005f, 1.0f - hl.x);
    hl.h = std::clamp(h, 0.005f, 1.0f - hl.y);
    hl.colorIndex = std::clamp(colorIndex, 0, 4);
    hl.quotedText = quotedText;
    hl.comment = comment;
    hl.linkedNoteId = linkedNoteId;
    hl.createdAt = currentIsoTimestamp();

    doc->highlights.push_back(hl);
    saveAnnotations(*doc);
    return hl.id;
}

bool PdfEngine::removeHighlight(const std::string& highlightId) {
    for (auto& doc : m_documents) {
        auto it = std::find_if(doc.highlights.begin(), doc.highlights.end(),
            [&](const PdfHighlight& h) { return h.id == highlightId; });
        if (it != doc.highlights.end()) {
            doc.highlights.erase(it);
            saveAnnotations(doc);
            return true;
        }
    }
    return false;
}

std::string PdfEngine::formatCitation(const PdfDocument& doc, const PdfHighlight& hl) const {
    int pageNum = hl.pageIndex + 1;
    std::string label = doc.title.empty() ? doc.id : doc.title;
    std::string cite = "[[pdf-cite:" + label + "#p" + std::to_string(pageNum) + "|" + hl.id + "]]";
    std::ostringstream out;
    if (!hl.quotedText.empty()) {
        out << "> \"" << hl.quotedText << "\"\n> " << cite;
    } else {
        out << cite;
    }
    if (!hl.comment.empty()) {
        out << "\n> " << hl.comment;
    }
    return out.str();
}

bool PdfEngine::exportCitationToNote(NoteManager& nm, const std::string& noteId, const std::string& highlightId) {
    if (noteId.empty() || highlightId.empty()) return false;
    Note* note = nm.getNoteById(noteId);
    if (!note) return false;

    for (auto& doc : m_documents) {
        for (auto& hl : doc.highlights) {
            if (hl.id == highlightId) {
                std::string citation = formatCitation(doc, hl);
                if (!note->content.empty() && note->content.back() != '\n') {
                    note->content.push_back('\n');
                }
                note->content += "\n" + citation + "\n";
                note->dirty = true;
                note->parseLinks();
                note->saveToFile();

                hl.linkedNoteId = noteId;
                saveAnnotations(doc);
                return true;
            }
        }
    }
    return false;
}

void PdfEngine::saveAnnotations(const PdfDocument& doc) const {
    if (doc.id.empty()) return;
    std::filesystem::path path = annotationFilePath(doc.id);
    std::error_code ec;
    if (path.has_parent_path() && !std::filesystem::exists(path.parent_path(), ec)) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;
    out << std::fixed << std::setprecision(5);
    for (const auto& hl : doc.highlights) {
        out << escapeTsv(hl.id) << '\t'
            << hl.pageIndex << '\t'
            << hl.x << '\t'
            << hl.y << '\t'
            << hl.w << '\t'
            << hl.h << '\t'
            << hl.colorIndex << '\t'
            << escapeTsv(hl.quotedText) << '\t'
            << escapeTsv(hl.comment) << '\t'
            << escapeTsv(hl.linkedNoteId) << '\t'
            << escapeTsv(hl.createdAt) << '\n';
    }
}

void PdfEngine::loadAnnotations(PdfDocument& doc) {
    doc.highlights.clear();
    if (doc.id.empty()) return;
    std::filesystem::path path = annotationFilePath(doc.id);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return;

    std::ifstream in(path);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> cols;
        size_t start = 0;
        while (true) {
            size_t tab = line.find('\t', start);
            if (tab == std::string::npos) {
                cols.push_back(line.substr(start));
                break;
            }
            cols.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
        if (cols.size() < 7) continue;

        PdfHighlight hl;
        hl.id = unescapeTsv(cols[0]);
        if (hl.id.empty()) continue;
        try {
            hl.pageIndex = std::stoi(cols[1]);
            hl.x = std::stof(cols[2]);
            hl.y = std::stof(cols[3]);
            hl.w = std::stof(cols[4]);
            hl.h = std::stof(cols[5]);
            hl.colorIndex = std::stoi(cols[6]);
        } catch (...) {
            continue;
        }
        if (cols.size() > 7) hl.quotedText = unescapeTsv(cols[7]);
        if (cols.size() > 8) hl.comment = unescapeTsv(cols[8]);
        if (cols.size() > 9) hl.linkedNoteId = unescapeTsv(cols[9]);
        if (cols.size() > 10) hl.createdAt = unescapeTsv(cols[10]);

        doc.highlights.push_back(std::move(hl));
    }
}

}
