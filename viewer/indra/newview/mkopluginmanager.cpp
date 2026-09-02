/**
 * @file mkopluginmanager.cpp
 * @brief Shared-library plugin loader and protocol hook callbacks.
 */

#include "llviewerprecompiledheaders.h"

#include "mkopluginmanager.h"
#include "mko_plugin_api.h"

#include "message.h"
#include "lldir.h"
#include "llsdserialize.h"
#include "llnotificationsutil.h"
#include "fsnearbychathub.h"
#include "llchat.h"

#include <sstream>

#if LL_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif

MkoHostInterface MkoPluginManager::sHostInterface = { 0 };

MkoPluginManager& MkoPluginManager::instance()
{
    static MkoPluginManager inst;
    return inst;
}

MkoPluginManager::~MkoPluginManager()
{
    unloadPlugins();
}

void MkoPluginManager::init()
{
    sHostInterface.version = MKO_PLUGIN_API_VERSION;
    sHostInterface.get_name = &MkoPluginManager::hostGetName;
    sHostInterface.log = &MkoPluginManager::hostLog;
    sHostInterface.send_message = &MkoPluginManager::hostSendMessage;
    sHostInterface.app_ptr = &MkoPluginManager::hostAppPtr;
    sHostInterface.get_plugin_dir = &MkoPluginManager::hostGetPluginDir;
    sHostInterface.show_notification = &MkoPluginManager::hostShowNotification;
    sHostInterface.chat = &MkoPluginManager::hostChat;

    // Register this manager as a protocol dispatch interceptor.
    LLMessageSystem::setDispatchInterceptor(&MkoPluginManager::dispatchMessage);

    loadPlugins();
}

void MkoPluginManager::shutdown()
{
    LLMessageSystem::setDispatchInterceptor(nullptr);
    unloadPlugins();
}

bool MkoPluginManager::dispatchMessage(const std::string& msg_name,
                                       const LLSD& message,
                                       LLHTTPNode::ResponsePtr responsep)
{
    MkoPluginManager& self = instance();
    if (self.mPlugins.empty())
    {
        return false;
    }

    // Convert the incoming LLSD to a notation string for C plugins.
    std::ostringstream ostr;
    LLSDSerialize::serialize(message, ostr, LLSDSerialize::LLSD_NOTATION);
    const std::string payload = ostr.str();
    const char* payload_cstr = payload.c_str();
    const char* msg_cstr = msg_name.c_str();

    for (LoadedPlugin& p : self.mPlugins)
    {
        if (!p.iface || !p.iface->on_message)
        {
            continue;
        }

        if (p.iface->on_message(msg_cstr, payload_cstr) != 0)
        {
            return true;
        }
    }

    return false;
}

void MkoPluginManager::loadPlugins()
{
    std::string plugins_dir = gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "plugins");
    if (!gDirUtilp->fileExists(plugins_dir))
    {
        LL_INFOS("MkoPlugin") << "Plugin directory not found: " << plugins_dir << LL_ENDL;
        return;
    }

    std::vector<std::string> files = gDirUtilp->getFilesInDir(plugins_dir);
    for (const std::string& f : files)
    {
        std::string::size_type dot = f.find_last_of('.');
        if (dot == std::string::npos)
        {
            continue;
        }

        std::string ext = f.substr(dot + 1);
        if (ext == "so" || ext == "dll" || ext == "dylib")
        {
            loadPlugin(gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "plugins", f));
        }
    }
}

void MkoPluginManager::loadPlugin(const std::string& path)
{
    void* handle = nullptr;

#if LL_WINDOWS
    HMODULE hmod = LoadLibraryA(path.c_str());
    handle = reinterpret_cast<void*>(hmod);
#else
    handle = dlopen(path.c_str(), RTLD_NOW);
#endif

    if (!handle)
    {
        const char* err = "unknown";
#if !LL_WINDOWS
        err = dlerror();
#endif
        LL_WARNS("MkoPlugin") << "Failed to load plugin " << path << ": " << err << LL_ENDL;
        return;
    }

    MkoGetInterfaceFunc get_iface = nullptr;
#if LL_WINDOWS
    get_iface = reinterpret_cast<MkoGetInterfaceFunc>(
        GetProcAddress(reinterpret_cast<HMODULE>(handle), "mko_get_interface"));
#else
    get_iface = reinterpret_cast<MkoGetInterfaceFunc>(dlsym(handle, "mko_get_interface"));
#endif

    if (!get_iface)
    {
        LL_WARNS("MkoPlugin") << "Plugin " << path << " has no mko_get_interface" << LL_ENDL;
#if !LL_WINDOWS
        dlclose(handle);
#else
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
#endif
        return;
    }

    const MkoPluginInterface* iface = get_iface();
    if (!iface
        || iface->version != MKO_PLUGIN_API_VERSION
        || !iface->init
        || !iface->on_message)
    {
        LL_WARNS("MkoPlugin") << "Plugin " << path << " has an incompatible interface" << LL_ENDL;
#if !LL_WINDOWS
        dlclose(handle);
#else
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
#endif
        return;
    }

    if (iface->init(&sHostInterface) != 0)
    {
        LL_WARNS("MkoPlugin") << "Plugin " << path << " init failed" << LL_ENDL;
#if !LL_WINDOWS
        dlclose(handle);
#else
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
#endif
        return;
    }

    mPlugins.push_back({ handle, iface });
    LL_INFOS("MkoPlugin") << "Loaded plugin "
                          << (iface->name ? iface->name : path)
                          << LL_ENDL;
}

void MkoPluginManager::unloadPlugins()
{
    for (LoadedPlugin& p : mPlugins)
    {
        if (p.iface && p.iface->shutdown)
        {
            p.iface->shutdown();
        }

#if !LL_WINDOWS
        dlclose(p.handle);
#else
        FreeLibrary(reinterpret_cast<HMODULE>(p.handle));
#endif
    }

    mPlugins.clear();
}

const char* MkoPluginManager::hostGetName(void)
{
    return "Manikineko Online";
}

void MkoPluginManager::hostLog(MkoLogLevel level, const char* msg)
{
    if (!msg)
    {
        return;
    }

    switch (level)
    {
        case MKO_LOG_DEBUG:
            LL_DEBUGS("MkoPlugin") << msg << LL_ENDL;
            break;
        case MKO_LOG_WARN:
            LL_WARNS("MkoPlugin") << msg << LL_ENDL;
            break;
        case MKO_LOG_ERROR:
            LL_ERRS("MkoPlugin") << msg << LL_ENDL;
            break;
        case MKO_LOG_INFO:
        default:
            LL_INFOS("MkoPlugin") << msg << LL_ENDL;
            break;
    }
}

int MkoPluginManager::hostSendMessage(const char* msg_name, const char* llsd_notation)
{
    if (!msg_name || !llsd_notation)
    {
        return -1;
    }

    if (!gMessageSystem)
    {
        LL_WARNS("MkoPlugin") << "send_message called before message system is ready" << LL_ENDL;
        return -1;
    }

    std::istringstream istr(llsd_notation);
    LLSD message;
    if (!LLSDSerialize::deserialize(message, istr, LLSDSerialize::SIZE_UNLIMITED))
    {
        LL_WARNS("MkoPlugin") << "Invalid LLSD from plugin for " << msg_name << LL_ENDL;
        return -1;
    }

    LLMessageSystem::dispatch(std::string(msg_name), message);
    return 0;
}

void* MkoPluginManager::hostAppPtr(const char* name)
{
    if (!name)
    {
        return nullptr;
    }

    if (std::string(name) == "message_system")
    {
        return static_cast<void*>(gMessageSystem);
    }

    return nullptr;
}

const char* MkoPluginManager::hostGetPluginDir(void)
{
    static std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "plugins");
    return dir.c_str();
}

void MkoPluginManager::hostShowNotification(const char* message)
{
    if (!message) return;
    LLSD args;
    args["MESSAGE"] = std::string(message);
    LLNotificationsUtil::add("GenericAlert", args);
}

void MkoPluginManager::hostChat(const char* message, int chat_type)
{
    if (!message) return;
    EChatType type = CHAT_TYPE_NORMAL;
    if (chat_type == 0) type = CHAT_TYPE_WHISPER;
    else if (chat_type == 2) type = CHAT_TYPE_SHOUT;
    FSNearbyChat::instance().sendChatFromViewer(std::string(message), type, false);
}
