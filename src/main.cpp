#include "app.h"
#include "cli_runner.h"

int main(int argc, char** argv) {
    nodepanda::App app;
    app.initCore();

    nodepanda::CliRunner cli;
    auto launch = cli.run(argc, argv, app);

    if (launch.mode == nodepanda::LaunchMode::CliFinished) {
        app.shutdown();
        return launch.exitCode;
    }

    if (launch.mode == nodepanda::LaunchMode::HeadlessServer) {
        app.runHeadlessServer(launch.serverPort);
        app.shutdown();
        return 0;
    }

    if (!app.init("Node Panda", 1480, 920)) {
        app.shutdown();
        return -1;
    }

    app.run();
    app.shutdown();
    return 0;
}
