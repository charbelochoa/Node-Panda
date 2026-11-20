#define SOL_ALL_SAFETIES_ON 1
#include "plugin_manager.h"
#include "app.h"
#include "note_manager.h"
#include "memory_engine.h"
#include "graph.h"

#include <sol/sol.hpp>
#include "imgui.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <cstdio>

namespace nodepanda {

namespace json_mini {

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\n\r");
    size_t b = s.find_last_not_of(" \t\n\r");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static std::string extractString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static bool extractBool(const std::string& json, const std::string& key, bool defaultVal) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return defaultVal;
    std::string rest = trim(json.substr(pos + 1));
    if (rest.rfind("true", 0) == 0) return true;
    if (rest.rfind("false", 0) == 0) return false;
    return defaultVal;
}

static std::vector<std::string> extractStringArray(const std::string& json,
                                                    const std::string& key) {
    std::vector<std::string> result;
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return result;
    pos = json.find('[', pos + needle.size());
    if (pos == std::string::npos) return result;
    size_t end = json.find(']', pos);
    if (end == std::string::npos) return result;
    std::string arr = json.substr(pos + 1, end - pos - 1);
    size_t cur = 0;
    while (cur < arr.size()) {
        size_t q1 = arr.find('"', cur);
        if (q1 == std::string::npos) break;
        size_t q2 = arr.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        result.push_back(arr.substr(q1 + 1, q2 - q1 - 1));
        cur = q2 + 1;
    }
    return result;
}

}

static std::string currentDate() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm buf{};
#ifdef _WIN32
    localtime_s(&buf, &t);
#else
    localtime_r(&t, &buf);
#endif
    char date[16];
    std::snprintf(date, sizeof(date), "%04d-%02d-%02d",
                  buf.tm_year + 1900, buf.tm_mon + 1, buf.tm_mday);
    return std::string(date);
}

PluginManager::PluginManager() = default;

PluginManager::~PluginManager() {
    shutdown();
}

void PluginManager::init(App* app) {
    m_app = app;
    m_pluginsDir = std::filesystem::current_path() / "data" / "plugins";

    if (!std::filesystem::exists(m_pluginsDir)) {
        std::filesystem::create_directories(m_pluginsDir);
    }

    loadPluginStates();
    discoverPlugins();
    callOnLoad();
}

void PluginManager::shutdown() {

    for (auto& plugin : m_plugins) {
        if (plugin.loaded && plugin.manifest.enabled) {
            callHook(plugin, "on_unload");
        }
        destroyPlugin(plugin);
    }
    m_plugins.clear();
    m_menuItems.clear();
    m_panels.clear();
    m_notifications.clear();
}

void PluginManager::discoverPlugins() {
    m_plugins.clear();
    m_menuItems.clear();
    m_panels.clear();

    if (!std::filesystem::exists(m_pluginsDir)) return;

    for (const auto& entry : std::filesystem::directory_iterator(m_pluginsDir)) {
        if (!entry.is_directory()) continue;

        auto manifestPath = entry.path() / "manifest.json";
        auto pluginPath   = entry.path() / "plugin.lua";

        if (!std::filesystem::exists(manifestPath) ||
            !std::filesystem::exists(pluginPath)) {
            continue;
        }

        PluginManifest manifest;
        if (!parseManifest(manifestPath, manifest)) continue;

        PluginInstance instance;
        instance.manifest  = manifest;
        instance.directory = entry.path();

        if (manifest.enabled) {
            initPluginLua(instance);
            loadPluginScript(instance);
        }

        m_plugins.push_back(std::move(instance));
    }

    fprintf(stderr, "[PluginManager] Descubiertos %d plugins en %s\n",
            (int)m_plugins.size(), m_pluginsDir.string().c_str());
}

bool PluginManager::parseManifest(const std::filesystem::path& manifestPath,
                                   PluginManifest& out) {
    std::ifstream file(manifestPath);
    if (!file.is_open()) return false;

    std::stringstream ss;
    ss << file.rdbuf();
    std::string json = ss.str();

    out.id          = json_mini::extractString(json, "id");
    out.name        = json_mini::extractString(json, "name");
    out.version     = json_mini::extractString(json, "version");
    out.author      = json_mini::extractString(json, "author");
    out.description = json_mini::extractString(json, "description");
    out.hooks       = json_mini::extractStringArray(json, "hooks");
    out.enabled     = json_mini::extractBool(json, "enabled", true);

    if (out.id.empty()) {
        out.id = manifestPath.parent_path().filename().string();
    }
    if (out.name.empty()) out.name = out.id;

    return true;
}

void PluginManager::initPluginLua(PluginInstance& plugin) {
    if (plugin.luaState) {
        lua_close(plugin.luaState);
        plugin.luaState = nullptr;
    }

    plugin.luaState = luaL_newstate();

    luaL_requiref(plugin.luaState, "base",   luaopen_base,   1); lua_pop(plugin.luaState, 1);
    luaL_requiref(plugin.luaState, "string", luaopen_string,  1); lua_pop(plugin.luaState, 1);
    luaL_requiref(plugin.luaState, "table",  luaopen_table,   1); lua_pop(plugin.luaState, 1);
    luaL_requiref(plugin.luaState, "math",   luaopen_math,    1); lua_pop(plugin.luaState, 1);
    luaL_requiref(plugin.luaState, "utf8",   luaopen_utf8,    1); lua_pop(plugin.luaState, 1);

    const char* sandbox = R"(
        dofile     = nil
        loadfile   = nil
        rawget     = nil
        rawset     = nil
        rawequal   = nil
        rawlen     = nil
        collectgarbage = nil
        os         = nil
        io         = nil
        package    = nil
        require    = nil
    )";
    luaL_dostring(plugin.luaState, sandbox);

    registerPluginBindings(plugin);
}

void PluginManager::registerPluginBindings(PluginInstance& plugin) {
    sol::state_view lua(plugin.luaState);
    const std::string pluginId = plugin.manifest.id;

    lua.set_function("print", [this](sol::variadic_args va) {
        std::ostringstream oss;
        bool first = true;
        sol::state_view lv(va.lua_state());
        for (auto arg : va) {
            if (!first) oss << "\t";
            first = false;
            sol::object obj = arg;
            if      (obj.is<std::string>()) oss << obj.as<std::string>();
            else if (obj.is<double>())      { double v = obj.as<double>(); oss << (v == (int)v ? std::to_string((int)v) : std::to_string(v)); }
            else if (obj.is<bool>())        oss << (obj.as<bool>() ? "true" : "false");
            else if (obj.is<sol::nil_t>())  oss << "nil";
            else                            oss << sol::type_name(va.lua_state(), obj.get_type());
        }
        m_app->getLuaManager().logInfo("[plugin] " + oss.str());
    });

    sol::table notas = lua.create_named_table("notas");

    notas.set_function("contar", [this]() -> int {
        return (int)m_app->getNoteManager().getAllNotes().size();
    });

    notas.set_function("listar", [this]() -> sol::as_table_t<std::vector<std::string>> {
        std::vector<std::string> ids;
        for (const auto& [id, _] : m_app->getNoteManager().getAllNotes())
            ids.push_back(id);
        return sol::as_table(std::move(ids));
    });

    notas.set_function("contenido", [this](const std::string& id) -> std::string {
        auto* note = m_app->getNoteManager().getNoteById(id);
        return note ? note->content : "";
    });

    notas.set_function("titulo", [this](const std::string& id) -> std::string {
        auto* note = m_app->getNoteManager().getNoteById(id);
        return note ? note->title : "";
    });

    notas.set_function("enlaces", [this](const std::string& id)
            -> sol::as_table_t<std::vector<std::string>> {
        auto* note = m_app->getNoteManager().getNoteById(id);
        if (!note) return sol::as_table(std::vector<std::string>{});
        std::vector<std::string> links(note->links.begin(), note->links.end());
        return sol::as_table(std::move(links));
    });

    notas.set_function("meta", [this](const std::string& id, const std::string& key) -> std::string {
        auto* note = m_app->getNoteManager().getNoteById(id);
        if (!note) return "";
        auto it = note->frontmatter.find(key);
        return (it != note->frontmatter.end()) ? it->second : "";
    });

    notas.set_function("seleccionada", [this]() -> std::string {
        return m_app->selectedNoteId;
    });

    notas.set_function("existe", [this](const std::string& id) -> bool {
        return m_app->getNoteManager().getNoteById(id) != nullptr;
    });

    notas.set_function("crear", [this](const std::string& nombre, sol::optional<std::string> contenido) -> bool {
        auto* note = m_app->getNoteManager().createNote(nombre);
        if (!note) return false;
        if (contenido.has_value()) {
            note->content = contenido.value();
            note->dirty = true;
            note->saveToFile();
        }
        note->parseLinks();
        note->parseTitle();
        m_app->graphNeedsRebuild = true;
        return true;
    });

    notas.set_function("editar", [this](const std::string& id, const std::string& contenido) -> bool {
        auto* note = m_app->getNoteManager().getNoteById(id);
        if (!note) return false;
        note->content = contenido;
        note->dirty = true;
        note->saveToFile();
        note->parseLinks();
        note->parseTitle();
        m_app->graphNeedsRebuild = true;
        return true;
    });

    notas.set_function("abrir", [this](const std::string& id) {
        m_app->selectedNoteId = id;
    });

    notas.set_function("establecer_meta", [this](const std::string& id,
                                                   const std::string& key,
                                                   const std::string& value) -> bool {
        auto* note = m_app->getNoteManager().getNoteById(id);
        if (!note) return false;
        note->setFrontmatter(key, value);
        note->dirty = true;
        note->saveToFile();
        return true;
    });

    sol::table grafo = lua.create_named_table("grafo");

    grafo.set_function("nodos", [this]() -> int {
        return (int)m_app->getGraph().getNodes().size();
    });

    grafo.set_function("aristas", [this]() -> int {
        return (int)m_app->getGraph().getEdges().size();
    });

    grafo.set_function("vecinos", [this](const std::string& noteId)
            -> sol::as_table_t<std::vector<std::string>> {
        auto& graph = m_app->getGraph();
        auto& nodes = graph.getNodes();
        auto& edges = graph.getEdges();
        int idx = graph.findNodeIndex(noteId);
        std::vector<std::string> result;
        if (idx >= 0) {
            for (const auto& e : edges) {
                if (e.from == idx) result.push_back(nodes[e.to].noteId);
                if (e.to   == idx) result.push_back(nodes[e.from].noteId);
            }
        }
        return sol::as_table(std::move(result));
    });

    grafo.set_function("conexiones", [this](const std::string& noteId) -> int {
        int idx = m_app->getGraph().findNodeIndex(noteId);
        if (idx < 0) return 0;
        return m_app->getGraph().getNodes()[idx].connectionCount;
    });

    sol::table mem = lua.create_named_table("memoria");

    lua_State* pluginLua = plugin.luaState;

    mem.set_function("similares", [this, pluginLua](const std::string& noteId, int topN) -> sol::table {
        auto results = m_app->getMemoryEngine().findSimilar(noteId, topN);
        sol::state_view lv(pluginLua);
        sol::table t = lv.create_table();
        int i = 1;
        for (const auto& r : results) {
            sol::table entry = lv.create_table();
            entry["id"]    = r.noteId;
            entry["titulo"] = r.title;
            entry["score"] = r.score;
            t[i++] = entry;
        }
        return t;
    });

    mem.set_function("buscar", [this, pluginLua](const std::string& query, int topN) -> sol::table {
        auto results = m_app->getMemoryEngine().search(query, topN);
        sol::state_view lv(pluginLua);
        sol::table t = lv.create_table();
        int i = 1;
        for (const auto& r : results) {
            sol::table entry = lv.create_table();
            entry["id"]    = r.noteId;
            entry["titulo"] = r.title;
            entry["score"] = r.score;
            t[i++] = entry;
        }
        return t;
    });

    sol::table appT = lua.create_named_table("app");

    appT.set_function("fecha_hoy", []() -> std::string {
        return currentDate();
    });

    appT.set_function("fps", [this]() -> float {
        return m_app->getPerformanceMetrics().fps();
    });

    appT.set_function("ram_mb", [this]() -> float {
        return m_app->getPerformanceMetrics().memoryMB();
    });

    appT.set_function("persistir", [this, pluginId](const std::string& key,
                                                      const std::string& value) {
        auto path = m_pluginsDir / pluginId / "state.json";

        std::unordered_map<std::string, std::string> state;
        {
            std::ifstream f(path);
            if (f.is_open()) {
                std::string json((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());

                size_t pos = 0;
                while (pos < json.size()) {
                    size_t q1 = json.find('"', pos);
                    if (q1 == std::string::npos) break;
                    size_t q2 = json.find('"', q1 + 1);
                    if (q2 == std::string::npos) break;
                    std::string k = json.substr(q1 + 1, q2 - q1 - 1);
                    size_t q3 = json.find('"', q2 + 1);
                    if (q3 == std::string::npos) break;
                    size_t q4 = json.find('"', q3 + 1);
                    if (q4 == std::string::npos) break;
                    std::string v = json.substr(q3 + 1, q4 - q3 - 1);
                    state[k] = v;
                    pos = q4 + 1;
                }
            }
        }
        state[key] = value;

        std::ofstream out(path);
        if (out.is_open()) {
            out << "{\n";
            bool first = true;
            for (const auto& [k, v] : state) {
                if (!first) out << ",\n";
                first = false;
                out << "  \"" << k << "\": \"" << v << "\"";
            }
            out << "\n}\n";
        }
    });

    appT.set_function("leer_persistido", [this, pluginId](const std::string& key) -> std::string {
        auto path = m_pluginsDir / pluginId / "state.json";
        std::ifstream f(path);
        if (!f.is_open()) return "";
        std::string json((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
        return json_mini::extractString(json, key);
    });

    sol::table ui = lua.create_named_table("ui");

    ui.set_function("menu_item", [this, pluginId](const std::string& label,
                                                    sol::function callback,
                                                    sol::this_state ts) {
        lua_State* L = ts;

        callback.push();
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        registerMenuItem(pluginId, label, ref, L);
    });

    ui.set_function("panel", [this, pluginId](const std::string& name,
                                                sol::function renderFn,
                                                sol::this_state ts) {
        lua_State* L = ts;
        renderFn.push();
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        registerPanel(pluginId, name, ref, L);
    });

    ui.set_function("notificacion", [this](const std::string& text,
                                            sol::optional<std::string> type) {
        addNotification(text, type.has_value() ? type.value() : "info");
    });

    sol::table ig = lua.create_named_table("imgui");

    ig.set_function("texto", [](const std::string& text) {
        ImGui::TextUnformatted(text.c_str());
    });

    ig.set_function("texto_color", [](float r, float g, float b, float a, const std::string& text) {
        ImGui::TextColored(ImVec4(r, g, b, a), "%s", text.c_str());
    });

    ig.set_function("texto_wrapped", [](const std::string& text) {
        ImGui::TextWrapped("%s", text.c_str());
    });

    ig.set_function("texto_disabled", [](const std::string& text) {
        ImGui::TextDisabled("%s", text.c_str());
    });

    ig.set_function("boton", [](const std::string& label) -> bool {
        return ImGui::Button(label.c_str());
    });

    ig.set_function("boton_pequeno", [](const std::string& label) -> bool {
        return ImGui::SmallButton(label.c_str());
    });

    ig.set_function("separador", []() {
        ImGui::Separator();
    });

    ig.set_function("espacio", []() {
        ImGui::Spacing();
    });

    ig.set_function("misma_linea", [](sol::optional<float> offset) {
        if (offset.has_value())
            ImGui::SameLine(offset.value());
        else
            ImGui::SameLine();
    });

    ig.set_function("nueva_linea", []() {
        ImGui::NewLine();
    });

    ig.set_function("tree_node", [](const std::string& label) -> bool {
        return ImGui::TreeNode(label.c_str());
    });

    ig.set_function("tree_pop", []() {
        ImGui::TreePop();
    });

    ig.set_function("collapsing_header", [](const std::string& label) -> bool {
        return ImGui::CollapsingHeader(label.c_str());
    });

    ig.set_function("indent", [](sol::optional<float> w) {
        ImGui::Indent(w.value_or(0.0f));
    });

    ig.set_function("unindent", [](sol::optional<float> w) {
        ImGui::Unindent(w.value_or(0.0f));
    });

    ig.set_function("begin_child", [](const std::string& id, sol::optional<float> w,
                                       sol::optional<float> h) -> bool {
        return ImGui::BeginChild(id.c_str(), ImVec2(w.value_or(0), h.value_or(0)),
                                 true, ImGuiWindowFlags_HorizontalScrollbar);
    });

    ig.set_function("end_child", []() {
        ImGui::EndChild();
    });

    ig.set_function("columns", [](int count, sol::optional<std::string> id) {
        ImGui::Columns(count, id.has_value() ? id.value().c_str() : nullptr);
    });

    ig.set_function("next_column", []() {
        ImGui::NextColumn();
    });

    ig.set_function("columns_end", []() {
        ImGui::Columns(1);
    });

    ig.set_function("bullet", []() {
        ImGui::Bullet();
    });

    ig.set_function("bullet_text", [](const std::string& text) {
        ImGui::BulletText("%s", text.c_str());
    });

    ig.set_function("progress_bar", [](float fraction, sol::optional<std::string> overlay) {
        ImGui::ProgressBar(fraction, ImVec2(-1, 0),
                           overlay.has_value() ? overlay.value().c_str() : nullptr);
    });

    ig.set_function("tooltip", [](const std::string& text) {
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", text.c_str());
    });

    ig.set_function("push_color", [](int idx, float r, float g, float b, float a) {
        ImGui::PushStyleColor(idx, ImVec4(r, g, b, a));
    });

    ig.set_function("pop_color", [](sol::optional<int> count) {
        ImGui::PopStyleColor(count.value_or(1));
    });

    ig.set_function("selectable", [](const std::string& label, bool selected) -> bool {
        return ImGui::Selectable(label.c_str(), selected);
    });

    ig.set_function("tab_bar_begin", [](const std::string& id) -> bool {
        return ImGui::BeginTabBar(id.c_str());
    });

    ig.set_function("tab_item_begin", [](const std::string& label) -> bool {
        return ImGui::BeginTabItem(label.c_str());
    });

    ig.set_function("tab_item_end", []() {
        ImGui::EndTabItem();
    });

    ig.set_function("tab_bar_end", []() {
        ImGui::EndTabBar();
    });

    ig.set_function("ancho_disponible", []() -> float {
        return ImGui::GetContentRegionAvail().x;
    });

    ig.set_function("alto_disponible", []() -> float {
        return ImGui::GetContentRegionAvail().y;
    });

    appT.set_function("guardar_archivo", [this](const std::string& filename,
                                                  const std::string& content) -> bool {
        auto path = std::filesystem::current_path() / "data" / filename;
        std::ofstream out(path);
        if (!out.is_open()) return false;
        out << content;
        return true;
    });

    ui.set_function("tema", [](sol::table colors) {
        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4* c = s.Colors;

        auto getColor = [&](const std::string& key) -> ImVec4 {
            sol::optional<std::string> hex = colors[key];
            if (!hex.has_value() || hex.value().size() != 6) return ImVec4(-1, -1, -1, -1);
            unsigned int val = 0;
            for (char ch : hex.value()) {
                val <<= 4;
                if (ch >= '0' && ch <= '9') val |= (ch - '0');
                else if (ch >= 'a' && ch <= 'f') val |= (ch - 'a' + 10);
                else if (ch >= 'A' && ch <= 'F') val |= (ch - 'A' + 10);
            }
            return ImVec4(((val >> 16) & 0xFF) / 255.0f,
                          ((val >> 8) & 0xFF) / 255.0f,
                          (val & 0xFF) / 255.0f, 1.0f);
        };

        auto apply = [&](const std::string& key, int idx) {
            ImVec4 col = getColor(key);
            if (col.x >= 0) c[idx] = col;
        };

        apply("bg",          ImGuiCol_WindowBg);
        apply("bg_child",    ImGuiCol_ChildBg);
        apply("bg_popup",    ImGuiCol_PopupBg);
        apply("text",        ImGuiCol_Text);
        apply("text_dim",    ImGuiCol_TextDisabled);
        apply("border",      ImGuiCol_Border);
        apply("frame_bg",    ImGuiCol_FrameBg);
        apply("title_bg",    ImGuiCol_TitleBgActive);
        apply("menu_bg",     ImGuiCol_MenuBarBg);
        apply("button",      ImGuiCol_Button);
        apply("button_hover",ImGuiCol_ButtonHovered);
        apply("header",      ImGuiCol_Header);
        apply("header_hover",ImGuiCol_HeaderHovered);
        apply("tab",         ImGuiCol_Tab);
        apply("tab_hover",   ImGuiCol_TabHovered);
        apply("tab_active",  ImGuiCol_TabActive);
        apply("accent",      ImGuiCol_CheckMark);
        apply("accent",      ImGuiCol_SliderGrab);
        apply("separator",   ImGuiCol_Separator);
        apply("scrollbar",   ImGuiCol_ScrollbarGrab);
    });
}

void PluginManager::loadPluginScript(PluginInstance& plugin) {
    if (!plugin.luaState) return;

    auto scriptPath = plugin.directory / "plugin.lua";
    sol::state_view lua(plugin.luaState);

    auto result = lua.safe_script_file(scriptPath.string(), sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        plugin.hasError = true;
        plugin.lastError = err.what();
        fprintf(stderr, "[PluginManager] Error cargando %s: %s\n",
                plugin.manifest.id.c_str(), err.what());
    } else {
        plugin.loaded = true;
        fprintf(stderr, "[PluginManager] Plugin cargado: %s v%s\n",
                plugin.manifest.name.c_str(), plugin.manifest.version.c_str());
    }
}

void PluginManager::callHook(PluginInstance& plugin, const std::string& hookName) {
    if (!plugin.luaState || !plugin.loaded || !plugin.manifest.enabled) return;

    auto& hooks = plugin.manifest.hooks;
    if (std::find(hooks.begin(), hooks.end(), hookName) == hooks.end()) return;

    sol::state_view lua(plugin.luaState);
    sol::optional<sol::function> fn = lua[hookName];
    if (!fn.has_value()) return;

    auto result = fn.value().call();
    if (!result.valid()) {
        sol::error err = result;
        plugin.hasError = true;
        plugin.lastError = err.what();
        fprintf(stderr, "[PluginManager] Error en %s.%s: %s\n",
                plugin.manifest.id.c_str(), hookName.c_str(), err.what());

        if (hookName == "on_frame") {
            plugin.consecutiveErrors++;
            if (plugin.consecutiveErrors >= PluginInstance::kMaxFrameErrors) {
                plugin.manifest.enabled = false;
                addNotification("Plugin '" + plugin.manifest.name +
                    "' desactivado por errores repetidos: " + err.what(), "error");
                fprintf(stderr, "[PluginManager] Plugin %s auto-desactivado tras %d errores\n",
                        plugin.manifest.id.c_str(), plugin.consecutiveErrors);
            }
        }
    } else {

        if (hookName == "on_frame") {
            plugin.consecutiveErrors = 0;
        }
    }
}

void PluginManager::callHookWithArg(PluginInstance& plugin, const std::string& hookName,
                                     const std::string& arg) {
    if (!plugin.luaState || !plugin.loaded || !plugin.manifest.enabled) return;

    auto& hooks = plugin.manifest.hooks;
    if (std::find(hooks.begin(), hooks.end(), hookName) == hooks.end()) return;

    sol::state_view lua(plugin.luaState);
    sol::optional<sol::function> fn = lua[hookName];
    if (!fn.has_value()) return;

    auto result = fn.value().call(arg);
    if (!result.valid()) {
        sol::error err = result;
        plugin.hasError = true;
        plugin.lastError = err.what();
        fprintf(stderr, "[PluginManager] Error en %s.%s(%s): %s\n",
                plugin.manifest.id.c_str(), hookName.c_str(), arg.c_str(), err.what());
    }
}

void PluginManager::callOnLoad() {
    for (auto& plugin : m_plugins) {
        callHook(plugin, "on_load");
    }
}

void PluginManager::callOnFrame() {
    for (auto& plugin : m_plugins) {
        callHook(plugin, "on_frame");
    }
}

void PluginManager::callOnNoteOpen(const std::string& noteId) {
    for (auto& plugin : m_plugins) {
        callHookWithArg(plugin, "on_note_open", noteId);
    }
}

void PluginManager::callOnNoteSave(const std::string& noteId) {
    for (auto& plugin : m_plugins) {
        callHookWithArg(plugin, "on_note_save", noteId);
    }
}

void PluginManager::callOnNoteCreate(const std::string& noteId) {
    for (auto& plugin : m_plugins) {
        callHookWithArg(plugin, "on_note_create", noteId);
    }
}

void PluginManager::enablePlugin(const std::string& pluginId) {
    for (auto& plugin : m_plugins) {
        if (plugin.manifest.id == pluginId) {
            plugin.manifest.enabled = true;
            if (!plugin.luaState) {
                initPluginLua(plugin);
                loadPluginScript(plugin);
            }
            callHook(plugin, "on_load");
            savePluginStates();
            return;
        }
    }
}

void PluginManager::disablePlugin(const std::string& pluginId) {
    for (auto& plugin : m_plugins) {
        if (plugin.manifest.id == pluginId) {
            callHook(plugin, "on_unload");
            plugin.manifest.enabled = false;

            m_menuItems.erase(
                std::remove_if(m_menuItems.begin(), m_menuItems.end(),
                    [&](const PluginMenuItem& mi) { return mi.pluginId == pluginId; }),
                m_menuItems.end());
            m_panels.erase(
                std::remove_if(m_panels.begin(), m_panels.end(),
                    [&](const PluginPanelData& p) { return p.pluginId == pluginId; }),
                m_panels.end());
            destroyPlugin(plugin);
            savePluginStates();
            return;
        }
    }
}

void PluginManager::reloadPlugin(const std::string& pluginId) {
    disablePlugin(pluginId);
    enablePlugin(pluginId);
}

void PluginManager::rescanPlugins() {
    shutdown();
    discoverPlugins();
    callOnLoad();
}

void PluginManager::registerMenuItem(const std::string& pluginId, const std::string& label,
                                      int callbackRef, lua_State* L) {
    PluginMenuItem item;
    item.pluginId    = pluginId;
    item.label       = label;
    item.callbackRef = callbackRef;
    item.luaState    = L;
    m_menuItems.push_back(std::move(item));
}

void PluginManager::registerPanel(const std::string& pluginId, const std::string& name,
                                    int renderRef, lua_State* L) {
    PluginPanelData panel;
    panel.pluginId  = pluginId;
    panel.name      = name;
    panel.renderRef = renderRef;
    panel.luaState  = L;
    m_panels.push_back(std::move(panel));
}

void PluginManager::addNotification(const std::string& text, const std::string& type) {
    PluginNotification notif;
    notif.text  = text;
    notif.type  = type;
    notif.timer = 4.0f;
    m_notifications.push_back(std::move(notif));
}

void PluginManager::destroyPlugin(PluginInstance& plugin) {
    if (plugin.luaState) {
        lua_close(plugin.luaState);
        plugin.luaState = nullptr;
    }
    plugin.loaded   = false;
    plugin.hasError = false;
    plugin.lastError.clear();
}

void PluginManager::savePluginStates() {
    auto path = m_pluginsDir / "plugin_states.json";
    std::ofstream out(path);
    if (!out.is_open()) return;
    out << "{\n";
    bool first = true;
    for (const auto& p : m_plugins) {
        if (!first) out << ",\n";
        first = false;
        out << "  \"" << p.manifest.id << "\": " << (p.manifest.enabled ? "true" : "false");
    }
    out << "\n}\n";
}

void PluginManager::loadPluginStates() {
    auto path = m_pluginsDir / "plugin_states.json";
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::stringstream ss;
    ss << file.rdbuf();
    std::string json = ss.str();

}

}
