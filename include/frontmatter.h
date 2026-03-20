#pragma once


#include <string>
#include <map>

namespace nodepanda {

class FrontmatterUtils {
public:
    static std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    static bool hasFrontmatter(const std::string& content) {
        return content.size() >= 3 && content.substr(0, 3) == "---";
    }
};

} 
