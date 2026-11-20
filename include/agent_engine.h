#pragma once

#include "ai_catalog.h"
#include "ai_provider.h"
#include "context_brain.h"
#include "note_manager.h"
#include "skill_manager.h"
#include "usage_ledger.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nodepanda {

struct AgentIteration {
    int index = 0;
    double score = 0.0;
    int64_t costNano = 0;
    int64_t tokens = 0;
    int64_t latencyMs = 0;
    std::string verdict;
    std::string candidate;
    bool accepted = false;
};

struct AgentStep {
    std::string role;
    std::string model;
    std::string detail;
};

struct AgentRun {
    std::string id;
    std::string owner;
    std::string title;
    std::string prompt;
    std::string model;
    std::string reviewer;
    std::string room;
    std::string status;
    std::string stopReason;
    std::string result;
    std::string error;
    double budgetUsd = 0.50;
    int maxIterations = 8;
    double targetScore = 90.0;
    TokenUsage usage;
    int64_t costNano = 0;
    std::vector<AgentIteration> iterations;
    std::vector<AgentStep> steps;
    std::string createdAt;
    std::string updatedAt;
};

struct RoomMessage {
    std::string id;
    std::string room;
    std::string from;
    std::string to;
    std::string text;
    std::string createdAt;
};

struct AgentEvent {
    std::string id;
    std::string runId;
    std::string type;
    std::string text;
    std::string createdAt;
};

class AgentEngine {
public:
    AgentEngine();
    ~AgentEngine();

    void init(
        const std::filesystem::path& workspaceDir,
        AiCatalog* catalog,
        ContextBrain* brain,
        SkillManager* skills,
        UsageLedger* ledger,
        NoteManager* nm);

    AgentRun createAndStartRun(
        const std::string& owner,
        const std::string& prompt,
        const std::string& model = "auto",
        const std::string& reviewer = "auto",
        const std::string& room = "general",
        double budgetUsd = -1.0,
        int maxIterations = -1);

    bool stopRun(const std::string& runId);
    const AgentRun* findRun(const std::string& runId) const;
    std::vector<AgentRun> listRuns() const;

    RoomMessage postMessage(const std::string& room, const std::string& from, const std::string& to, const std::string& text);
    std::vector<RoomMessage> listMessages(const std::string& room) const;
    std::vector<AgentEvent> listEvents(size_t maxCount = 50) const;

    std::string executeTool(
        const std::string& toolName,
        const std::map<std::string, std::string>& args,
        const std::string& room,
        const std::string& agentName);

    std::string selectBestModel(const std::string& requestedModel) const;
    void shutdown();

private:
    void runMissionWorker(std::string runId);
    void emitEventLocked(const std::string& runId, const std::string& type, const std::string& text);
    bool isStopRequested(const std::string& runId) const;
    double scoreCandidateDeterministically(const std::string& prompt, const std::string& candidate, bool usedFallback) const;

    std::filesystem::path m_workspaceDir;
    AiCatalog* m_catalog = nullptr;
    ContextBrain* m_brain = nullptr;
    SkillManager* m_skills = nullptr;
    UsageLedger* m_ledger = nullptr;
    NoteManager* m_noteManager = nullptr;

    mutable std::mutex m_mutex;
    std::deque<AgentRun> m_runs;
    std::vector<RoomMessage> m_messages;
    std::vector<AgentEvent> m_events;
    std::map<std::string, bool> m_stopFlags;

    std::mutex m_threadMutex;
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_shuttingDown{ false };
};

}
