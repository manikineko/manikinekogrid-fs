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

#include "llsd.h"
#include "llhttpnode.h"
#include "mko_plugin_api.h"

class MkoPluginManager
{
public:
    static MkoPluginManager& instance();

    void init();
    void shutdown();

    static bool dispatchMessage(const std::string& msg_name,
                                const LLSD& message,
                                LLHTTPNode::ResponsePtr responsep);

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
};

#endif /* MKO_PLUGIN_MANAGER_H */
