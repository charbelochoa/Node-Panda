#include "agent_engine.h"
#include "crypto_engine.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <sstream>

namespace nodepanda {

namespace {

std::string nowIsoTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm buf{};
#ifdef _WIN32
    gmtime_s(&buf, &t);
#else
    gmtime_r(&t, &buf);
#endif
    char out[32];
    std::snprintf(out, sizeof(out), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  buf.tm_year + 1900, buf.tm_mon + 1, buf.tm_mday,
                  buf.tm_hour, buf.tm_min, buf.tm_sec);
    return std::string(out);
}

std::string buildTitleFromPrompt(const std::string& prompt) {
    std::string cleaned;
    for (char c : prompt) {
        if (c == '\n' || c == '\r' || c == '\t') cleaned.push_back(' ');
        else cleaned.push_back(c);
    }
    size_t a = cleaned.find_first_not_of(' ');
    if (a == std::string::npos) return "Mission";
    cleaned = cleaned.substr(a);
    if (cleaned.size() > 56) {
        cleaned = cleaned.substr(0, 53) + "...";
    }
    return cleaned;
}

double parseScoreFromReview(const std::string& reviewText, double fallbackScore) {
    std::string low = reviewText;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    size_t pos = low.find("score");
    if (pos == std::string::npos) pos = low.find("puntaje");
    if (pos == std::string::npos) return fallbackScore;

    for (size_t i = pos; i < low.size() && i < pos + 40; ++i) {
        if (std::isdigit(static_cast<unsigned char>(low[i]))) {
            size_t end = i;
            while (end < low.size() && (std::isdigit(static_cast<unsigned char>(low[end])) || low[end] == '.')) {
                ++end;
            }
            try {
                double v = std::stod(low.substr(i, end - i));
                if (v >= 0.0 && v <= 100.0) return v;
            } catch (...) {}
            break;
        }
    }
    return fallbackScore;
}

}

AgentEngine::AgentEngine() = default;

AgentEngine::~AgentEngine() {
    shutdown();
}

void AgentEngine::init(
    const std::filesystem::path& workspaceDir,
    AiCatalog* catalog,
    ContextBrain* brain,
    SkillManager* skills,
    UsageLedger* ledger,
    NoteManager* nm) {
    m_workspaceDir = workspaceDir;
    m_catalog = catalog;
    m_brain = brain;
    m_skills = skills;
    m_ledger = ledger;
    m_noteManager = nm;
    m_shuttingDown.store(false);
}

std::string AgentEngine::selectBestModel(const std::string& requestedModel) const {
    if (!m_catalog) return "qwen2.5-0.5b-instruct";

    if (!requestedModel.empty() && requestedModel != "auto") {
        if (m_catalog->findModel(requestedModel)) {
            return requestedModel;
        }
    }

    const auto& cfg = m_catalog->settings();
    if (cfg.router && !cfg.routerModel.empty()) {
        const ModelSpec* rm = m_catalog->findModel(cfg.routerModel);
        if (rm) {
            const ProviderSpec* rp = m_catalog->findProvider(rm->provider);
            if (rp && rp->enabled && m_catalog->hasApiKey(rm->provider)) {
                return rm->id;
            }
        }
    }

    for (const auto& m : m_catalog->models()) {
        const ProviderSpec* p = m_catalog->findProvider(m.provider);
        if (!p || !p->enabled) continue;
        if (m.provider != ProviderId::Local && m_catalog->hasApiKey(m.provider)) {
            return m.id;
        }
    }

    for (const auto& m : m_catalog->models()) {
        if (m.provider == ProviderId::Local) {
            return m.id;
        }
    }

    return m_catalog->models().empty() ? "qwen2.5-0.5b-instruct" : m_catalog->models().front().id;
}

void AgentEngine::emitEventLocked(const std::string& runId, const std::string& type, const std::string& text) {
    AgentEvent ev;
    ev.id = "ev_" + CryptoEngine::randomHex(4);
    ev.runId = runId;
    ev.type = type;
    ev.text = text;
    ev.createdAt = nowIsoTimestamp();
    m_events.push_back(std::move(ev));
    if (m_events.size() > 500) {
        m_events.erase(m_events.begin(), m_events.begin() + 100);
    }
}

bool AgentEngine::isStopRequested(const std::string& runId) const {
    if (m_shuttingDown.load()) return true;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stopFlags.find(runId);
    return it != m_stopFlags.end() && it->second;
}

double AgentEngine::scoreCandidateDeterministically(const std::string& prompt, const std::string& candidate, bool usedFallback) const {
    if (candidate.empty()) return 0.0;
    double base = usedFallback ? 76.0 : 82.0;
    if (candidate.size() >= 120) base += 5.0;
    if (candidate.size() >= 320) base += 4.0;
    if (candidate.find('\n') != std::string::npos) base += 3.0;

    std::string pLow = prompt;
    std::string cLow = candidate;
    std::transform(pLow.begin(), pLow.end(), pLow.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(cLow.begin(), cLow.end(), cLow.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::istringstream iss(pLow);
    std::string tok;
    int matched = 0;
    int total = 0;
    while (iss >> tok) {
        if (tok.size() < 4) continue;
        ++total;
        if (cLow.find(tok) != std::string::npos) ++matched;
    }
    if (total > 0) {
        base += 6.0 * (static_cast<double>(matched) / static_cast<double>(total));
    }
    return std::clamp(base, 0.0, 98.0);
}

AgentRun AgentEngine::createAndStartRun(
    const std::string& owner,
    const std::string& prompt,
    const std::string& model,
    const std::string& reviewer,
    const std::string& room,
    double budgetUsd,
    int maxIterations) {
    AgentRun run;
    run.id = "run_" + CryptoEngine::randomHex(5);
    run.owner = owner.empty() ? "operator" : owner;
    run.title = buildTitleFromPrompt(prompt);
    run.prompt = prompt;
    run.model = selectBestModel(model);
    run.reviewer = (reviewer.empty() || reviewer == "auto") ? run.model : selectBestModel(reviewer);
    run.room = room.empty() ? "general" : room;
    run.status = "running";
    run.createdAt = nowIsoTimestamp();
    run.updatedAt = run.createdAt;

    if (m_catalog) {
        const auto& cfg = m_catalog->settings();
        run.budgetUsd = (budgetUsd > 0.0) ? budgetUsd : cfg.budgetUsd;
        run.maxIterations = (maxIterations > 0) ? maxIterations : cfg.maxIterations;
        run.targetScore = cfg.targetScore;
    } else {
        run.budgetUsd = (budgetUsd > 0.0) ? budgetUsd : 0.50;
        run.maxIterations = (maxIterations > 0) ? maxIterations : 4;
        run.targetScore = 90.0;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_runs.push_back(run);
        m_stopFlags[run.id] = false;
        emitEventLocked(run.id, "run_started", "Mission started with model " + run.model);
    }

    postMessage(run.room, run.owner, "agent:" + run.model, run.prompt);

    if (!m_shuttingDown.load()) {
        std::lock_guard<std::mutex> tlock(m_threadMutex);
        m_workers.emplace_back(&AgentEngine::runMissionWorker, this, run.id);
    }

    return run;
}

void AgentEngine::runMissionWorker(std::string runId) {
    AgentRun localRun;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& r : m_runs) {
            if (r.id == runId) {
                localRun = r;
                break;
            }
        }
    }
    if (localRun.id.empty() || !m_catalog) return;

    const ModelSpec* execModel = m_catalog->findModel(localRun.model);
    if (!execModel && !m_catalog->models().empty()) {
        execModel = &m_catalog->models().front();
    }
    if (!execModel) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& r : m_runs) {
            if (r.id == runId) {
                r.status = "failed";
                r.error = "No models available in catalog";
                r.updatedAt = nowIsoTimestamp();
                emitEventLocked(runId, "run_failed", r.error);
                break;
            }
        }
        return;
    }

    const ProviderSpec* execProv = m_catalog->findProvider(execModel->provider);

    std::string contextXml;
    if (m_brain && m_noteManager) {
        ContextRetrieval ctx = m_brain->retrieve(*m_noteManager, localRun.prompt, 1600);
        contextXml = ctx.formattedXml;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& r : m_runs) {
            if (r.id == runId) {
                r.steps.push_back({ "context", ctx.method, "Retrieved " + std::to_string(ctx.hits.size()) + " notes (~" + std::to_string(ctx.estimatedTokens) + " tokens)" });
                emitEventLocked(runId, "context_ready", "Retrieved " + std::to_string(ctx.hits.size()) + " vault hits");
                break;
            }
        }
    }

    std::string skillsXml = m_skills ? m_skills->buildActiveSkillsPrompt() : "";

    AgentSettings runCfg = m_catalog->settings();
    runCfg.budgetUsd = localRun.budgetUsd;
    runCfg.maxIterations = std::clamp(localRun.maxIterations, 1, 16);
    runCfg.targetScore = localRun.targetScore;

    std::vector<double> scoreHistory;
    std::string bestCandidate;
    double bestScore = -1.0;
    std::string lastFeedback;
    std::string stopReason;
    int64_t accumCostNano = 0;

    int maxIters = runCfg.optimizer ? runCfg.maxIterations : 1;

    for (int iter = 1; iter <= maxIters; ++iter) {
        if (isStopRequested(runId)) {
            stopReason = "stopped_by_user";
            break;
        }

        std::ostringstream sysPrompt;
        sysPrompt << "You are a disciplined Node Panda autonomous engineering agent. Provide concise, accurate, actionable output.\n";
        if (!skillsXml.empty()) {
            sysPrompt << "\n" << skillsXml << "\n";
        }
        if (!contextXml.empty()) {
            sysPrompt << "\n" << contextXml << "\n";
        }

        std::ostringstream userPrompt;
        userPrompt << localRun.prompt;
        if (!lastFeedback.empty()) {
            userPrompt << "\n\nRefinement feedback from iteration " << (iter - 1) << ":\n" << lastFeedback;
        }

        std::vector<ChatMsg> msgs = {
            { "system", sysPrompt.str() },
            { "user", userPrompt.str() }
        };

        CompletionResult res = AiProviderClient::complete(*m_catalog, *execModel, msgs, 1536);

        if (!res.ok) {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& r : m_runs) {
                if (r.id == runId) {
                    r.status = "failed";
                    r.error = res.error;
                    r.stopReason = "provider_error";
                    r.updatedAt = nowIsoTimestamp();
                    r.steps.push_back({ "error", execModel->id, res.error });
                    emitEventLocked(runId, "run_failed", res.error);
                    break;
                }
            }
            postMessage(localRun.room, "agent:" + execModel->id, localRun.owner, "Error: " + res.error);
            return;
        }

        bool isFree = (execProv && execProv->freeTier) || res.usedLocalFallback;
        int64_t iterCost = AiCatalog::calculateCostNano(res.usage, *execModel, isFree);
        accumCostNano += iterCost;

        if (m_ledger) {
            UsageEntry ue;
            ue.runId = runId;
            ue.owner = localRun.owner;
            ue.provider = execProv ? execProv->idStr : "unknown";
            ue.requestedModel = execModel->id;
            ue.actualModel = res.actualModel;
            ue.usage = res.usage;
            ue.costNano = iterCost;
            ue.latencyMs = res.latencyMs;
            ue.freeTier = isFree;
            m_ledger->record(ue);
        }

        double iterScore = scoreCandidateDeterministically(localRun.prompt, res.text, res.usedLocalFallback);
        std::string verdict = "deterministic_rubric";

        if (runCfg.optimizer && !res.usedLocalFallback && maxIters > 1) {
            const ModelSpec* revModel = m_catalog->findModel(localRun.reviewer);
            if (revModel && m_catalog->hasApiKey(revModel->provider) && revModel->provider != ProviderId::Local) {
                std::vector<ChatMsg> revMsgs = {
                    { "system", "Score the candidate answer from 0 to 100. Reply with 'SCORE: <num>' on the first line and one concise improvement note." },
                    { "user", "Prompt:\n" + localRun.prompt + "\n\nCandidate:\n" + res.text }
                };
                CompletionResult revRes = AiProviderClient::complete(*m_catalog, *revModel, revMsgs, 256);
                if (revRes.ok) {
                    const ProviderSpec* revProv = m_catalog->findProvider(revModel->provider);
                    bool revFree = revProv && revProv->freeTier;
                    int64_t revCost = AiCatalog::calculateCostNano(revRes.usage, *revModel, revFree);
                    accumCostNano += revCost;
                    if (m_ledger) {
                        UsageEntry rue;
                        rue.runId = runId;
                        rue.owner = localRun.owner;
                        rue.provider = revProv ? revProv->idStr : "unknown";
                        rue.requestedModel = revModel->id;
                        rue.actualModel = revRes.actualModel;
                        rue.usage = revRes.usage;
                        rue.costNano = revCost;
                        rue.latencyMs = revRes.latencyMs;
                        rue.freeTier = revFree;
                        m_ledger->record(rue);
                    }
                    iterScore = parseScoreFromReview(revRes.text, iterScore);
                    verdict = "reviewer:" + revModel->id;
                    lastFeedback = revRes.text;
                }
            }
        }

        bool accepted = (iterScore >= bestScore);
        if (accepted) {
            bestScore = iterScore;
            bestCandidate = res.text;
        }
        scoreHistory.push_back(iterScore);

        AgentIteration itRec;
        itRec.index = iter;
        itRec.score = iterScore;
        itRec.costNano = iterCost;
        itRec.tokens = res.usage.input + res.usage.output + res.usage.reasoning;
        itRec.latencyMs = res.latencyMs;
        itRec.verdict = verdict;
        itRec.candidate = res.text;
        itRec.accepted = accepted;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& r : m_runs) {
                if (r.id == runId) {
                    r.usage.input += res.usage.input;
                    r.usage.output += res.usage.output;
                    r.usage.cached += res.usage.cached;
                    r.usage.cacheWrite += res.usage.cacheWrite;
                    r.usage.reasoning += res.usage.reasoning;
                    r.costNano = accumCostNano;
                    r.iterations.push_back(itRec);
                    r.steps.push_back({ "iteration_" + std::to_string(iter), res.actualModel, "Score " + std::to_string(static_cast<int>(iterScore)) + " (" + verdict + ")" });
                    r.result = bestCandidate;
                    r.updatedAt = nowIsoTimestamp();
                    emitEventLocked(runId, "iteration_done", "Iteration " + std::to_string(iter) + " score=" + std::to_string(static_cast<int>(iterScore)));
                    break;
                }
            }
        }

        if (res.usedLocalFallback) {
            stopReason = "local_fallback_complete";
            break;
        }

        std::string policy = AiCatalog::evaluateStopPolicy(scoreHistory, accumCostNano, runCfg);
        if (!policy.empty()) {
            stopReason = policy;
            break;
        }
        if (lastFeedback.empty()) {
            lastFeedback = "Tighten precision, structure key findings, and address all aspects of the prompt.";
        }
    }

    if (stopReason.empty()) {
        stopReason = "completed";
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& r : m_runs) {
            if (r.id == runId) {
                r.status = (stopReason == "stopped_by_user") ? "stopped" : "completed";
                r.stopReason = stopReason;
                r.result = bestCandidate;
                r.updatedAt = nowIsoTimestamp();
                emitEventLocked(runId, "run_finished", "Status=" + r.status + " reason=" + stopReason);
                break;
            }
        }
    }

    if (!bestCandidate.empty()) {
        postMessage(localRun.room, "agent:" + execModel->id, localRun.owner, bestCandidate);
    }
}

bool AgentEngine::stopRun(const std::string& runId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stopFlags.find(runId);
    if (it == m_stopFlags.end()) return false;
    it->second = true;
    emitEventLocked(runId, "stop_requested", "Stop signal sent");
    return true;
}

const AgentRun* AgentEngine::findRun(const std::string& runId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& r : m_runs) {
        if (r.id == runId) return &r;
    }
    return nullptr;
}

std::vector<AgentRun> AgentEngine::listRuns() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<AgentRun>(m_runs.begin(), m_runs.end());
}

RoomMessage AgentEngine::postMessage(const std::string& room, const std::string& from, const std::string& to, const std::string& text) {
    RoomMessage msg;
    msg.id = "msg_" + CryptoEngine::randomHex(4);
    msg.room = room.empty() ? "general" : room;
    msg.from = from.empty() ? "system" : from;
    msg.to = to.empty() ? "all" : to;
    msg.text = text;
    msg.createdAt = nowIsoTimestamp();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_messages.push_back(msg);
    if (m_messages.size() > 1000) {
        m_messages.erase(m_messages.begin(), m_messages.begin() + 200);
    }
    return msg;
}

std::vector<RoomMessage> AgentEngine::listMessages(const std::string& room) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RoomMessage> out;
    for (const auto& m : m_messages) {
        if (room.empty() || m.room == room) {
            out.push_back(m);
        }
    }
    return out;
}

std::vector<AgentEvent> AgentEngine::listEvents(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_events.size() <= maxCount) return m_events;
    return std::vector<AgentEvent>(m_events.end() - static_cast<std::ptrdiff_t>(maxCount), m_events.end());
}

std::string AgentEngine::executeTool(
    const std::string& toolName,
    const std::map<std::string, std::string>& args,
    const std::string& room,
    const std::string& agentName) {
    auto getArg = [&](const std::string& key) -> std::string {
        auto it = args.find(key);
        return (it != args.end()) ? it->second : "";
    };

    if (toolName == "list_notes") {
        if (!m_noteManager) return "NoteManager unavailable";
        std::ostringstream oss;
        for (const auto& kv : m_noteManager->getAllNotes()) {
            oss << kv.first << "\n";
        }
        return oss.str();
    }

    if (toolName == "read_note") {
        if (!m_noteManager) return "NoteManager unavailable";
        Note* n = m_noteManager->getNoteById(getArg("id"));
        return n ? n->content : "Note not found";
    }

    if (toolName == "search_notes") {
        if (!m_noteManager || !m_brain) return "ContextBrain unavailable";
        auto ret = m_brain->retrieve(*m_noteManager, getArg("query"), 1200);
        return ret.formattedXml;
    }

    if (toolName == "create_note") {
        if (!m_catalog || !m_catalog->settings().allowWrites) {
            return "Write tools are disabled in settings";
        }
        if (!m_noteManager) return "NoteManager unavailable";
        std::string title = getArg("name");
        if (title.empty()) return "Missing note name";
        Note* n = m_noteManager->createNote(title);
        if (!n) return "Failed to create note";
        n->content = getArg("content");
        n->dirty = true;
        n->saveToFile();
        return "Created note: " + n->id;
    }

    if (toolName == "post_room_message") {
        std::string msgText = getArg("text");
        std::string target = getArg("to");
        postMessage(room, agentName, target.empty() ? "all" : target, msgText);
        return "Message posted";
    }

    if (toolName == "list_files") {
        std::error_code ec;
        std::ostringstream oss;
        int count = 0;
        if (!m_workspaceDir.empty() && std::filesystem::exists(m_workspaceDir, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(m_workspaceDir, ec)) {
                oss << entry.path().filename().string() << "\n";
                if (++count >= 100) break;
            }
        }
        return oss.str();
    }

    return "Unknown tool: " + toolName;
}

void AgentEngine::shutdown() {
    m_shuttingDown.store(true);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& kv : m_stopFlags) {
            kv.second = true;
        }
    }
    std::vector<std::thread> toJoin;
    {
        std::lock_guard<std::mutex> tlock(m_threadMutex);
        toJoin.swap(m_workers);
    }
    for (auto& th : toJoin) {
        if (th.joinable()) th.join();
    }
}

}
