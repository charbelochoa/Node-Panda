#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace nodepanda {

struct GitCommitInfo {
    std::string hash;
    std::string message;
    std::string date;
    std::string author;
    int filesChanged = 0;
};

struct GitDiffLine {
    std::string text;
    int type = 0;
};

class GitManager {
public:

    bool init(const std::filesystem::path& notesDir);

    bool autoCommit(const std::string& message = "");

    std::vector<GitCommitInfo> getHistory(const std::string& filename = "", int maxCount = 50) const;

    std::vector<GitDiffLine> getDiff(const std::string& filename) const;

    std::vector<GitDiffLine> getDiffBetween(const std::string& commitHash1,
                                             const std::string& commitHash2,
                                             const std::string& filename) const;

    bool restoreFile(const std::string& commitHash, const std::string& filename);

    bool isAvailable() const { return m_available; }

    bool hasChanges() const;

    struct RepoStatus {
        int modified = 0;
        int added = 0;
        int deleted = 0;
    };
    RepoStatus getStatus() const;

private:
    std::string runGitCommand(const std::string& args) const;

    std::filesystem::path m_notesDir;
    bool m_available = false;
};

}
