/**
 * @file mkopluginmanager.h
 * @brief DLL plugin loader and protocol dispatch hooks.
 *
 * Loads shared libraries from <viewer-binary-dir>/plugins at startup and
 * registers them as interceptors on the LLMessageSystem dispatch path.
 */

#ifndef MKO_PLUGIN_MANAGER_H
#define MKO_PLUGIN_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <functional>

#include "llsd.h"
#include "llhttpnode.h"
#include "llgl.h"
#include "llshadermgr.h"
#include "mko_plugin_api.h"

struct MkoSettingDef
{
    std::string label;
    std::string type;
    std::string default_value;
    std::string tab_id;   /* empty = default "plugins" tab */
    std::string options;  /* pipe-separated enum options */
    double min_value = 0.0;
    double max_value = 0.0;
};

struct MkoSettingsTab
{
    std::string id;
    std::string label;
};

class MkoPluginManager
{
public:
    static MkoPluginManager& instance();

    void init();
    void shutdown();

    static bool dispatchMessage(const std::string& msg_name,
                                const LLSD& message,
                                LLHTTPNode::ResponsePtr responsep);

    // Broadcast a message to all loaded protocol plugins.
    void broadcastToPlugins(const std::string& msg_name, const LLSD& message);

    // Plugin-accessible settings (Preferences > Graphics > plugin tabs).
    static int getSetting(const char* name, char* out, size_t out_len);
    static int setSetting(const char* name, const char* value);
    static int registerSetting(const char* name, const char* default_value,
                               const char* label, const char* type);
    static int registerSettingsTab(const char* id, const char* label);
    static int registerSetting2(const MkoSettingDesc2* setting);
    static const char* getGraphicsInfo(void);

    // UI entry points (used by the Preferences plugin-settings panel).
    static int setSettingFromUI(const char* name, const char* value);
    std::vector<MkoSettingsTab> getSettingsTabs() const;
    void getSettingsForTab(const std::string& tab_id,
                           std::vector<std::pair<std::string, MkoSettingDef> >& out) const;

    // Shader override registration (called from the plugin API).
    static int registerShader(const MkoShaderDesc* desc);
    static int unregisterShader(const char* name, MkoShaderType type);
    std::string getShaderSource(const std::string& name, GLenum type) const;

    // <Mko> API v8: HTML overlay support.
    static int registerHtmlOverlay(const MkoHtmlOverlayDesc* desc);
    static int unregisterHtmlOverlay(const char* id);
    static int showHtmlOverlay(const char* id, int visible);
    static int navigateHtmlOverlay(const char* id, const char* url);

    // <Mko> API v8: embedded script editor.
    static int openScriptEditor(const char* id, const char* title,
                                const char* language, const char* content);
    static int closeScriptEditor(const char* id);

    // C++ callback invoked by the editor floater when a script is saved.
    // The callback receives the MkoScriptEditorSaved payload.
    void registerScriptSaveCallback(const std::string& id,
                                    std::function<void(const LLSD&)> callback);
    void unregisterScriptSaveCallback(const std::string& id);
    void onScriptEditorSaved(const LLSD& message);

    // <Mko> API v8: health & region stats.
    static int getAvatarHealth();
    static int getAvatarHealthMax();
    static LLSD getRegionStatsLLSD();
    static std::string getRegionStatsText();

    // <Mko> API v8: async HTTP; results delivered via msg_name.
    static int httpRequest(const char* url, const char* method,
                           const char* body, const char* msg_name);
    // </Mko>

private:
    MkoPluginManager() = default;
    ~MkoPluginManager();

    MkoPluginManager(const MkoPluginManager&) = delete;
    MkoPluginManager& operator=(const MkoPluginManager&) = delete;

    struct LoadedPlugin
    {
        void* handle;
        const MkoPluginInterface* iface;
    };

    void loadPlugins();
    void loadPlugin(const std::string& path);
    void unloadPlugins();

    std::vector<LoadedPlugin> mPlugins;

    static MkoHostInterface sHostInterface;

    // Host service callbacks passed to each plugin.
    static const char* hostGetName(void);
    static void hostLog(MkoLogLevel level, const char* msg);
    static int hostSendMessage(const char* msg_name, const char* llsd_notation);
    static void* hostAppPtr(const char* name);
    static const char* hostGetPluginDir(void);
    static void hostShowNotification(const char* message);
    static void hostChat(const char* message, int chat_type);
    static int hostGetSetting(const char* name, char* out, size_t out_len);
    static int hostSetSetting(const char* name, const char* value);
    static int hostRegisterSetting(const char* name, const char* default_value,
                                   const char* label, const char* type);
    static int hostRegisterSettingsTab(const MkoSettingsTabDesc* tab);
    static int hostRegisterSetting2(const MkoSettingDesc2* setting);
    static const char* hostGetGraphicsInfo(void);
    static int hostRegisterShader(const MkoShaderDesc* desc);
    static int hostUnregisterShader(const char* name, MkoShaderType type);
    static int hostRegisterHtmlOverlay(const MkoHtmlOverlayDesc* desc);
    static int hostUnregisterHtmlOverlay(const char* id);
    static int hostShowHtmlOverlay(const char* id, int visible);
    static int hostNavigateHtmlOverlay(const char* id, const char* url);
    static int hostOpenScriptEditor(const char* id, const char* title,
                                    const char* language, const char* content);
    static int hostCloseScriptEditor(const char* id);
    static int hostGetAvatarHealth(void);
    static int hostGetAvatarHealthMax(void);
    static const char* hostGetRegionStats(void);
    static int hostHttpRequest(const char* url, const char* method,
                               const char* body, const char* msg_name);

    std::string getSettingsFilePath() const;
    void loadSettings();
    void saveSettings();

    // Latest SimStats values captured from the message dispatch path.
    void captureSimStats(const LLSD& message);

    std::map<std::string, std::string> mSettings;
    std::map<std::string, MkoSettingDef> mRegistry;
    std::vector<MkoSettingsTab> mTabs;
    mutable std::mutex mSettingsMutex;

    std::map<std::string, std::string> mShaderOverrides;
    mutable std::mutex mShaderMutex;

    std::map<S32, F32> mSimStats;
    mutable std::mutex mSimStatsMutex;
    std::string mRegionStatsCache; // serialized LLSD for the C API

    std::map<std::string, std::function<void(const LLSD&)>> mScriptSaveCallbacks;
    mutable std::mutex mScriptSaveMutex;
};

#endif /* MKO_PLUGIN_MANAGER_H */
