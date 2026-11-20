#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nodepanda {

struct RtcPeer {
    std::string peerId;
    std::string username;
    std::string editingNoteId;
    std::string editingBlockUuid;
    std::string status;
    int64_t lastSeenUnix = 0;
    uintptr_t socketHandle = 0;
};

struct RtcEvent {
    std::string id;
    std::string type;
    std::string peer;
    std::string payload;
    std::string timestamp;
};

struct RtcStats {
    bool serverRunning = false;
    int port = 8043;
    int connectedPeers = 0;
    int64_t messagesSent = 0;
    int64_t messagesReceived = 0;
    int64_t currentRev = 0;
};

class WebSocketRtcServer {
public:
    WebSocketRtcServer();
    ~WebSocketRtcServer();

    void start(int port = 8043, const std::string& bindHost = "127.0.0.1");
    void stop();
    bool isRunning() const;

    void setLocalPresence(const std::string& username, const std::string& noteId, const std::string& blockUuid = "");
    void broadcastDelta(const std::string& entityId, const std::string& op, const std::string& content);
    void broadcastMessage(const std::string& type, const std::string& jsonPayload);
    bool connectRemotePeer(const std::string& host, int port, const std::string& username, std::string& errorOut);

    std::vector<RtcPeer> listPeers() const;
    std::vector<RtcEvent> recentEvents(size_t maxCount = 60) const;
    RtcStats stats() const;
    std::string getBindHost() const;

    static std::string computeWebSocketAccept(const std::string& clientKey);

private:
    struct PeerConn;
    struct WorkerEntry {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> done;
    };

    void serverLoop();
    void peerLoop(std::shared_ptr<PeerConn> peer,
                  std::vector<uint8_t> initialBytes,
                  std::shared_ptr<std::atomic<bool>> doneFlag);
    bool performServerHandshake(uintptr_t sockHandle,
                                std::vector<uint8_t>& leftover,
                                std::string& peerUserHint);
    void handleIncomingText(const std::shared_ptr<PeerConn>& peer, const std::string& text);
    bool sendFrameToPeer(const std::shared_ptr<PeerConn>& peer, uint8_t opcode, const std::string& payload);
    void broadcastRawText(const std::string& text, const std::string& excludePeerId = "");
    void removePeer(const std::string& peerId);
    void recordEvent(const std::string& type, const std::string& peer, const std::string& payload);
    void reapFinishedWorkers();
    void closeAllPeers();

    mutable std::mutex m_mutex;
    std::mutex m_threadMutex;

    int m_port = 8043;
    std::string m_bindHost = "127.0.0.1";
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_destroying{false};
    std::atomic<int64_t> m_messagesSent{0};
    std::atomic<int64_t> m_messagesReceived{0};
    std::atomic<int64_t> m_currentRev{0};
    std::atomic<uint64_t> m_eventSeq{0};
    std::atomic<uint64_t> m_peerSeq{0};

    std::string m_localUsername = "local";
    std::string m_localNoteId;
    std::string m_localBlockUuid;

    std::thread m_serverThread;
    uintptr_t m_serverSocket = ~static_cast<uintptr_t>(0);
    bool m_wsaReady = false;

    std::vector<std::shared_ptr<PeerConn>> m_peers;
    std::vector<RtcEvent> m_events;
    std::vector<WorkerEntry> m_workers;
};

}
