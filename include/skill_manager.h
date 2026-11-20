#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace nodepanda {

struct AgentSkill {
    std::string id;
    std::string name;
    std::string description;
    std::string content;
    bool enabled = true;
    bool builtin = false;
};

class SkillManager {
public:
    void init(const std::filesystem::path& skillsDir);
    void scanSkills();
    bool addSkill(const std::string& name, const std::string& description, const std::string& content);
    bool setEnabled(const std::string& id, bool enabled);
    bool removeSkill(const std::string& id);
    const std::vector<AgentSkill>& getSkills() const;
    std::string buildActiveSkillsPrompt() const;

private:
    std::string slugify(const std::string& name) const;
    void saveState() const;
    void loadState();

    std::filesystem::path m_skillsDir;
    std::vector<AgentSkill> m_skills;
    std::vector<std::string> m_disabledIds;
};

}
