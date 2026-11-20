#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace nodepanda {

class App;

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    void start(App* app, int port = 8042, const std::string& bindHost = "127.0.0.1");
    void stop();
    bool isRunning() const { return m_running.load(); }
    int getPort() const { return m_port; }
    const std::string& getBindHost() const { return m_bindHost; }

private:
    void serverLoop();
    std::string handleRequest(const std::string& method, const std::string& path,
                              const std::string& headers, const std::string& body);
    std::string buildJsonHealth();
    std::string buildJsonSnapshot();
    std::string buildJsonNoteList();
    std::string buildJsonNote(const std::string& id);
    std::string buildJsonGraph();
    std::string buildJsonSearch(const std::string& query, int topN);
    std::string buildJsonBlocks(const std::string& pageId);
    std::string buildJsonTasks();
    std::string buildJsonRuns();
    std::string buildJsonMessages(const std::string& room);
    std::string buildJsonSkills();
    std::string buildJsonDbStatus();
    std::string buildJsonSyncStatus();
    std::string handleAuthLogin(const std::string& body);
    std::string handleAuthRegister(const std::string& body);
    std::string handleCreateRun(const std::string& body);
    std::string handlePostMessage(const std::string& body);
    std::string handleDatalogQuery(const std::string& body);
    std::string handleUpsertBlock(const std::string& body);
    std::string handleContextRetrieve(const std::string& body);

    static std::string jsonEscape(const std::string& s);
    static std::string extractJsonString(const std::string& json, const std::string& key);
    static std::string httpResponse(int code, const std::string& body,
                                    const std::string& contentType = "application/json");
    static std::string urlDecode(const std::string& str);
    static std::string extractParam(const std::string& path, const std::string& param);

    App* m_app = nullptr;
    int m_port = 8042;
    std::string m_bindHost = "127.0.0.1";
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    uintptr_t m_serverSocket = ~static_cast<uintptr_t>(0);
};

}
