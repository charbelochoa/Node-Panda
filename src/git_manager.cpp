#include "git_manager.h"
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <chrono>
#include <iostream>

namespace nodepanda {

std::string GitManager::runGitCommand(const std::string& args) const {
    std::ostringstream command;

#ifdef _WIN32
    command << "cd \"" << m_notesDir.string() << "\" && git " << args << " 2>&1";
#else
    command << "cd \"" << m_notesDir.string() << "\" && git " << args << " 2>&1";
#endif

    std::string result;
    FILE* pipe = nullptr;

#ifdef _WIN32
    pipe = _popen(command.str().c_str(), "r");
#else
    pipe = popen(command.str().c_str(), "r");
#endif

    if (!pipe) {
        return "";
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return result;
}

bool GitManager::init(const std::filesystem::path& notesDir) {
    m_notesDir = notesDir;

    std::string versionOutput = runGitCommand("--version");
    if (versionOutput.empty() || versionOutput.find("git version") == std::string::npos) {
        m_available = false;
        return false;
    }

    m_available = true;

    std::filesystem::path gitDir = m_notesDir / ".git";
    if (std::filesystem::exists(gitDir)) {
        return true;
    }

    std::string initOutput = runGitCommand("init");
    if (initOutput.empty()) {
        return false;
    }

    runGitCommand("config user.email \"nodepanda@local\"");
    runGitCommand("config user.name \"Node Panda\"");

    runGitCommand("add -A");
    std::string commitOutput = runGitCommand("commit -m \"Initial commit\"");

    return true;
}

bool GitManager::autoCommit(const std::string& message) {
    if (!m_available) {
        return false;
    }

    runGitCommand("add -A");

    std::string commitMsg = message;
    if (commitMsg.empty()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        struct tm* timeinfo = localtime(&time);
        char buffer[100];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        commitMsg = std::string("Auto-commit: ") + buffer;
    }

    size_t pos = 0;
    while ((pos = commitMsg.find('"', pos)) != std::string::npos) {
        commitMsg.replace(pos, 1, "\\\"");
        pos += 2;
    }

    std::ostringstream cmd;
#ifdef _WIN32
    cmd << "commit -m \"" << commitMsg << "\"";
#else
    cmd << "commit -m \"" << commitMsg << "\"";
#endif

    std::string output = runGitCommand(cmd.str());

    return !output.empty() && output.find("nothing to commit") == std::string::npos;
}

std::vector<GitCommitInfo> GitManager::getHistory(const std::string& filename, int maxCount) const {
    std::vector<GitCommitInfo> history;

    if (!m_available) {
        return history;
    }

    std::ostringstream cmd;
    cmd << "log --pretty=format:\"%h|%s|%ai|%an\" --shortstat -" << maxCount;
    if (!filename.empty()) {
        cmd << " -- \"" << filename << "\"";
    }

    std::string output = runGitCommand(cmd.str());
    if (output.empty()) {
        return history;
    }

    std::istringstream iss(output);
    std::string line;

    GitCommitInfo currentCommit;
    bool inCommit = false;

    while (std::getline(iss, line)) {

        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        if (line.find('|') != std::string::npos) {
            if (inCommit && !currentCommit.hash.empty()) {
                history.push_back(currentCommit);
            }

            size_t pos1 = line.find('|');
            size_t pos2 = line.find('|', pos1 + 1);
            size_t pos3 = line.find('|', pos2 + 1);

            currentCommit.hash = line.substr(0, pos1);
            currentCommit.message = line.substr(pos1 + 1, pos2 - pos1 - 1);

            std::string dateStr = line.substr(pos2 + 1, pos3 - pos2 - 1);
            if (dateStr.length() >= 16) {
                currentCommit.date = dateStr.substr(0, 16);
            } else {
                currentCommit.date = dateStr;
            }

            currentCommit.author = line.substr(pos3 + 1);
            currentCommit.filesChanged = 0;
            inCommit = true;
        } else if (inCommit && line.find("changed") != std::string::npos) {

            std::istringstream shortstatStream(line);
            int files = 0;
            shortstatStream >> files;
            currentCommit.filesChanged = files;
        }
    }

    if (inCommit && !currentCommit.hash.empty()) {
        history.push_back(currentCommit);
    }

    return history;
}

std::vector<GitDiffLine> GitManager::getDiff(const std::string& filename) const {
    std::vector<GitDiffLine> diff;

    if (!m_available || filename.empty()) {
        return diff;
    }

    std::ostringstream cmd;
    cmd << "diff -- \"" << filename << "\"";

    std::string output = runGitCommand(cmd.str());
    if (output.empty()) {
        return diff;
    }

    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.empty()) {
            continue;
        }

        GitDiffLine diffLine;
        if (line[0] == '+' && line.substr(0, 3) != "+++") {
            diffLine.type = 1;
            diffLine.text = line.substr(1);
        } else if (line[0] == '-' && line.substr(0, 3) != "---") {
            diffLine.type = -1;
            diffLine.text = line.substr(1);
        } else if (line[0] == ' ' || line[0] == '@') {
            diffLine.type = 0;
            if (line[0] == '@') {
                diffLine.text = line;
            } else {
                diffLine.text = line.substr(1);
            }
        } else {
            diffLine.type = 0;
            diffLine.text = line;
        }

        diff.push_back(diffLine);
    }

    return diff;
}

std::vector<GitDiffLine> GitManager::getDiffBetween(const std::string& commitHash1,
                                                      const std::string& commitHash2,
                                                      const std::string& filename) const {
    std::vector<GitDiffLine> diff;

    if (!m_available || filename.empty()) {
        return diff;
    }

    std::ostringstream cmd;
    cmd << "diff " << commitHash1 << ".." << commitHash2 << " -- \"" << filename << "\"";

    std::string output = runGitCommand(cmd.str());
    if (output.empty()) {
        return diff;
    }

    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.empty()) {
            continue;
        }

        GitDiffLine diffLine;
        if (line[0] == '+' && line.substr(0, 3) != "+++") {
            diffLine.type = 1;
            diffLine.text = line.substr(1);
        } else if (line[0] == '-' && line.substr(0, 3) != "---") {
            diffLine.type = -1;
            diffLine.text = line.substr(1);
        } else if (line[0] == ' ' || line[0] == '@') {
            diffLine.type = 0;
            if (line[0] == '@') {
                diffLine.text = line;
            } else {
                diffLine.text = line.substr(1);
            }
        } else {
            diffLine.type = 0;
            diffLine.text = line;
        }

        diff.push_back(diffLine);
    }

    return diff;
}

bool GitManager::restoreFile(const std::string& commitHash, const std::string& filename) {
    if (!m_available || filename.empty() || commitHash.empty()) {
        return false;
    }

    std::ostringstream cmd;
    cmd << "checkout " << commitHash << " -- \"" << filename << "\"";

    std::string output = runGitCommand(cmd.str());
    return !output.empty() || output.find("error") == std::string::npos;
}

bool GitManager::hasChanges() const {
    if (!m_available) {
        return false;
    }

    std::string output = runGitCommand("status --porcelain");
    return !output.empty();
}

GitManager::RepoStatus GitManager::getStatus() const {
    RepoStatus status;

    if (!m_available) {
        return status;
    }

    std::string output = runGitCommand("status --porcelain");
    if (output.empty()) {
        return status;
    }

    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.length() >= 3) {
            std::string status_code = line.substr(0, 2);

            if (status_code[0] == 'M' || status_code[1] == 'M') {
                status.modified++;
            } else if (status_code[0] == 'A' || status_code[1] == 'A') {
                status.added++;
            } else if (status_code[0] == 'D' || status_code[1] == 'D') {
                status.deleted++;
            }
        }
    }

    return status;
}

}
