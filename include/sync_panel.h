#pragma once

#include <string>

namespace nodepanda {

class App;

class SyncPanel {
public:
    void render(App& app);

private:
    char m_httpBaseBuf[256] = {};
    char m_wsUrlBuf[256] = {};
    bool m_transportLoaded = false;

    char m_e2eePasswordBuf[128] = {};
    char m_e2eeVerifyBuf[128] = {};

    char m_snapshotPathBuf[512] = {};
    char m_snapshotPasswordBuf[128] = {};
    bool m_encryptSnapshot = true;

    char m_backupLabelBuf[128] = {};

    std::string m_statusMsg;
    bool m_statusIsError = false;
};

}
