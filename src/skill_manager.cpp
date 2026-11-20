#include "skill_manager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace nodepanda {

namespace {

std::string trimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

void parseSkillMarkdown(const std::string& raw, const std::string& fallbackId, AgentSkill& out) {
    out.id = fallbackId;
    out.name = fallbackId;
    out.description.clear();
    out.content = raw;

    std::istringstream iss(raw);
    std::string line;
    bool inFrontmatter = false;
    bool parsedFrontmatter = false;
    std::ostringstream body;
    int lineIdx = 0;

    while (std::getline(iss, line)) {
        std::string trimmed = trimStr(line);
        if (lineIdx == 0 && trimmed == "---") {
            inFrontmatter = true;
            ++lineIdx;
            continue;
        }
        ++lineIdx;

        if (inFrontmatter) {
            if (trimmed == "---") {
                inFrontmatter = false;
                parsedFrontmatter = true;
                continue;
            }
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string k = trimStr(line.substr(0, colon));
                std::string v = trimStr(line.substr(colon + 1));
                if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) {
                    v = v.substr(1, v.size() - 2);
                }
                if (k == "name" && !v.empty()) out.name = v;
                else if (k == "description") out.description = v;
                else if (k == "builtin") out.builtin = (v == "true" || v == "1");
            }
            continue;
        }

        if (!parsedFrontmatter && out.name == fallbackId && trimmed.rfind("# ", 0) == 0) {
            out.name = trimStr(trimmed.substr(2));
        }
        body << line << "\n";
    }

    if (parsedFrontmatter) {
        out.content = trimStr(body.str());
    }
    if (out.description.empty()) {
        std::istringstream bss(out.content);
        std::string bl;
        while (std::getline(bss, bl)) {
            std::string t = trimStr(bl);
            if (!t.empty() && t[0] != '#') {
                out.description = t.size() > 120 ? (t.substr(0, 117) + "...") : t;
                break;
            }
        }
    }
}

}

std::string SkillManager::slugify(const std::string& name) const {
    std::string out;
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == ' ' || c == '-' || c == '_') {
            if (!out.empty() && out.back() != '_') out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty()) out = "skill_custom";
    return out;
}

void SkillManager::init(const std::filesystem::path& skillsDir) {
    m_skillsDir = skillsDir;
    std::error_code ec;
    if (!m_skillsDir.empty() && !std::filesystem::exists(m_skillsDir, ec)) {
        std::filesystem::create_directories(m_skillsDir, ec);
    }
    loadState();
    scanSkills();
}

void SkillManager::scanSkills() {
    m_skills.clear();
    if (m_skillsDir.empty()) return;
    std::error_code ec;
    if (!std::filesystem::exists(m_skillsDir, ec)) return;

    for (const auto& entry : std::filesystem::directory_iterator(m_skillsDir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".md") continue;

        std::ifstream f(entry.path());
        if (!f.is_open()) continue;
        std::ostringstream ss;
        ss << f.rdbuf();

        AgentSkill skill;
        std::string stem = entry.path().stem().string();
        parseSkillMarkdown(ss.str(), stem, skill);
        skill.enabled = (std::find(m_disabledIds.begin(), m_disabledIds.end(), skill.id) == m_disabledIds.end());
        m_skills.push_back(std::move(skill));
    }

    std::sort(m_skills.begin(), m_skills.end(), [](const AgentSkill& a, const AgentSkill& b) {
        return a.name < b.name;
    });
}

bool SkillManager::addSkill(const std::string& name, const std::string& description, const std::string& content) {
    if (name.empty() || content.empty() || m_skillsDir.empty()) return false;
    std::string id = slugify(name);
    std::filesystem::path filePath = m_skillsDir / (id + ".md");

    std::ofstream out(filePath, std::ios::trunc);
    if (!out.is_open()) return false;
    out << "---\n";
    out << "name: " << name << "\n";
    out << "description: " << description << "\n";
    out << "builtin: false\n";
    out << "---\n\n";
    out << content << "\n";
    out.close();

    scanSkills();
    return true;
}

bool SkillManager::setEnabled(const std::string& id, bool enabled) {
    bool found = false;
    for (auto& s : m_skills) {
        if (s.id == id) {
            s.enabled = enabled;
            found = true;
            break;
        }
    }
    if (!found) return false;

    auto it = std::find(m_disabledIds.begin(), m_disabledIds.end(), id);
    if (enabled) {
        if (it != m_disabledIds.end()) m_disabledIds.erase(it);
    } else {
        if (it == m_disabledIds.end()) m_disabledIds.push_back(id);
    }
    saveState();
    return true;
}

bool SkillManager::removeSkill(const std::string& id) {
    auto it = std::find_if(m_skills.begin(), m_skills.end(), [&](const AgentSkill& s) {
        return s.id == id;
    });
    if (it == m_skills.end()) return false;
    if (it->builtin) return false;

    std::error_code ec;
    std::filesystem::remove(m_skillsDir / (id + ".md"), ec);
    m_skills.erase(it);
    m_disabledIds.erase(std::remove(m_disabledIds.begin(), m_disabledIds.end(), id), m_disabledIds.end());
    saveState();
    return true;
}

const std::vector<AgentSkill>& SkillManager::getSkills() const {
    return m_skills;
}

std::string SkillManager::buildActiveSkillsPrompt() const {
    std::ostringstream oss;
    int activeCount = 0;
    for (const auto& s : m_skills) {
        if (!s.enabled) continue;
        if (activeCount == 0) {
            oss << "<active_skills>\n";
        }
        ++activeCount;
        oss << "<skill id=\"" << s.id << "\" name=\"" << s.name << "\">\n";
        oss << s.content << "\n</skill>\n";
    }
    if (activeCount > 0) {
        oss << "</active_skills>";
    }
    return oss.str();
}

void SkillManager::saveState() const {
    if (m_skillsDir.empty()) return;
    std::ofstream out(m_skillsDir / ".skills_state", std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& id : m_disabledIds) {
        out << id << "\n";
    }
}

void SkillManager::loadState() {
    m_disabledIds.clear();
    if (m_skillsDir.empty()) return;
    std::ifstream in(m_skillsDir / ".skills_state");
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trimStr(line);
        if (!t.empty()) m_disabledIds.push_back(t);
    }
}

}
