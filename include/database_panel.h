#pragma once

#include "database_manager.h"
#include <string>

namespace nodepanda {

class App;

class DatabasePanel {
public:
    void render(App& app);

private:
    bool m_initialized = false;
    int m_engineIdx = 1;
    int m_modeIdx = 0;
    char m_host[128] = "127.0.0.1";
    int m_port = 5432;
    char m_database[128] = "nodepanda";
    char m_user[128] = "nodepanda";
    char m_password[128] = "";
    bool m_autoSync = true;
    char m_sqlInput[2048] = "SELECT id, title FROM notes LIMIT 20;";
    std::string m_statusMessage;
    DbQueryResult m_lastQueryResult;
};

}
