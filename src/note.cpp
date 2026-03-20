#include "note.h"
#include <fstream>
#include <sstream>
#include <regex>

namespace nodepanda {


static std::string trimStr(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}


bool Note::loadFromFile() {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string raw = ss.str();

    frontmatter.clear();
    content.clear();

    if (raw.size() >= 3 && raw.substr(0, 3) == "---") {
        size_t afterFirst = raw.find('\n', 0);
        if (afterFirst == std::string::npos) {
            content = raw;
            parseLinks();
            parseTitle();
            dirty = false;
            return true;
        }
        afterFirst++; 

        size_t secondDelim = std::string::npos;
        size_t searchFrom = afterFirst;
        while (searchFrom < raw.size()) {
            size_t lineEnd = raw.find('\n', searchFrom);
            std::string line;
            if (lineEnd == std::string::npos) {
                line = raw.substr(searchFrom);
                lineEnd = raw.size();
            } else {
                line = raw.substr(searchFrom, lineEnd - searchFrom);
            }
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (trimStr(line) == "---") {
                secondDelim = searchFrom;
                break;
            }
            searchFrom = lineEnd + 1;
        }

        if (secondDelim != std::string::npos) {
            std::string yamlBlock = raw.substr(afterFirst, secondDelim - afterFirst);
            std::istringstream yamlStream(yamlBlock);
            std::string yamlLine;
            while (std::getline(yamlStream, yamlLine)) {
                if (!yamlLine.empty() && yamlLine.back() == '\r') yamlLine.pop_back();
                size_t colonPos = yamlLine.find(':');
                if (colonPos != std::string::npos) {
                    std::string key = trimStr(yamlLine.substr(0, colonPos));
                    std::string value = trimStr(yamlLine.substr(colonPos + 1));
                    if (!key.empty()) {
                        frontmatter[key] = value;
                    }
                }
            }

            size_t bodyStart = raw.find('\n', secondDelim);
            if (bodyStart != std::string::npos) {
                bodyStart++; // saltar el '\n' del "---"
                content = raw.substr(bodyStart);
            } else {
                content = "";
            }
        } else {
            content = raw;
        }
    } else {
        content = raw;
    }

    parseLinks();
    parseTitle();
    dirty = false;
    return true;
}


bool Note::saveToFile() {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    std::string output = serializeFrontmatter() + content;
    file << output;
    file.close();

    dirty = false;
    lastModified = std::chrono::system_clock::now();
    return true;
}


std::string Note::serializeFrontmatter() const {
    if (frontmatter.empty()) return "";

    std::ostringstream ss;
    ss << "---\n";
    for (const auto& [key, value] : frontmatter) {
        ss << key << ": " << value << "\n";
    }
    ss << "---\n";
    return ss.str();
}


std::string Note::getFrontmatter(const std::string& key, const std::string& defaultVal) const {
    auto it = frontmatter.find(key);
    if (it != frontmatter.end()) return it->second;
    return defaultVal;
}

void Note::setFrontmatter(const std::string& key, const std::string& value) {
    frontmatter[key] = value;
    dirty = true;
}


void Note::parseLinks() {
    links.clear();
    std::regex linkRegex(R"(\[\[(.+?)\]\])");
    auto begin = std::sregex_iterator(content.begin(), content.end(), linkRegex);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::smatch match = *it;
        std::string linkName = match[1].str();
        size_t slashPos = linkName.rfind('/');
        if (slashPos != std::string::npos) {
            linkName = linkName.substr(slashPos + 1);
        }
        bool found = false;
        for (const auto& l : links) {
            if (l == linkName) { found = true; break; }
        }
        if (!found) links.push_back(linkName);
    }
}


void Note::parseTitle() {
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() >= 2 && line[0] == '#' && line[1] == ' ') {
            title = line.substr(2);
            return;
        }
    }
    title = id;
}

} 
