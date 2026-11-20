#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>

namespace nodepanda {
class App;

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    void start(App* app, int port = 8042);
    void stop();
    bool isRunning() const { return m_running.load(); }
    int getPort() const { return m_port; }

private:
    void serverLoop();
    std::string handleRequest(const std::string& method, const std::string& path,
                              const std::string& body);
    std::string buildJsonNoteList();
    std::string buildJsonNote(const std::string& id);
    std::string buildJsonGraph();
    std::string buildJsonSearch(const std::string& query, int topN);

    static std::string httpResponse(int code, const std::string& body,
                                     const std::string& contentType = "application/json");
    static std::string urlDecode(const std::string& str);
    static std::string extractParam(const std::string& path, const std::string& param);

    App* m_app = nullptr;
    int m_port = 8042;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    uintptr_t m_serverSocket = ~(uintptr_t)0;
};

}
