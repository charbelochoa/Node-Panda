#pragma once

#include <string>

namespace nodepanda {

class App;

class RtcPanel {
public:
    void render(App& app);

private:
    int m_port = 8043;
    char m_bindHostBuf[64] = "127.0.0.1";
    char m_remoteAddrBuf[128] = "127.0.0.1:8043";
    char m_usernameBuf[64] = "local";
    char m_blockUuidBuf[64] = {};
    char m_deltaContentBuf[256] = {};
    bool m_userInitialized = false;
    std::string m_lastTrackedNoteId;
    std::string m_statusMsg;
    bool m_statusIsError = false;
};

}
