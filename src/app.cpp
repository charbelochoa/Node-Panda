
#ifdef _WIN32
    #define NOMINMAX            
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <commdlg.h>        
    #pragma comment(lib, "comdlg32.lib")
#endif

#include "app.h"
#include "explorer_panel.h"
#include "editor_panel.h"
#include "graph_panel.h"
#include "backlinks_panel.h"
#include "lua_console.h"
#include "memory_panel.h"
#include "command_panel.h"
#include "ai_exporter.h"

#include "imgui.h"
#include "imgui_internal.h"          
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unordered_set>

namespace nodepanda {

static void glfwErrorCallback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}


static std::string openFileDialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn        = {};
    ofn.lStructSize          = sizeof(ofn);
    ofn.hwndOwner            = nullptr;
    ofn.lpstrFilter          = "Archivos de notas (*.md;*.txt)\0*.md;*.txt\0"
                               "Markdown (*.md)\0*.md\0"
                               "Texto plano (*.txt)\0*.txt\0"
                               "Todos los archivos (*.*)\0*.*\0";
    ofn.lpstrFile            = filename;
    ofn.nMaxFile             = MAX_PATH;
    ofn.Flags                = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle           = "Seleccionar archivo de notas";
    if (GetOpenFileNameA(&ofn))
        return std::string(filename);
#endif
    return {};
}


App::App()
    : m_explorerPanel(new ExplorerPanel())
    , m_editorPanel(new EditorPanel())
    , m_graphPanel(new GraphPanel())
    , m_backlinksPanel(new BacklinksPanel())
    , m_luaConsole(new LuaConsole())
    , m_memoryPanel(new MemoryPanel())
    , m_commandPanel(new CommandPanel())
{}

App::~App() {
    delete m_explorerPanel;
    delete m_editorPanel;
    delete m_graphPanel;
    delete m_backlinksPanel;
    delete m_luaConsole;
    delete m_memoryPanel;
    delete m_commandPanel;
}


static void applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding     = 0.0f;   
    s.ChildRounding      = 6.0f;
    s.FrameRounding      = 5.0f;
    s.GrabRounding       = 5.0f;
    s.TabRounding        = 5.0f;
    s.PopupRounding      = 6.0f;
    s.ScrollbarRounding  = 6.0f;
    s.WindowBorderSize   = 0.0f;   
    s.ChildBorderSize    = 1.0f;
    s.FrameBorderSize    = 0.0f;
    s.PopupBorderSize    = 1.0f;
    s.TabBorderSize      = 0.0f;

    s.WindowPadding      = ImVec2(12.0f, 10.0f);
    s.FramePadding       = ImVec2(8.0f,  4.5f);
    s.ItemSpacing        = ImVec2(8.0f,  6.0f);
    s.ItemInnerSpacing   = ImVec2(5.0f,  4.0f);
    s.ScrollbarSize      = 10.0f;
    s.GrabMinSize        = 8.0f;
    s.IndentSpacing      = 16.0f;
    s.DockingSeparatorSize = 2.0f;

    
    ImVec4* c = s.Colors;

    
    c[ImGuiCol_Text]                 = ImVec4(0.86f, 0.87f, 0.93f, 1.00f); 
    c[ImGuiCol_TextDisabled]         = ImVec4(0.40f, 0.41f, 0.50f, 1.00f);
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.20f, 0.60f, 0.57f, 0.38f); 

    
    c[ImGuiCol_WindowBg]             = ImVec4(0.07f, 0.07f, 0.10f, 1.00f); // #121219
    c[ImGuiCol_ChildBg]              = ImVec4(0.08f, 0.08f, 0.11f, 1.00f); // #14141C
    c[ImGuiCol_PopupBg]              = ImVec4(0.09f, 0.09f, 0.13f, 0.97f); // #17171F

    c[ImGuiCol_Border]               = ImVec4(0.18f, 0.18f, 0.26f, 0.55f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    c[ImGuiCol_FrameBg]              = ImVec4(0.11f, 0.11f, 0.17f, 1.00f); // #1C1C2B
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.15f, 0.16f, 0.23f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.19f, 0.20f, 0.29f, 1.00f);

    c[ImGuiCol_TitleBg]              = ImVec4(0.07f, 0.07f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.09f, 0.09f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.05f, 0.05f, 0.07f, 0.90f);

    c[ImGuiCol_MenuBarBg]            = ImVec4(0.06f, 0.06f, 0.09f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.06f, 0.09f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.20f, 0.22f, 0.32f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.27f, 0.30f, 0.42f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.34f, 0.38f, 0.52f, 1.00f);

    c[ImGuiCol_CheckMark]            = ImVec4(0.28f, 0.82f, 0.76f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.28f, 0.72f, 0.68f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.35f, 0.88f, 0.82f, 1.00f);

    c[ImGuiCol_Button]               = ImVec4(0.12f, 0.20f, 0.34f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.17f, 0.33f, 0.52f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.20f, 0.40f, 0.62f, 1.00f);

    c[ImGuiCol_Header]               = ImVec4(0.13f, 0.20f, 0.36f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.18f, 0.30f, 0.50f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.22f, 0.36f, 0.58f, 1.00f);

    c[ImGuiCol_Separator]            = ImVec4(0.16f, 0.17f, 0.24f, 0.80f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.28f, 0.72f, 0.68f, 0.78f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.35f, 0.88f, 0.82f, 1.00f);

    c[ImGuiCol_ResizeGrip]           = ImVec4(0.28f, 0.72f, 0.68f, 0.20f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.28f, 0.72f, 0.68f, 0.67f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.35f, 0.88f, 0.82f, 0.95f);

    c[ImGuiCol_Tab]                  = ImVec4(0.09f, 0.10f, 0.15f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.18f, 0.32f, 0.52f, 1.00f);
    c[ImGuiCol_TabActive]            = ImVec4(0.14f, 0.25f, 0.42f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.07f, 0.08f, 0.12f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.10f, 0.16f, 0.28f, 1.00f);

    c[ImGuiCol_DockingPreview]       = ImVec4(0.28f, 0.72f, 0.68f, 0.70f); 
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.06f, 0.06f, 0.09f, 1.00f);

    c[ImGuiCol_PlotLines]            = ImVec4(0.35f, 0.80f, 0.72f, 1.00f);
    c[ImGuiCol_PlotLinesHovered]     = ImVec4(0.42f, 0.95f, 0.87f, 1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.28f, 0.65f, 0.62f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.35f, 0.82f, 0.78f, 1.00f);

    c[ImGuiCol_NavHighlight]         = ImVec4(0.28f, 0.72f, 0.68f, 1.00f);
    c[ImGuiCol_NavWindowingHighlight]= ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    c[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.04f, 0.04f, 0.07f, 0.65f);
    c[ImGuiCol_DragDropTarget]       = ImVec4(0.28f, 0.88f, 0.82f, 0.90f);
}


static void setupDockLayout(ImGuiID dockspaceId, ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId,
        ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoWindowMenuButton);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    ImGuiID leftId, centerId, rightId, rightBottomId, centerBottomId;

    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.18f,
                                &leftId, &centerId);

    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.37f,
                                &rightId, &centerId);

    ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.40f,
                                &rightBottomId, &rightId);

    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.30f,
                                &centerBottomId, &centerId);

    ImGui::DockBuilderDockWindow("Explorador de Notas", leftId);
    ImGui::DockBuilderDockWindow("Editor de Notas",    centerId);
    ImGui::DockBuilderDockWindow("Consola Lua",        centerBottomId);
    ImGui::DockBuilderDockWindow("Grafo de Nodos",     rightId);
    ImGui::DockBuilderDockWindow("Backlinks",           rightBottomId);
    ImGui::DockBuilderDockWindow("Motor de Memoria",    rightBottomId); // tab junto a Backlinks
    ImGui::DockBuilderDockWindow("Centro de Mando",     leftId);        // tab junto a Explorador

    ImGui::DockBuilderFinish(dockspaceId);
}


bool App::init(const char* title, int width, int height) {
    m_windowWidth  = width;
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
    if (!m_window) { glfwTerminate(); return false; }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); 

    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigDockingWithShift          = false; 
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    
    io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark(); 
    applyTheme();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    auto notesDir = std::filesystem::current_path() / "data" / "notas";
    m_noteManager.setNotesDirectory(notesDir);
    m_noteManager.scanDirectory();

    if (m_noteManager.getAllNotes().empty()) {
        auto* welcome = m_noteManager.createNote("Bienvenida");
        if (welcome) {
            welcome->content =
                "# Bienvenida a Node Panda\n\n"
                "Gestor personal de notas enlazadas, con grafo visual y exportacion de contexto.\n\n"
                "## Comenzar\n\n"
                "- Crea notas con el boton **+ Nota** en el explorador izquierdo\n"
                "- Enlaza notas usando `[[NombreDeLaNota]]`\n"
                "- Visualiza las conexiones en el **Grafo de Nodos** (panel derecho)\n"
                "- Exporta contexto con el boton **Exportar Contexto**\n\n"
                "## Ejemplo\n\n"
                "Visita [[ProyectoIA]] y [[Ideas]] para ver el grafo en accion.\n";
            welcome->saveToFile();
            welcome->parseLinks();
        }

        auto* proyecto = m_noteManager.createNote("ProyectoIA");
        if (proyecto) {
            proyecto->frontmatter["tipo"]    = "proyecto";
            proyecto->frontmatter["aliases"] = "IA, InteligenciaArtificial";
            proyecto->frontmatter["tags"]    = "machine-learning, agentes";
            proyecto->content =
                "# Proyecto IA\n\n"
                "Investigacion sobre sistemas multi-agente.\n\n"
                "Relacionado con [[Ideas]] y [[Bienvenida]].\n";
            proyecto->saveToFile();
            proyecto->parseLinks();
        }

        auto* ideas = m_noteManager.createNote("Ideas");
        if (ideas) {
            ideas->frontmatter["tipo"] = "concepto";
            ideas->content =
                "# Ideas\n\n"
                "Banco de ideas para [[ProyectoIA]].\n";
            ideas->saveToFile();
            ideas->parseLinks();
        }
    }

    m_fileWatcher.start(notesDir);

    m_luaManager.init(this);

    graphNeedsRebuild = true;
    return true;
}


void App::run() {
    while (!glfwWindowShouldClose(m_window)) {
        m_perfMetrics.beginFrame();
        glfwPollEvents();

        processFileWatcherEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (currentState == AppState::HUB_BIENVENIDA) {

            renderHubBienvenida();

        } else {

            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);

            constexpr ImGuiWindowFlags kDockFlags =
                ImGuiWindowFlags_NoDocking        |
                ImGuiWindowFlags_NoTitleBar        |
                ImGuiWindowFlags_NoCollapse        |
                ImGuiWindowFlags_NoResize          |
                ImGuiWindowFlags_NoMove            |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus        |
                ImGuiWindowFlags_MenuBar;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("##DockSpaceHost", nullptr, kDockFlags);
            ImGui::PopStyleVar(3);

            ImGuiID dockId = ImGui::GetID("MainDockSpace");
            constexpr ImGuiDockNodeFlags kDockspaceFlags =
                ImGuiDockNodeFlags_PassthruCentralNode;

            if (ImGui::DockBuilderGetNode(dockId) == nullptr) {
                setupDockLayout(dockId, viewport->WorkSize);
            }

            ImGui::DockSpace(dockId, ImVec2(0, 0), kDockspaceFlags);
            renderMenuBar();
            ImGui::End();

            
            if (selectedNoteId != m_prevSelectedNoteId) {
                m_prevSelectedNoteId = selectedNoteId;
                graphNeedsRebuild = true;
            }
            if (graphNeedsRebuild) {
                rebuildGraph();
                graphNeedsRebuild = false;
                m_memoryEngine.markDirty(); // re-indexar en próximo frame del panel
            }
            m_forceLayout.step(m_graph);

            m_explorerPanel->render(*this);
            m_editorPanel->render(*this);
            m_graphPanel->render(*this);
            m_backlinksPanel->render(*this);
            m_luaConsole->render(*this);
            m_memoryPanel->render(*this);
            m_commandPanel->render(*this);

            if (m_showDemoWindow) {
                ImGui::ShowDemoWindow(&m_showDemoWindow);
            }

        } 

        
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(m_window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
        m_perfMetrics.endFrame();
    }
}


void App::shutdown() {
    m_fileWatcher.stop();
    m_noteManager.saveAllDirty();
    m_imageCache.clear();
    m_textureManager.clear();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}


void App::renderMenuBar() {
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("Archivo")) {
        if (ImGui::MenuItem("Guardar Todo", "Ctrl+S")) {
            m_noteManager.saveAllDirty();
        }
        if (ImGui::MenuItem("Reescanear Notas")) {
            m_noteManager.scanDirectory();
            graphNeedsRebuild = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exportar Boveda para Claude")) {
            auto res = ExportVaultForClaude(m_noteManager);
            m_exportSuccess = res.success;
            if (res.success) {
                char buf[512];
                std::snprintf(buf, sizeof(buf),
                    "%d notas exportadas en formato XML.\n"
                    "Tamano: %.1f KB\n"
                    "Archivo: %s",
                    res.noteCount,
                    (float)res.totalBytes / 1024.0f,
                    res.outputPath.c_str());
                m_exportResultMsg = buf;
            } else {
                m_exportResultMsg = "Error: " + res.error;
            }
            m_showExportResult = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Exportar TODAS las notas en formato XML\n"
                              "optimizado para el LLM Claude.\n"
                              "Se guarda en claude_context_export.txt");
        ImGui::Separator();
        if (ImGui::MenuItem("Salir", "Alt+F4")) {
            glfwSetWindowShouldClose(m_window, true);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Vista")) {
        ImGui::MenuItem("Demo ImGui", nullptr, &m_showDemoWindow);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Grafo")) {
        ImGui::SetNextItemWidth(140.0f);
        ImGui::SliderFloat("Gravedad Central",
                           &m_forceLayout.centralGravity, 0.0001f, 0.01f, "%.4f");
        ImGui::SetNextItemWidth(140.0f);
        ImGui::SliderFloat("Theta (Barnes-Hut)",
                           &m_forceLayout.barnesHutTheta, 0.0f, 1.5f);
        ImGui::Checkbox("Activar Barnes-Hut", &m_forceLayout.useBarnesHut);
        if (ImGui::MenuItem("Reiniciar Posiciones")) {
            m_forceLayout.reset();
            m_forceLayout.initialize(m_graph, 800.0f, 600.0f);
        }
        ImGui::EndMenu();
    }

    
    const char* watcherState = m_fileWatcher.isRunning() ? "ON" : "OFF";
    ImVec4 watcherColor = m_fileWatcher.isRunning()
        ? ImVec4(0.28f, 0.85f, 0.70f, 1.0f)
        : ImVec4(0.80f, 0.35f, 0.30f, 1.0f);

    
    float currentFps = m_perfMetrics.fps();
    ImVec4 fpsColor = (currentFps >= 60.0f) ? ImVec4(0.30f, 0.88f, 0.55f, 1.0f)
                    : (currentFps >= 30.0f) ? ImVec4(0.90f, 0.80f, 0.25f, 1.0f)
                    :                         ImVec4(0.90f, 0.35f, 0.30f, 1.0f);

    float statusWidth = 580.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - statusWidth);

    
    ImGui::TextColored(fpsColor, "%.0f FPS", currentFps);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.45f, 0.46f, 0.58f, 1.0f),
        "(%.1fms)  |  %.1f MB  |  Notas: %d  |  Nodos: %d  |  Watcher:",
        m_perfMetrics.frameTimeMs(),
        m_perfMetrics.memoryMB(),
        (int)m_noteManager.getAllNotes().size(),
        (int)m_graph.getNodes().size());
    ImGui::SameLine();
    ImGui::TextColored(watcherColor, "%s", watcherState);

    ImGui::EndMenuBar();

    // ── Popup de resultado de exportación ────────────────────────────────────
    if (m_showExportResult) ImGui::OpenPopup("Resultado Exportacion");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Resultado Exportacion", &m_showExportResult,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_exportSuccess) {
            ImGui::TextColored(ImVec4(0.28f, 0.92f, 0.65f, 1.0f),
                               "Exportacion completada");
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.30f, 1.0f),
                               "Error en la exportacion");
        }
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.15f, 0.17f, 0.25f, 0.80f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_exportResultMsg.c_str());
        ImGui::Spacing();
        if (ImGui::Button("  Cerrar  ", ImVec2(120, 0))) {
            m_showExportResult = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}


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
                    if (visited.count(target))
                        filtered.push_back(target);
                }
                filteredLinks[nodeId] = std::move(filtered);
            }
            if (allTypes.count(nodeId))
                filteredTypes[nodeId] = allTypes.at(nodeId);
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
                fprintf(stderr, "[App] FileWatcher overflow — rescan completo\n");
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

    std::string stem     = src.stem().string();
    std::string destName = stem + ".md";
    fs::path    dest     = m_noteManager.getNotesDirectory() / destName;

    int suffix = 1;
    while (fs::exists(dest)) {
        destName = stem + "_" + std::to_string(suffix++) + ".md";
        dest     = m_noteManager.getNotesDirectory() / destName;
    }

    std::error_code ec;
    fs::copy_file(src, dest, ec);
    if (ec) {
        fprintf(stderr, "[App] Error importando '%s': %s\n",
                srcPath.c_str(), ec.message().c_str());
        return;
    }

    m_noteManager.scanDirectory();
    graphNeedsRebuild = true;

    selectedNoteId = fs::path(destName).stem().string();
    fprintf(stderr, "[App] Archivo importado: %s → %s\n",
            srcPath.c_str(), dest.string().c_str());
}


void App::renderHubBienvenida() {

    if (!m_hubPandaLoaded) {
        m_hubPandaLoaded = true;          
        namespace fs = std::filesystem;
        fs::path p1 = fs::current_path() / "panda.png";
        fs::path p2 = fs::current_path() / "data" / "panda.png";
        std::string pandaPath = fs::exists(p1) ? p1.string() : p2.string();
        const TextureInfo* tex = m_textureManager.getTexture(pandaPath);
        if (tex && tex->textureId != 0)
            m_hubPandaTexId = static_cast<unsigned int>(tex->textureId);
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kHubFlags =
        ImGuiWindowFlags_NoDocking             |
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus            |
        ImGuiWindowFlags_NoScrollbar           |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.09f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##HubBienvenida", nullptr, kHubFlags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    const float winW = ImGui::GetContentRegionAvail().x;
    const float winH = ImGui::GetContentRegionAvail().y;

    
    constexpr float IMG_SIZE = 128.0f;
    constexpr float TOTAL_H  =
          IMG_SIZE + 24.0f    
        + 16.0f  +  8.0f     
        + 16.0f  + 28.0f     
        + 21.0f              
        + 16.0f  + 22.0f     
        + 38.0f  + 16.0f     
        + 21.0f              
        + 44.0f;             

    float cy = (winH - TOTAL_H) * 0.5f;
    if (cy < 20.0f) cy = 20.0f;

    if (m_hubPandaTexId != 0) {
        ImGui::SetCursorPos(ImVec2((winW - IMG_SIZE) * 0.5f, cy));
        ImGui::Image(
            reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_hubPandaTexId)),
            ImVec2(IMG_SIZE, IMG_SIZE));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Node Panda — tu asistente de notas enlazadas");
    } else {
        const char* ph = "[ panda.png ]";
        ImVec2 phsz = ImGui::CalcTextSize(ph);
        ImGui::SetCursorPos(ImVec2(
            (winW - phsz.x) * 0.5f,
            cy + (IMG_SIZE - phsz.y) * 0.5f));
        ImGui::TextDisabled("%s", ph);
    }
    cy += IMG_SIZE + 24.0f;

    {
        const char* t1 = "BIENVENIDO, USUARIO!";
        ImVec2 t1sz = ImGui::CalcTextSize(t1);
        ImGui::SetCursorPos(ImVec2((winW - t1sz.x) * 0.5f, cy));
        ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "%s", t1);
        cy += t1sz.y + 8.0f;
    }

    {
        const char* t2 = "Central de Conocimiento";
        ImVec2 t2sz = ImGui::CalcTextSize(t2);
        ImGui::SetCursorPos(ImVec2((winW - t2sz.x) * 0.5f, cy));
        ImGui::TextColored(ImVec4(0.45f, 0.46f, 0.60f, 1.0f), "%s", t2);
        cy += t2sz.y + 28.0f;
    }

    {
        constexpr float SEP_W = 340.0f;
        ImVec2 wpos = ImGui::GetWindowPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float sx = wpos.x + (winW - SEP_W) * 0.5f;
        float sy = wpos.y + cy;
        dl->AddLine(ImVec2(sx, sy), ImVec2(sx + SEP_W, sy),
                    IM_COL32(40, 44, 68, 200), 1.0f);
        cy += 21.0f;
    }

    {
        int  noteCount = (int)m_noteManager.getAllNotes().size();
        bool watcherOn = m_fileWatcher.isRunning();

        char statBuf[128];
        snprintf(statBuf, sizeof(statBuf),
                 "Notas totales: %d     |     Watcher: %s",
                 noteCount, watcherOn ? "activo" : "inactivo");

        ImVec2 statsz = ImGui::CalcTextSize(statBuf);
        ImGui::SetCursorPos(ImVec2((winW - statsz.x) * 0.5f, cy));
        ImGui::TextColored(ImVec4(0.38f, 0.40f, 0.52f, 1.0f), "%s", statBuf);
        cy += statsz.y + 22.0f;
    }

    
    {
        constexpr float BTN_W   = 190.0f;
        constexpr float BTN_H   = 38.0f;
        constexpr float BTN_GAP = 16.0f;
        float rowW = BTN_W * 2.0f + BTN_GAP;
        float startX = (winW - rowW) * 0.5f;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        ImGui::SetCursorPos(ImVec2(startX, cy));
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImVec4(0.10f, 0.28f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.14f, 0.42f, 0.26f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4(0.08f, 0.22f, 0.14f, 1.0f));
        if (ImGui::Button("  + Crear Nueva Nota  ", ImVec2(BTN_W, BTN_H))) {
            std::memset(m_hubNewNoteName, 0, sizeof(m_hubNewNoteName));
            ImGui::OpenPopup("Crear Nueva Nota");
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Crear una nota Markdown en blanco");

        ImGui::SetCursorPos(ImVec2(startX + BTN_W + BTN_GAP, cy));
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImVec4(0.14f, 0.18f, 0.34f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.20f, 0.28f, 0.50f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4(0.10f, 0.14f, 0.26f, 1.0f));
        if (ImGui::Button("  Cargar Archivo (.md/.txt)  ", ImVec2(BTN_W, BTN_H))) {
            std::string path = openFileDialog();
            if (!path.empty()) {
                importFileToNotes(path);
                currentState = AppState::ENTORNO_NOTAS;
            }
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Importar un archivo .md o .txt existente\n"
                              "al directorio de notas");

        ImGui::PopStyleVar(); 
        cy += BTN_H + 16.0f;
    }

    {
        constexpr float SEP_W = 340.0f;
        ImVec2 wpos = ImGui::GetWindowPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float sx = wpos.x + (winW - SEP_W) * 0.5f;
        float sy = wpos.y + cy;
        dl->AddLine(ImVec2(sx, sy), ImVec2(sx + SEP_W, sy),
                    IM_COL32(40, 44, 68, 200), 1.0f);
        cy += 21.0f;
    }

    
    {
        constexpr float BTN_W = 280.0f;
        constexpr float BTN_H = 44.0f;

        ImGui::SetCursorPos(ImVec2((winW - BTN_W) * 0.5f, cy));

        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImVec4(0.08f, 0.38f, 0.42f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.12f, 0.52f, 0.58f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4(0.06f, 0.28f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        if (ImGui::Button("  ACCEDER A MIS NOTAS  ", ImVec2(BTN_W, BTN_H))) {
            currentState = AppState::ENTORNO_NOTAS;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Entrar al entorno de notas enlazadas");
    }

    
    {
        ImVec2 mc = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(mc, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Crear Nueva Nota", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f),
                               "Nueva nota en blanco");
            ImGui::Spacing();

            ImGui::Text("Nombre:");
            ImGui::SetNextItemWidth(350.0f);
            bool enterHit = ImGui::InputText(
                "##new_note_name", m_hubNewNoteName, sizeof(m_hubNewNoteName),
                ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            ImGui::Spacing();

            bool canCreate = m_hubNewNoteName[0] != '\0';

            if (!canCreate) ImGui::BeginDisabled();
            if (ImGui::Button("  Crear  ", ImVec2(120.0f, 0.0f)) || (enterHit && canCreate)) {
                auto* note = m_noteManager.createNote(m_hubNewNoteName);
                if (note) {
                    note->content = "# " + std::string(m_hubNewNoteName) + "\n\n";
                    note->saveToFile();
                    note->parseLinks();
                    note->parseTitle();
                    selectedNoteId    = note->id;
                    graphNeedsRebuild = true;
                    currentState      = AppState::ENTORNO_NOTAS;
                }
                ImGui::CloseCurrentPopup();
            }
            if (!canCreate) ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("  Cancelar  ", ImVec2(120.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

} 
