

#define SOL_ALL_SAFETIES_ON 1
#include "lua_manager.h"
#include "app.h"

#include <sol/sol.hpp>
#include <chrono>
#include <ctime>
#include <sstream>

namespace nodepanda {


std::string LuaManager::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm buf{};
#ifdef _WIN32
    localtime_s(&buf, &t);
#else
    localtime_r(&t, &buf);
#endif
    char ts[16];
    std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d", buf.tm_hour, buf.tm_min, buf.tm_sec);
    return std::string(ts);
}


LuaManager::LuaManager() = default;

LuaManager::~LuaManager() {
    if (m_lua) {
        lua_close(m_lua);
        m_lua = nullptr;
    }
}


void LuaManager::init(App* app) {
    m_app = app;

    if (m_lua) lua_close(m_lua);
    m_lua = luaL_newstate();

    luaL_requiref(m_lua, "base",   luaopen_base,   1); lua_pop(m_lua, 1);
    luaL_requiref(m_lua, "string", luaopen_string,  1); lua_pop(m_lua, 1);
    luaL_requiref(m_lua, "table",  luaopen_table,   1); lua_pop(m_lua, 1);
    luaL_requiref(m_lua, "math",   luaopen_math,    1); lua_pop(m_lua, 1);
    luaL_requiref(m_lua, "utf8",   luaopen_utf8,    1); lua_pop(m_lua, 1);

    setupSandbox();
    registerBindings();

    logInfo("Lua 5.4 inicializado (sandbox activo)");
}


void LuaManager::setupSandbox() {
    const char* sandbox = R"(
        dofile     = nil
        loadfile   = nil
        rawget     = nil
        rawset     = nil
        rawequal   = nil
        rawlen     = nil
        collectgarbage = nil
    )";
    luaL_dostring(m_lua, sandbox);
}


void LuaManager::registerBindings() {
    sol::state_view lua(m_lua);

    lua.set_function("print", [this](sol::variadic_args va) {
        std::ostringstream oss;
        bool first = true;
        for (auto arg : va) {
            if (!first) oss << "\t";
            first = false;
            sol::object obj = arg;
            if (obj.is<std::string>()) {
                oss << obj.as<std::string>();
            } else if (obj.is<double>()) {
                double v = obj.as<double>();
                if (v == (int)v)
                    oss << (int)v;
                else
                    oss << v;
            } else if (obj.is<bool>()) {
                oss << (obj.as<bool>() ? "true" : "false");
            } else if (obj.is<sol::nil_t>()) {
                oss << "nil";
            } else {
                oss << sol::type_name(m_lua, obj.get_type());
            }
        }
        oss << "\n";
        m_capturedOutput += oss.str();
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

    grafo.set_function("tipo", [this](const std::string& noteId) -> std::string {
        int idx = m_app->getGraph().findNodeIndex(noteId);
        if (idx < 0) return "";
        return m_app->getGraph().getNodes()[idx].nodeType;
    });

    sol::table appT = lua.create_named_table("app");

    appT.set_function("fps", [this]() -> float {
        return m_app->getPerformanceMetrics().fps();
    });

    appT.set_function("ram_mb", [this]() -> float {
        return m_app->getPerformanceMetrics().memoryMB();
    });

    // ── API: memoria (Motor de Memoria TF-IDF) ─────────────────────────────
    sol::table mem = lua.create_named_table("memoria");

    mem.set_function("similares", [this](const std::string& noteId, int topN) -> sol::table {
        auto results = m_app->getMemoryEngine().findSimilar(noteId, topN);
        sol::state_view lua(m_lua);
        sol::table t = lua.create_table();
        int i = 1;
        for (const auto& r : results) {
            sol::table entry = lua.create_table();
            entry["id"]    = r.noteId;
            entry["titulo"] = r.title;
            entry["score"] = r.score;
            t[i++] = entry;
        }
        return t;
    });

    mem.set_function("buscar", [this](const std::string& query, int topN) -> sol::table {
        auto results = m_app->getMemoryEngine().search(query, topN);
        sol::state_view lua(m_lua);
        sol::table t = lua.create_table();
        int i = 1;
        for (const auto& r : results) {
            sol::table entry = lua.create_table();
            entry["id"]    = r.noteId;
            entry["titulo"] = r.title;
            entry["score"] = r.score;
            t[i++] = entry;
        }
        return t;
    });

    mem.set_function("vocabulario", [this]() -> int {
        return m_app->getMemoryEngine().vocabularySize();
    });

    mem.set_function("indexadas", [this]() -> int {
        return m_app->getMemoryEngine().indexedNotes();
    });

    mem.set_function("reindexar", [this]() {
        m_app->getMemoryEngine().reindex(m_app->getNoteManager());
    });
}


LuaResult LuaManager::execute(const std::string& code) {
    LuaResult result;
    m_capturedOutput.clear();

    auto start = std::chrono::high_resolution_clock::now();

    if (timeoutMs > 0) {
        struct TimeoutData { std::chrono::high_resolution_clock::time_point deadline; };
        auto* td = new TimeoutData{start + std::chrono::milliseconds(timeoutMs)};

        lua_setallocf(m_lua, nullptr, nullptr); 
        lua_sethook(m_lua, [](lua_State* L, lua_Debug*) {
            lua_getfield(L, LUA_REGISTRYINDEX, "__np_deadline");
            if (lua_islightuserdata(L, -1)) {
                auto* d = (TimeoutData*)lua_touserdata(L, -1);
                if (std::chrono::high_resolution_clock::now() > d->deadline) {
                    lua_pop(L, 1);
                    luaL_error(L, "Timeout: el script excedio el limite de tiempo");
                    return;
                }
            }
            lua_pop(L, 1);
        }, LUA_MASKCOUNT, 10000); 

        lua_pushlightuserdata(m_lua, td);
        lua_setfield(m_lua, LUA_REGISTRYINDEX, "__np_deadline");

        sol::state_view lua(m_lua);
        auto r = lua.safe_script(code, sol::script_pass_on_error);

        lua_sethook(m_lua, nullptr, 0, 0);
        lua_pushnil(m_lua);
        lua_setfield(m_lua, LUA_REGISTRYINDEX, "__np_deadline");
        delete td;

        if (!r.valid()) {
            sol::error err = r;
            result.error = err.what();
        } else {
            result.success = true;
            sol::object retVal = r;
            if (retVal.valid() && retVal.get_type() != sol::type::none
                && retVal.get_type() != sol::type::nil) {
                if (retVal.is<std::string>())
                    m_capturedOutput += "=> " + retVal.as<std::string>() + "\n";
                else if (retVal.is<double>()) {
                    double v = retVal.as<double>();
                    if (v == (int)v)
                        m_capturedOutput += "=> " + std::to_string((int)v) + "\n";
                    else
                        m_capturedOutput += "=> " + std::to_string(v) + "\n";
                } else if (retVal.is<bool>()) {
                    m_capturedOutput += std::string("=> ") + (retVal.as<bool>() ? "true" : "false") + "\n";
                }
            }
        }
    } else {
        sol::state_view lua(m_lua);
        auto r = lua.safe_script(code, sol::script_pass_on_error);
        if (!r.valid()) {
            sol::error err = r;
            result.error = err.what();
        } else {
            result.success = true;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    result.output = m_capturedOutput;

    std::string ts = currentTimestamp();
    if (!result.output.empty()) {
        m_log.push_back({LuaLogEntry::OUTPUT, result.output, ts});
    }
    if (!result.error.empty()) {
        m_log.push_back({LuaLogEntry::LOG_ERROR, result.error, ts});
    }

    return result;
}


void LuaManager::clearLog() {
    m_log.clear();
}

void LuaManager::logInfo(const std::string& text) {
    m_log.push_back({LuaLogEntry::INFO, text, currentTimestamp()});
}

} 
