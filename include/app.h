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

struct GLFWwindow;

namespace nodepanda {
    class ExplorerPanel;
    class EditorPanel;
    class GraphPanel;
    class BacklinksPanel;
    class LuaConsole;
    class MemoryPanel;
    class CommandPanel;
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

    bool init(const char* title, int width, int height);
    void run();
    void shutdown();

    NoteManager& getNoteManager() { return m_noteManager; }
    Graph& getGraph() { return m_graph; }
    ForceLayout& getForceLayout() { return m_forceLayout; }
    ImageCache& getImageCache() { return m_imageCache; }
    TextureManager& getTextureManager() { return m_textureManager; }
    PerformanceMetrics& getPerformanceMetrics() { return m_perfMetrics; }
    LuaManager& getLuaManager() { return m_luaManager; }
    MemoryEngine& getMemoryEngine() { return m_memoryEngine; }

    std::string selectedNoteId;
    bool        graphNeedsRebuild = true;
    AppState    currentState      = AppState::HUB_BIENVENIDA;

private:
    void renderMenuBar();
    void rebuildGraph();
    void processFileWatcherEvents();
    void renderHubBienvenida();     
    void importFileToNotes(const std::string& srcPath); 

    GLFWwindow* m_window = nullptr;
    NoteManager m_noteManager;
    Graph m_graph;
    ForceLayout m_forceLayout;
    FileWatcher m_fileWatcher;
    ImageCache m_imageCache;
    TextureManager m_textureManager;
    PerformanceMetrics m_perfMetrics;
    LuaManager m_luaManager;
    MemoryEngine m_memoryEngine;

    ExplorerPanel* m_explorerPanel = nullptr;
    EditorPanel* m_editorPanel = nullptr;
    GraphPanel* m_graphPanel = nullptr;
    BacklinksPanel* m_backlinksPanel = nullptr;
    LuaConsole*     m_luaConsole     = nullptr;
    MemoryPanel*    m_memoryPanel    = nullptr;
    CommandPanel*   m_commandPanel   = nullptr;

    bool m_showDemoWindow = false;
    bool m_showExportResult = false;
    bool m_exportSuccess    = false;
    std::string m_exportResultMsg;
    int  m_windowWidth    = 1400;
    int  m_windowHeight   = 900;
    std::string m_prevSelectedNoteId;  

    unsigned int m_hubPandaTexId  = 0;      
    bool         m_hubPandaLoaded = false;  
    char         m_hubNewNoteName[256] = {}; 
};

} 
