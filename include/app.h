#pragma once

#include "note_manager.h"
#include "graph.h"
#include "force_layout.h"
#include "file_watcher.h"
#include "image_cache.h"
#include "texture_manager.h"
#include "performance_metrics.h"
#include "lua_manager.h"
#include "memory_engine.h"
#include "plugin_manager.h"
#include "query_engine.h"
#include "graph_analytics.h"
#include "srs_engine.h"
#include "git_manager.h"
#include "http_server.h"
#include "auth_manager.h"
#include "database_manager.h"
#include "outliner_engine.h"
#include "task_engine.h"
#include "journal_engine.h"
#include "whiteboard_engine.h"
#include "datalog_engine.h"
#include "property_engine.h"
#include "asset_manager.h"
#include "backup_manager.h"
#include "sync_engine.h"
#include "ai_catalog.h"
#include "context_brain.h"
#include "skill_manager.h"
#include "usage_ledger.h"
#include "data_lab.h"
#include "evolution_engine.h"
#include "agent_engine.h"
#include "pdf_engine.h"
#include "websocket_rtc.h"
#include "table_view_engine.h"
#include "vector_engine.h"
#include "math_renderer.h"
#include "mobile_bridge.h"

struct GLFWwindow;

namespace nodepanda {
    class ExplorerPanel;
    class EditorPanel;
    class GraphPanel;
    class BacklinksPanel;
    class LuaConsole;
    class MemoryPanel;
    class CommandPanel;
    class PluginPanel;
    class QueryPanel;
    class AnalyticsPanel;
    class SRSPanel;
    class GitPanel;
    class HttpPanel;
    class DatabasePanel;
    class OutlinerPanel;
    class TaskPanel;
    class JournalPanel;
    class WhiteboardPanel;
    class AssetPanel;
    class SyncPanel;
    class CommandPalette;
    class AgentPanel;
    class LabPanel;
    class PdfPanel;
    class RtcPanel;
    class TableViewPanel;
    class VectorPanel;
    class MathPanel;
}

namespace nodepanda {

enum class AppState {
    HUB_BIENVENIDA,
    ENTORNO_NOTAS
};

class App {
public:
    App();
    ~App();

    bool initCore();
    bool init(const char* title, int width, int height);
    void run();
    void runHeadlessServer(int port);
    void shutdown();

    NoteManager& getNoteManager() { return m_noteManager; }
    Graph& getGraph() { return m_graph; }
    ForceLayout& getForceLayout() { return m_forceLayout; }
    ImageCache& getImageCache() { return m_imageCache; }
    TextureManager& getTextureManager() { return m_textureManager; }
    PerformanceMetrics& getPerformanceMetrics() { return m_perfMetrics; }
    LuaManager& getLuaManager() { return m_luaManager; }
    MemoryEngine& getMemoryEngine() { return m_memoryEngine; }
    PluginManager& getPluginManager() { return m_pluginManager; }
    QueryEngine& getQueryEngine() { return m_queryEngine; }
    GraphAnalytics& getGraphAnalytics() { return m_graphAnalytics; }
    SRSEngine& getSRSEngine() { return m_srsEngine; }
    GitManager& getGitManager() { return m_gitManager; }
    HttpServer& getHttpServer() { return m_httpServer; }
    AuthManager& getAuthManager() { return m_authManager; }
    DatabaseManager& getDatabaseManager() { return m_databaseManager; }
    OutlinerEngine& getOutlinerEngine() { return m_outlinerEngine; }
    TaskEngine& getTaskEngine() { return m_taskEngine; }
    JournalEngine& getJournalEngine() { return m_journalEngine; }
    WhiteboardEngine& getWhiteboardEngine() { return m_whiteboardEngine; }
    DatalogEngine& getDatalogEngine() { return m_datalogEngine; }
    PropertyEngine& getPropertyEngine() { return m_propertyEngine; }
    AssetManager& getAssetManager() { return m_assetManager; }
    BackupManager& getBackupManager() { return m_backupManager; }
    SyncEngine& getSyncEngine() { return m_syncEngine; }
    AiCatalog& getAiCatalog() { return m_aiCatalog; }
    ContextBrain& getContextBrain() { return m_contextBrain; }
    SkillManager& getSkillManager() { return m_skillManager; }
    UsageLedger& getUsageLedger() { return m_usageLedger; }
    DataLab& getDataLab() { return m_dataLab; }
    EvolutionEngine& getEvolutionEngine() { return m_evolutionEngine; }
    AgentEngine& getAgentEngine() { return m_agentEngine; }
    PdfEngine& getPdfEngine() { return m_pdfEngine; }
    WebSocketRtcServer& getWebSocketRtc() { return m_wsRtcServer; }
    WebSocketRtcServer& getWebSocketRtcServer() { return m_wsRtcServer; }
    TableViewEngine& getTableViewEngine() { return m_tableViewEngine; }
    VectorEngine& getVectorEngine() { return m_vectorEngine; }

    void rebuildKnowledgeIndices();

    std::string selectedNoteId;
    bool graphNeedsRebuild = true;
    AppState currentState = AppState::HUB_BIENVENIDA;
    int activeWorkspaceTab = 0;

private:
    void renderMenuBar();
    void renderActivityRail();
    void rebuildGraph();
    void processFileWatcherEvents();
    void renderHubBienvenida();
    void importFileToNotes(const std::string& srcPath);

    GLFWwindow* m_window = nullptr;
    bool m_coreInitialized = false;
    bool m_guiInitialized = false;

    NoteManager m_noteManager;
    Graph m_graph;
    ForceLayout m_forceLayout;
    FileWatcher m_fileWatcher;
    ImageCache m_imageCache;
    TextureManager m_textureManager;
    PerformanceMetrics m_perfMetrics;
    LuaManager m_luaManager;
    MemoryEngine m_memoryEngine;
    PluginManager m_pluginManager;
    QueryEngine m_queryEngine;
    GraphAnalytics m_graphAnalytics;
    SRSEngine m_srsEngine;
    GitManager m_gitManager;
    HttpServer m_httpServer;
    AuthManager m_authManager;
    DatabaseManager m_databaseManager;
    OutlinerEngine m_outlinerEngine;
    TaskEngine m_taskEngine;
    JournalEngine m_journalEngine;
    WhiteboardEngine m_whiteboardEngine;
    DatalogEngine m_datalogEngine;
    PropertyEngine m_propertyEngine;
    AssetManager m_assetManager;
    BackupManager m_backupManager;
    SyncEngine m_syncEngine;
    AiCatalog m_aiCatalog;
    ContextBrain m_contextBrain;
    SkillManager m_skillManager;
    UsageLedger m_usageLedger;
    DataLab m_dataLab;
    EvolutionEngine m_evolutionEngine;
    AgentEngine m_agentEngine;
    PdfEngine m_pdfEngine;
    WebSocketRtcServer m_wsRtcServer;
    TableViewEngine m_tableViewEngine;
    VectorEngine m_vectorEngine;

    ExplorerPanel* m_explorerPanel = nullptr;
    EditorPanel* m_editorPanel = nullptr;
    GraphPanel* m_graphPanel = nullptr;
    BacklinksPanel* m_backlinksPanel = nullptr;
    LuaConsole* m_luaConsole = nullptr;
    MemoryPanel* m_memoryPanel = nullptr;
    CommandPanel* m_commandPanel = nullptr;
    PluginPanel* m_pluginPanel = nullptr;
    QueryPanel* m_queryPanel = nullptr;
    AnalyticsPanel* m_analyticsPanel = nullptr;
    SRSPanel* m_srsPanel = nullptr;
    GitPanel* m_gitPanel = nullptr;
    HttpPanel* m_httpPanel = nullptr;
    DatabasePanel* m_databasePanel = nullptr;
    OutlinerPanel* m_outlinerPanel = nullptr;
    TaskPanel* m_taskPanel = nullptr;
    JournalPanel* m_journalPanel = nullptr;
    WhiteboardPanel* m_whiteboardPanel = nullptr;
    AssetPanel* m_assetPanel = nullptr;
    SyncPanel* m_syncPanel = nullptr;
    CommandPalette* m_commandPalette = nullptr;
    AgentPanel* m_agentPanel = nullptr;
    LabPanel* m_labPanel = nullptr;
    PdfPanel* m_pdfPanel = nullptr;
    RtcPanel* m_rtcPanel = nullptr;
    TableViewPanel* m_tableViewPanel = nullptr;
    VectorPanel* m_vectorPanel = nullptr;
    MathPanel* m_mathPanel = nullptr;

    bool m_showDemoWindow = false;
    bool m_showExportResult = false;
    bool m_exportSuccess = false;
    std::string m_exportResultMsg;
    int m_windowWidth = 1480;
    int m_windowHeight = 920;
    std::string m_prevSelectedNoteId;

    unsigned int m_hubPandaTexId = 0;
    bool m_hubPandaLoaded = false;
    char m_hubNewNoteName[256] = {};
};

}
