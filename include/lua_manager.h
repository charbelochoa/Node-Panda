#pragma once


#include <string>
#include <vector>
#include <functional>


struct lua_State;

namespace nodepanda {

class App;  

struct LuaResult {
    bool        success = false;
    std::string output;          
    std::string error;           
    double      elapsedMs = 0.0; 
};

struct LuaLogEntry {
    enum Type { OUTPUT, LOG_ERROR, INFO };
    Type        type;
    std::string text;
    std::string timestamp;  
};

class LuaManager {
public:
    LuaManager();
    ~LuaManager();

    void init(App* app);

    LuaResult execute(const std::string& code);

    const std::vector<LuaLogEntry>& getLog() const { return m_log; }
    void clearLog();

    void logInfo(const std::string& text);

    int timeoutMs = 5000;

private:
    void setupSandbox();
    void registerBindings();
    static std::string currentTimestamp();

    lua_State*               m_lua = nullptr;
    App*                     m_app = nullptr;
    std::vector<LuaLogEntry> m_log;
    std::string              m_capturedOutput;  
};

}
