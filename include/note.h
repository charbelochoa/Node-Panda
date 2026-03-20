#pragma once
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>

namespace nodepanda {

struct Note {
    std::string id;                      
    std::string title;                   
    std::string content;                 
    std::filesystem::path filepath;      
    std::vector<std::string> links;      
    std::map<std::string, std::string> frontmatter;  
    bool dirty = false;
    std::chrono::system_clock::time_point lastModified;

    bool loadFromFile();

    bool saveToFile();

    void parseLinks();

    void parseTitle();

    std::string getFrontmatter(const std::string& key, const std::string& defaultVal = "") const;
    void setFrontmatter(const std::string& key, const std::string& value);

    std::string serializeFrontmatter() const;
};

} 
