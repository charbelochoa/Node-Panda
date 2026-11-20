#pragma once

#include <string>
#include <vector>

namespace nodepanda {

class App;

enum class LaunchMode {
    GuiApp,
    HeadlessServer,
    CliFinished
};

struct CliLaunchResult {
    LaunchMode mode = LaunchMode::CliFinished;
    int exitCode = 0;
    int serverPort = 8042;
    std::string authenticatedUser;
};

class CliRunner {
public:
    CliLaunchResult run(int argc, char** argv, App& app);

private:
    bool ensureAuthenticated(App& app, bool skipPrompt);
    int runInteractiveLauncher(App& app, CliLaunchResult& outResult);
    int runChatRepl(App& app, const std::string& agentName, const std::string& room, const std::string& model);
    int handleGraphCommand(App& app, const std::vector<std::string>& args);
    int handleListCommand(App& app, const std::vector<std::string>& args);
    int handleUpsertCommand(App& app, const std::vector<std::string>& args);
    int handleShowCommand(App& app, const std::vector<std::string>& args);
    int handleSearchCommand(App& app, const std::vector<std::string>& args);
    int handleQueryCommand(App& app, const std::vector<std::string>& args);
    int handleDbCommand(App& app, const std::vector<std::string>& args);
    int handleSyncCommand(App& app, const std::vector<std::string>& args);
    int handleBackupCommand(App& app, const std::vector<std::string>& args);
    int handleRunCommand(App& app, const std::vector<std::string>& args);
    int handleLabCommand(App& app, const std::string& subcmd, const std::vector<std::string>& args);
    int handleVectorCommand(App& app, const std::vector<std::string>& args);
    int handleRtcCommand(App& app, const std::vector<std::string>& args);
    int handlePdfCommand(App& app, const std::vector<std::string>& args);
    int handleTableCommand(App& app, const std::vector<std::string>& args);
    int handleMobileCommand(App& app, const std::vector<std::string>& args, int defaultPort);
    int handleDoctorCommand(App& app);
    void printHelp() const;
    static std::string getFlag(const std::vector<std::string>& args, const std::string& flag, const std::string& def = "");
    static bool hasFlag(const std::vector<std::string>& args, const std::string& flag);
};

}
