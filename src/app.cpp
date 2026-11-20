#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

#include "app.h"
#include "explorer_panel.h"
#include "editor_panel.h"
#include "graph_panel.h"
#include "backlinks_panel.h"
#include "lua_console.h"
#include "memory_panel.h"
#include "command_panel.h"
#include "plugin_panel.h"
#include "query_panel.h"
#include "analytics_panel.h"
#include "srs_panel.h"
#include "git_panel.h"
#include "http_panel.h"
#include "database_panel.h"
#include "outliner_panel.h"
#include "task_panel.h"
#include "journal_panel.h"
#include "whiteboard_panel.h"
#include "asset_panel.h"
#include "sync_panel.h"
#include "command_palette.h"
#include "agent_panel.h"
#include "lab_panel.h"
#include "ai_exporter.h"
#include "ui_theme.h"
#include "ui_components.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unordered_set>

namespace nodepanda {

namespace {

volatile std::sig_atomic_t g_serverStopSignal = 0;

void handleServerSignal(int) {
    g_serverStopSignal = 1;
}

void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

std::string openFileDialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Archivos de notas (*.md;*.txt)\0*.md;*.txt\0Markdown (*.md)\0*.md\0Texto plano (*.txt)\0*.txt\0Todos los archivos (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "Seleccionar archivo";
    if (GetOpenFileNameA(&ofn)) return std::string(filename);
#endif
    return {};
}

void setupDockLayout(ImGuiID dockspaceId, ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoWindowMenuButton);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    ImGuiID leftId, centerId, rightId, rightBottomId, centerBottomId;
    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.19f, &leftId, &centerId);
    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.36f, &rightId, &centerId);
    ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.42f, &rightBottomId, &rightId);
    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.30f, &centerBottomId, &centerId);

    ImGui::DockBuilderDockWindow("Explorador de Notas", leftId);
    ImGui::DockBuilderDockWindow("Diarios", leftId);
    ImGui::DockBuilderDockWindow("Centro de Mando", leftId);
    ImGui::DockBuilderDockWindow("Base de Datos", leftId);
    ImGui::DockBuilderDockWindow("Sincronizacion", leftId);
    ImGui::DockBuilderDockWindow("Activos", leftId);
    ImGui::DockBuilderDockWindow("Gestor de Plugins", leftId);
    ImGui::DockBuilderDockWindow("Repeticion Espaciada", leftId);
    ImGui::DockBuilderDockWindow("Control de Versiones", leftId);
    ImGui::DockBuilderDockWindow("Servidor API", leftId);

    ImGui::DockBuilderDockWindow("Editor de Notas", centerId);
    ImGui::DockBuilderDockWindow("Outliner", centerId);
    ImGui::DockBuilderDockWindow("Tareas", centerId);
    ImGui::DockBuilderDockWindow("Pizarra", centerId);
    ImGui::DockBuilderDockWindow("Agentes IA", centerId);
    ImGui::DockBuilderDockWindow("Laboratorio", centerId);

    ImGui::DockBuilderDockWindow("Consola Lua", centerBottomId);
    ImGui::DockBuilderDockWindow("Motor de Consultas", centerBottomId);

    ImGui::DockBuilderDockWindow("Grafo de Nodos", rightId);
    ImGui::DockBuilderDockWindow("Backlinks", rightBottomId);
    ImGui::DockBuilderDockWindow("Motor de Memoria", rightBottomId);
    ImGui::DockBuilderDockWindow("Analisis de Grafo", rightBottomId);

    ImGui::DockBuilderFinish(dockspaceId);
}

}

App::App()
    : m_explorerPanel(new ExplorerPanel())
    , m_editorPanel(new EditorPanel())
    , m_graphPanel(new GraphPanel())
    , m_backlinksPanel(new BacklinksPanel())
    , m_luaConsole(new LuaConsole())
    , m_memoryPanel(new MemoryPanel())
    , m_commandPanel(new CommandPanel())
    , m_pluginPanel(new PluginPanel())
    , m_queryPanel(new QueryPanel())
    , m_analyticsPanel(new AnalyticsPanel())
    , m_srsPanel(new SRSPanel())
    , m_gitPanel(new GitPanel())
    , m_httpPanel(new HttpPanel())
    , m_databasePanel(new DatabasePanel())
    , m_outlinerPanel(new OutlinerPanel())
    , m_taskPanel(new TaskPanel())
    , m_journalPanel(new JournalPanel())
    , m_whiteboardPanel(new WhiteboardPanel())
    , m_assetPanel(new AssetPanel())
    , m_syncPanel(new SyncPanel())
    , m_commandPalette(new CommandPalette())
    , m_agentPanel(new AgentPanel())
    , m_labPanel(new LabPanel())
{}

App::~App() {
    delete m_explorerPanel;
    delete m_editorPanel;
    delete m_graphPanel;
    delete m_backlinksPanel;
    delete m_luaConsole;
    delete m_memoryPanel;
    delete m_commandPanel;
    delete m_pluginPanel;
    delete m_queryPanel;
    delete m_analyticsPanel;
    delete m_srsPanel;
    delete m_gitPanel;
    delete m_httpPanel;
    delete m_databasePanel;
    delete m_outlinerPanel;
    delete m_taskPanel;
    delete m_journalPanel;
    delete m_whiteboardPanel;
    delete m_assetPanel;
    delete m_syncPanel;
    delete m_commandPalette;
    delete m_agentPanel;
    delete m_labPanel;
}

bool App::initCore() {
    if (m_coreInitialized) return true;

    auto baseDir = std::filesystem::current_path();
    auto dataDir = baseDir / "data";
    auto notesDir = dataDir / "notas";
    std::error_code ec;
    std::filesystem::create_directories(notesDir, ec);

    m_authManager.init(baseDir);
    m_databaseManager.init(dataDir);
    m_noteManager.setNotesDirectory(notesDir);
    m_noteManager.scanDirectory();

    if (m_noteManager.getAllNotes().empty()) {
        auto* welcome = m_noteManager.createNote("Bienvenida");
        if (welcome) {
            welcome->frontmatter["tipo"] = "proyecto";
            welcome->frontmatter["tags"] = "inicio, grafo";
            welcome->content =
                "# Bienvenida\n\n"
                "- Crea y organiza bloques jerarquicos en el Outliner\n"
                "  - Enlaza paginas con `[[NombreDeLaNota]]`\n"
                "  - Usa propiedades con `estado:: activo`\n"
                "- TODO [#A] Revisar conexion con [[ProyectoIA]] y [[Ideas]]\n";
            welcome->saveToFile();
            welcome->parseLinks();
            welcome->parseTitle();
        }

        auto* proyecto = m_noteManager.createNote("ProyectoIA");
        if (proyecto) {
            proyecto->frontmatter["tipo"] = "proyecto";
            proyecto->frontmatter["aliases"] = "IA, Agentes";
            proyecto->frontmatter["tags"] = "agentes, datalog";
            proyecto->content =
                "# Proyecto IA\n\n"
                "- Coordinacion multi-agente en C++\n"
                "- DOING [#A] Evaluar modelos locales y APIs externas\n"
                "- Relacionado con [[Ideas]] y [[Bienvenida]]\n";
            proyecto->saveToFile();
            proyecto->parseLinks();
            proyecto->parseTitle();
        }

        auto* ideas = m_noteManager.createNote("Ideas");
        if (ideas) {
            ideas->frontmatter["tipo"] = "concepto";
            ideas->frontmatter["tags"] = "arquitectura";
            ideas->content =
                "# Ideas\n\n"
                "- Base de conocimiento enlazada para [[ProyectoIA]]\n"
                "- LATER [#B] Disenar pizarra visual del grafo\n";
            ideas->saveToFile();
            ideas->parseLinks();
            ideas->parseTitle();
        }
    }

    m_propertyEngine.init(dataDir);
    m_assetManager.init(dataDir);
    m_backupManager.init(dataDir);
    m_syncEngine.init(dataDir);
    m_whiteboardEngine.init(dataDir / "whiteboards");
    m_aiCatalog.init(dataDir);
    m_skillManager.init(dataDir / "skills");
    m_usageLedger.init(dataDir);
    m_evolutionEngine.init(dataDir);
    m_agentEngine.init(baseDir, &m_aiCatalog, &m_contextBrain, &m_skillManager, &m_usageLedger, &m_noteManager);
    m_gitManager.init(notesDir);

    rebuildKnowledgeIndices();
    rebuildGraph();
    m_coreInitialized = true;
    return true;
}

void App::rebuildKnowledgeIndices() {
    m_outlinerEngine.rebuildFromNotes(m_noteManager);
    m_taskEngine.rebuildFromNotes(m_noteManager);
    m_datalogEngine.rebuildIndex(m_noteManager);
    m_memoryEngine.markDirty();
}

bool App::init(const char* title, int width, int height) {
    if (!m_coreInitialized) initCore();

    m_windowWidth = width;
    m_windowHeight = height;

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigDockingWithShift = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    UITheme::loadFonts();
    UITheme::apply(ThemeMode::CarbonDark);

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    m_fileWatcher.start(m_noteManager.getNotesDirectory());
    m_luaManager.init(this);
    m_pluginManager.init(this);
    m_commandPalette->registerDefaultCommands();

    int port = 8042;
    const char* envPort = std::getenv("NODEPANDA_PORT");
    if (envPort) { try { port = std::stoi(envPort); } catch (...) {} }
    m_httpServer.start(this, port, "127.0.0.1");

    graphNeedsRebuild = true;
    m_guiInitialized = true;
    return true;
}

void App::runHeadlessServer(int port) {
    if (!m_coreInitialized) initCore();

    const char* envHost = std::getenv("NODEPANDA_HOST");
    std::string bindHost = envHost ? std::string(envHost) : "0.0.0.0";

    g_serverStopSignal = 0;
    std::signal(SIGINT, handleServerSignal);
    std::signal(SIGTERM, handleServerSignal);

    m_httpServer.start(this, port, bindHost);
    std::cout << "Servidor Node Panda activo en http://" << bindHost << ":" << port << "\n";
    std::cout << "Motor DB: " << m_databaseManager.getStatus().engineName
              << " (" << m_databaseManager.getStatus().modeName << ")\n";
    std::cout << "Presiona Ctrl+C para detener.\n";

    while (!g_serverStopSignal && m_httpServer.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    m_httpServer.stop();
}

void App::run() {
    while (!glfwWindowShouldClose(m_window)) {
        m_perfMetrics.beginFrame();
        glfwPollEvents();
        processFileWatcherEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) {
            if (m_commandPalette->isOpen()) m_commandPalette->close();
            else m_commandPalette->open();
        }

        if (currentState == AppState::HUB_BIENVENIDA) {
            renderHubBienvenida();
        } else {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);

            constexpr ImGuiWindowFlags kDockFlags =
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus |
                ImGuiWindowFlags_MenuBar;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("##DockSpaceHost", nullptr, kDockFlags);
            ImGui::PopStyleVar(3);

            ImGuiID dockId = ImGui::GetID("MainDockSpace");
            constexpr ImGuiDockNodeFlags kDockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
            if (ImGui::DockBuilderGetNode(dockId) == nullptr) {
                setupDockLayout(dockId, viewport->WorkSize);
            }

            ImGui::DockSpace(dockId, ImVec2(0, 0), kDockspaceFlags);
            renderMenuBar();
            ImGui::End();

            if (selectedNoteId != m_prevSelectedNoteId) {
                m_prevSelectedNoteId = selectedNoteId;
                graphNeedsRebuild = true;
                if (!selectedNoteId.empty()) {
                    m_pluginManager.callOnNoteOpen(selectedNoteId);
                }
            }
            if (graphNeedsRebuild) {
                rebuildGraph();
                rebuildKnowledgeIndices();
                graphNeedsRebuild = false;
            }
            m_forceLayout.step(m_graph);

            m_explorerPanel->render(*this);
            m_editorPanel->render(*this);
            m_outlinerPanel->render(*this);
            m_taskPanel->render(*this);
            m_journalPanel->render(*this);
            m_whiteboardPanel->render(*this);
            m_graphPanel->render(*this);
            m_backlinksPanel->render(*this);
            m_luaConsole->render(*this);
            m_memoryPanel->render(*this);
            m_commandPanel->render(*this);
            m_pluginPanel->render(*this);
            m_queryPanel->render(*this);
            m_analyticsPanel->render(*this);
            m_srsPanel->render(*this);
            m_gitPanel->render(*this);
            m_httpPanel->render(*this);
            m_databasePanel->render(*this);
            m_assetPanel->render(*this);
            m_syncPanel->render(*this);
            m_agentPanel->render(*this);
            m_labPanel->render(*this);
            m_commandPalette->render(*this);

            m_pluginManager.callOnFrame();

            if (m_showDemoWindow) {
                ImGui::ShowDemoWindow(&m_showDemoWindow);
            }
        }

        ui::renderToasts(io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f);

        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(m_window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        const auto& bg = UITheme::palette().bgBase;
        glClearColor(bg.x, bg.y, bg.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
        m_perfMetrics.endFrame();
    }
}

void App::shutdown() {
    m_httpServer.stop();
    m_agentEngine.shutdown();
    if (m_guiInitialized) {
        m_pluginManager.shutdown();
        m_fileWatcher.stop();
    }
    m_noteManager.saveAllDirty();
    if (m_gitManager.isAvailable() && m_gitManager.hasChanges()) {
        m_gitManager.autoCommit("[autosave] Cierre de Node Panda");
    }
    if (m_guiInitialized) {
        m_imageCache.clear();
        m_textureManager.clear();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_guiInitialized = false;
    }
}

void App::renderMenuBar() {
    if (!ImGui::BeginMenuBar()) return;
    const auto& pal = UITheme::palette();

    if (ImGui::BeginMenu("Archivo")) {
        if (ImGui::MenuItem("Guardar Todo", "Ctrl+S")) {
            m_noteManager.saveAllDirty();
            rebuildKnowledgeIndices();
            ui::pushToast("Notas guardadas", "success");
        }
        if (ImGui::MenuItem("Diario de Hoy")) {
            selectedNoteId = m_journalEngine.ensureTodayJournal(m_noteManager);
            graphNeedsRebuild = true;
        }
        if (ImGui::MenuItem("Reescanear Boveda")) {
            m_noteManager.scanDirectory();
            graphNeedsRebuild = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exportar Contexto XML")) {
            auto res = ExportVaultForClaude(m_noteManager);
            m_exportSuccess = res.success;
            if (res.success) {
                char buf[512];
                std::snprintf(buf, sizeof(buf), "%d notas exportadas (%.1f KB) en %s",
                              res.noteCount, static_cast<float>(res.totalBytes) / 1024.0f,
                              res.outputPath.c_str());
                m_exportResultMsg = buf;
            } else {
                m_exportResultMsg = "Error: " + res.error;
            }
            m_showExportResult = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Salir", "Alt+F4")) {
            glfwSetWindowShouldClose(m_window, true);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Vista")) {
        if (ImGui::MenuItem("Paleta de Comandos", "Ctrl+K")) {
            m_commandPalette->open();
        }
        ImGui::Separator();
        bool isDark = (UITheme::currentMode() == ThemeMode::CarbonDark);
        if (ImGui::MenuItem(isDark ? "Cambiar a Tema Claro" : "Cambiar a Tema Oscuro")) {
            UITheme::apply(isDark ? ThemeMode::PaperLight : ThemeMode::CarbonDark);
        }
        if (ImGui::MenuItem("Volver al Inicio")) {
            currentState = AppState::HUB_BIENVENIDA;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Herramientas")) {
        if (ImGui::MenuItem("Crear Respaldo Ahora")) {
            auto b = m_backupManager.createBackup(m_noteManager, "", "manual");
            ui::pushToast("Respaldo creado: " + b.name, "success");
        }
        if (ImGui::MenuItem("Sincronizar con Base de Datos")) {
            std::string summary;
            bool ok = m_databaseManager.syncNotes(m_noteManager, summary);
            ui::pushToast(summary, ok ? "success" : "warn");
        }
        if (ImGui::MenuItem("Guardar Version (Git)")) {
            if (m_gitManager.isAvailable()) {
                m_noteManager.saveAllDirty();
                m_gitManager.autoCommit();
                ui::pushToast("Commit Git creado", "success");
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Recalcular Analiticas del Grafo")) {
            m_graphAnalytics.analyze(m_graph);
        }
        if (ImGui::MenuItem("Refrescar Cola SRS")) {
            m_srsEngine.buildQueue(m_noteManager);
        }
        ImGui::Separator();
        bool serverRunning = m_httpServer.isRunning();
        if (ImGui::MenuItem(serverRunning ? "Detener Servidor API" : "Iniciar Servidor API")) {
            if (serverRunning) m_httpServer.stop();
            else m_httpServer.start(this, 8042);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Plugins")) {
        auto& menuItems = m_pluginManager.getMenuItems();
        if (menuItems.empty()) {
            ImGui::MenuItem("(sin items)", nullptr, false, false);
        } else {
            for (const auto& item : menuItems) {
                if (ImGui::MenuItem(item.label.c_str())) {
                    if (item.luaState && item.callbackRef != -1) {
                        lua_rawgeti(item.luaState, LUA_REGISTRYINDEX, item.callbackRef);
                        if (lua_pcall(item.luaState, 0, 0, 0) != LUA_OK) {
                            lua_pop(item.luaState, 1);
                        }
                    }
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reescanear Plugins")) {
            m_pluginManager.rescanPlugins();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Grafo")) {
        ImGui::SetNextItemWidth(140.0f);
        ImGui::SliderFloat("Gravedad Central", &m_forceLayout.centralGravity, 0.0001f, 0.01f, "%.4f");
        ImGui::SetNextItemWidth(140.0f);
        ImGui::SliderFloat("Theta (Barnes-Hut)", &m_forceLayout.barnesHutTheta, 0.0f, 1.5f);
        ImGui::Checkbox("Barnes-Hut", &m_forceLayout.useBarnesHut);
        if (ImGui::MenuItem("Reiniciar Posiciones")) {
            m_forceLayout.reset();
            m_forceLayout.initialize(m_graph, 800.0f, 600.0f);
        }
        ImGui::EndMenu();
    }

    float currentFps = m_perfMetrics.fps();
    ImVec4 fpsColor = (currentFps >= 55.0f) ? pal.success : (currentFps >= 30.0f) ? pal.warning : pal.danger;

    float rightSectionWidth = 560.0f;
    float avail = ImGui::GetWindowWidth();
    if (avail > rightSectionWidth + 320.0f) {
        ImGui::SameLine(avail - rightSectionWidth);
        ImGui::TextColored(fpsColor, "%.0f FPS", currentFps);
        ImGui::SameLine(0.0f, 10.0f);
        ImGui::TextColored(pal.textMuted, "%.1f MB | %d notas | %d bloques | %d tareas | DB:%s",
                           m_perfMetrics.memoryMB(),
                           static_cast<int>(m_noteManager.getAllNotes().size()),
                           static_cast<int>(m_outlinerEngine.getAllBlocks().size()),
                           static_cast<int>(m_taskEngine.getTasks().size()),
                           m_databaseManager.getStatus().engineName.c_str());
    }

    ImGui::EndMenuBar();

    if (m_showExportResult) ImGui::OpenPopup("Resultado Exportacion");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Resultado Exportacion", &m_showExportResult, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(m_exportSuccess ? pal.success : pal.danger,
                           m_exportSuccess ? "Exportacion completada" : "Error en la exportacion");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_exportResultMsg.c_str());
        ImGui::Spacing();
        if (ui::primaryButton("Cerrar", ImVec2(120, 0))) {
            m_showExportResult = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::renderActivityRail() {}

void App::rebuildGraph() {
    std::unordered_map<std::string, std::vector<std::string>> allLinks;
    std::unordered_map<std::string, std::string> allTypes;
    for (const auto& [id, note] : m_noteManager.getAllNotes()) {
        std::vector<std::string> resolved;
        for (const auto& link : note.links) {
            resolved.push_back(m_noteManager.resolveAlias(link));
        }
        allLinks[id] = std::move(resolved);
        auto tipoIt = note.frontmatter.find("tipo");
        if (tipoIt != note.frontmatter.end() && !tipoIt->second.empty()) {
            allTypes[id] = tipoIt->second;
        }
    }

    if (!selectedNoteId.empty() && allLinks.count(selectedNoteId)) {
        std::unordered_map<std::string, std::vector<std::string>> adj;
        for (const auto& [id, links] : allLinks) {
            for (const auto& target : links) {
                adj[id].push_back(target);
                adj[target].push_back(id);
            }
        }
        std::unordered_set<std::string> visited;
        std::vector<std::string> queue;
        queue.push_back(selectedNoteId);
        visited.insert(selectedNoteId);
        size_t front = 0;
        while (front < queue.size()) {
            const std::string current = queue[front++];
            if (adj.count(current)) {
                for (const auto& neighbor : adj.at(current)) {
                    if (visited.insert(neighbor).second) {
                        queue.push_back(neighbor);
                    }
                }
            }
        }
        std::unordered_map<std::string, std::vector<std::string>> filteredLinks;
        std::unordered_map<std::string, std::string> filteredTypes;
        for (const auto& nodeId : visited) {
            if (allLinks.count(nodeId)) {
                std::vector<std::string> filtered;
                for (const auto& target : allLinks.at(nodeId)) {
                    if (visited.count(target)) filtered.push_back(target);
                }
                filteredLinks[nodeId] = std::move(filtered);
            }
            if (allTypes.count(nodeId)) filteredTypes[nodeId] = allTypes.at(nodeId);
        }
        m_graph.buildFromNotes(filteredLinks, filteredTypes);
    } else {
        m_graph.buildFromNotes(allLinks, allTypes);
    }

    m_forceLayout.reset();
    m_forceLayout.initialize(m_graph, 800.0f, 600.0f);
}

void App::processFileWatcherEvents() {
    auto changes = m_fileWatcher.pollChanges();
    if (changes.empty()) return;

    bool needsRebuild = false;
    for (const auto& change : changes) {
        switch (change.event) {
            case FileEvent::Modified:
            case FileEvent::Added:
                if (m_noteManager.reloadNote(change.filename)) needsRebuild = true;
                break;
            case FileEvent::Removed:
                m_noteManager.reloadNote(change.filename);
                needsRebuild = true;
                break;
            case FileEvent::Renamed:
                m_noteManager.reloadNote(change.filename);
                m_noteManager.reloadNote(change.newFilename);
                needsRebuild = true;
                break;
            case FileEvent::Overflow:
                m_noteManager.scanDirectory();
                needsRebuild = true;
                break;
        }
    }
    if (needsRebuild) graphNeedsRebuild = true;
}

void App::importFileToNotes(const std::string& srcPath) {
    namespace fs = std::filesystem;
    fs::path src(srcPath);
    if (!fs::exists(src)) return;

    std::string stem = src.stem().string();
    std::string destName = stem + ".md";
    fs::path dest = m_noteManager.getNotesDirectory() / destName;

    int suffix = 1;
    while (fs::exists(dest)) {
        destName = stem + "_" + std::to_string(suffix++) + ".md";
        dest = m_noteManager.getNotesDirectory() / destName;
    }

    std::error_code ec;
    fs::copy_file(src, dest, ec);
    if (ec) return;

    m_noteManager.scanDirectory();
    graphNeedsRebuild = true;
    selectedNoteId = fs::path(destName).stem().string();
}

void App::renderHubBienvenida() {
    const auto& pal = UITheme::palette();

    if (!m_hubPandaLoaded) {
        m_hubPandaLoaded = true;
        namespace fs = std::filesystem;
        fs::path p1 = fs::current_path() / "panda.png";
        fs::path p2 = fs::current_path() / "data" / "panda.png";
        std::string pandaPath = fs::exists(p1) ? p1.string() : p2.string();
        const TextureInfo* tex = m_textureManager.getTexture(pandaPath);
        if (tex && tex->textureId != 0) {
            m_hubPandaTexId = static_cast<unsigned int>(tex->textureId);
        }
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kHubFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, pal.bgBase);
    ImGui::Begin("##HubBienvenida", nullptr, kHubFlags);
    ImGui::PopStyleColor();

    float winW = ImGui::GetContentRegionAvail().x;
    float winH = ImGui::GetContentRegionAvail().y;
    float cardW = 540.0f;
    float startX = (winW - cardW) * 0.5f;
    float startY = std::max(40.0f, (winH - 420.0f) * 0.5f);

    ImGui::SetCursorPos(ImVec2(startX, startY));
    if (ui::beginCard("##hub_main_card", ImVec2(cardW, 0))) {
        if (m_hubPandaTexId != 0) {
            float imgSize = 92.0f;
            ImGui::SetCursorPosX((cardW - imgSize) * 0.5f);
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_hubPandaTexId)),
                         ImVec2(imgSize, imgSize));
            ImGui::Spacing();
        }

        if (UITheme::fontHeading()) ImGui::PushFont(UITheme::fontHeading());
        std::string titleStr = "Node Panda";
        float tw = ImGui::CalcTextSize(titleStr.c_str()).x;
        ImGui::SetCursorPosX((cardW - tw) * 0.5f);
        ImGui::TextColored(pal.textPrimary, "%s", titleStr.c_str());
        if (UITheme::fontHeading()) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float colW = (cardW - 48.0f) / 3.0f;
        ui::metricCard("##hub_m1", "Notas", std::to_string(m_noteManager.getAllNotes().size()), pal.accent, colW);
        ImGui::SameLine();
        ui::metricCard("##hub_m2", "Bloques", std::to_string(m_outlinerEngine.getAllBlocks().size()), pal.aqua, colW);
        ImGui::SameLine();
        ui::metricCard("##hub_m3", "Tareas", std::to_string(m_taskEngine.getTasks().size()), pal.warning, colW);

        ImGui::Spacing();
        if (m_authManager.currentUser()) {
            ui::statusDot(("Sesion: " + m_authManager.currentUser()->username).c_str(), pal.success, true);
        } else {
            ui::statusDot("Sesion local activa", pal.accent, true);
        }
        ImGui::SameLine(0.0f, 18.0f);
        ui::badge(("API :" + std::to_string(m_httpServer.getPort())).c_str(), pal.accentSoft, pal.accent);
        ImGui::SameLine(0.0f, 8.0f);
        ui::badge(("DB " + m_databaseManager.getStatus().engineName).c_str(), pal.bgSurface3, pal.textSecondary);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float halfW = (cardW - 32.0f) * 0.5f;
        if (ui::subtleButton("+ Nueva Nota", ImVec2(halfW, 38.0f))) {
            std::memset(m_hubNewNoteName, 0, sizeof(m_hubNewNoteName));
            ImGui::OpenPopup("Crear Nueva Nota");
        }
        ImGui::SameLine();
        if (ui::subtleButton("Importar Archivo (.md/.txt)", ImVec2(halfW, 38.0f))) {
            std::string path = openFileDialog();
            if (!path.empty()) {
                importFileToNotes(path);
                currentState = AppState::ENTORNO_NOTAS;
            }
        }

        ImGui::Spacing();
        if (ui::primaryButton("Abrir Espacio de Trabajo", ImVec2(cardW - 24.0f, 44.0f))) {
            currentState = AppState::ENTORNO_NOTAS;
        }
    }
    ui::endCard();

    ImVec2 mc = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(mc, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Crear Nueva Nota", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Nombre:");
        ImGui::SetNextItemWidth(380.0f);
        bool enterHit = ImGui::InputText("##new_note_name", m_hubNewNoteName, sizeof(m_hubNewNoteName),
                                         ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        bool canCreate = m_hubNewNoteName[0] != '\0';
        if (!canCreate) ImGui::BeginDisabled();
        if (ui::primaryButton("Crear", ImVec2(120.0f, 0.0f)) || (enterHit && canCreate)) {
            auto* note = m_noteManager.createNote(m_hubNewNoteName);
            if (note) {
                note->content = "# " + std::string(m_hubNewNoteName) + "\n\n";
                note->saveToFile();
                note->parseLinks();
                note->parseTitle();
                selectedNoteId = note->id;
                graphNeedsRebuild = true;
                currentState = AppState::ENTORNO_NOTAS;
                m_pluginManager.callOnNoteCreate(note->id);
            }
            ImGui::CloseCurrentPopup();
        }
        if (!canCreate) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ui::subtleButton("Cancelar", ImVec2(120.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

}
