#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>

struct lua_State;

namespace nodepanda {

class App;

struct PluginManifest {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::vector<std::string> hooks;
    bool enabled = true;
};

struct PluginInstance {
    PluginManifest manifest;
    std::filesystem::path directory;
    lua_State* luaState = nullptr;
    bool loaded  = false;
    bool hasError = false;
    std::string lastError;
    int consecutiveErrors = 0;
    static constexpr int kMaxFrameErrors = 3;
};

struct PluginMenuItem {
    std::string pluginId;
    std::string label;
    int callbackRef = -1;
    lua_State* luaState = nullptr;
};

struct PluginPanelData {
    std::string pluginId;
    std::string name;
    int renderRef = -1;
    lua_State* luaState = nullptr;
    bool open = false;
};

struct PluginNotification {
    std::string text;
    std::string type;
    float timer = 3.0f;
};

class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    void init(App* app);

    void shutdown();

    void callOnLoad();
    void callOnFrame();
    void callOnNoteOpen(const std::string& noteId);
    void callOnNoteSave(const std::string& noteId);
    void callOnNoteCreate(const std::string& noteId);

    void enablePlugin(const std::string& pluginId);
    void disablePlugin(const std::string& pluginId);
    void reloadPlugin(const std::string& pluginId);
    void rescanPlugins();

    const std::vector<PluginInstance>& getPlugins() const { return m_plugins; }
    std::vector<PluginInstance>& getPlugins() { return m_plugins; }

    const std::vector<PluginMenuItem>& getMenuItems() const { return m_menuItems; }
    const std::vector<PluginPanelData>& getPanels() const { return m_panels; }
    std::vector<PluginPanelData>& getPanels() { return m_panels; }
    std::vector<PluginNotification>& getNotifications() { return m_notifications; }

    void registerMenuItem(const std::string& pluginId, const std::string& label,
                          int callbackRef, lua_State* L);
    void registerPanel(const std::string& pluginId, const std::string& name,
                       int renderRef, lua_State* L);
    void addNotification(const std::string& text, const std::string& type);

private:
    void discoverPlugins();
    bool parseManifest(const std::filesystem::path& manifestPath, PluginManifest& out);
    void initPluginLua(PluginInstance& plugin);
    void registerPluginBindings(PluginInstance& plugin);
    void loadPluginScript(PluginInstance& plugin);
    void callHook(PluginInstance& plugin, const std::string& hookName);
    void callHookWithArg(PluginInstance& plugin, const std::string& hookName,
                         const std::string& arg);
    void destroyPlugin(PluginInstance& plugin);
    void savePluginStates();
    void loadPluginStates();

    App* m_app = nullptr;
    std::filesystem::path m_pluginsDir;
    std::vector<PluginInstance> m_plugins;
    std::vector<PluginMenuItem> m_menuItems;
    std::vector<PluginPanelData> m_panels;
    std::vector<PluginNotification> m_notifications;
};

}
