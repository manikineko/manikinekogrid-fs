/**
 * @file mkolua.cpp
 * @brief Optional Manikineko Lua plugin add-on.
 *
 * Build this as libmkolua.so and place it in <viewer-exe>/plugins.
 * It loads every *.lua file in the same directory, exposes a global
 * `mko` table, and lets scripts register an on_message callback.
 *
 * Requires a Lua development package (e.g. liblua5.4-dev).
 */

#include "mko_plugin_api.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <vector>
#include <string>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif

static const MkoHostInterface* sHost = nullptr;
static std::vector<lua_State*> sStates;

static const char* mkolua_get_version(void) { return "1.0"; }

// ------------------------------------------------------------------
// Lua-bound host services
// ------------------------------------------------------------------

static int l_log(lua_State* L)
{
    if (!sHost) return 0;
    MkoLogLevel level = (MkoLogLevel)luaL_checkinteger(L, 1);
    const char* msg = luaL_checkstring(L, 2);
    sHost->log(level, msg);
    return 0;
}

static int l_send_message(lua_State* L)
{
    if (!sHost)
    {
        lua_pushinteger(L, -1);
        return 1;
    }
    const char* name = luaL_checkstring(L, 1);
    const char* body = luaL_checkstring(L, 2);
    int r = sHost->send_message(name, body);
    lua_pushinteger(L, r);
    return 1;
}

static int l_get_name(lua_State* L)
{
    if (!sHost)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, sHost->get_name());
    return 1;
}

static int l_get_plugin_dir(lua_State* L)
{
    if (!sHost)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, sHost->get_plugin_dir());
    return 1;
}

static int l_show_notification(lua_State* L)
{
    if (!sHost) return 0;
    const char* msg = luaL_checkstring(L, 1);
    sHost->show_notification(msg);
    return 0;
}

static int l_chat(lua_State* L)
{
    if (!sHost) return 0;
    const char* msg = luaL_checkstring(L, 1);
    int chat_type = (int)luaL_optinteger(L, 2, 1);  /* 1 = normal */
    sHost->chat(msg, chat_type);
    return 0;
}

static int l_register_on_message(lua_State* L)
{
    if (!lua_isfunction(L, 1))
    {
        return luaL_error(L, "mko.register_on_message: expected a function");
    }

    lua_pushvalue(L, 1);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushnumber(L, ref);
    lua_setglobal(L, "__mko_on_message_ref");
    return 0;
}

static void register_mko_table(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_log);          lua_setfield(L, -2, "log");
    lua_pushcfunction(L, l_send_message); lua_setfield(L, -2, "send_message");
    lua_pushcfunction(L, l_get_name);     lua_setfield(L, -2, "get_name");
    lua_pushcfunction(L, l_get_plugin_dir); lua_setfield(L, -2, "get_plugin_dir");
    lua_pushcfunction(L, l_show_notification); lua_setfield(L, -2, "show_notification");
    lua_pushcfunction(L, l_chat);         lua_setfield(L, -2, "chat");
    lua_pushcfunction(L, l_register_on_message); lua_setfield(L, -2, "register_on_message");
    lua_setglobal(L, "mko");
}

// ------------------------------------------------------------------

static bool load_lua_script(lua_State* L, const std::string& path)
{
    if (luaL_loadfile(L, path.c_str()) != 0)
    {
        if (sHost) sHost->log(MKO_LOG_WARN, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != 0)
    {
        if (sHost) sHost->log(MKO_LOG_WARN, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

static void scan_and_load_scripts(const char* dir)
{
    if (!dir) return;

    std::string prefix = dir;
    if (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\')
    {
        prefix += '/';
    }

#if defined(_WIN32)
    // Windows implementation left as future exercise.
    (void)dir;
#else
    DIR* d = opendir(dir);
    if (!d)
    {
        if (sHost) sHost->log(MKO_LOG_INFO, "mkolua: no plugin directory; not loading scripts");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        const char* name = entry->d_name;
        size_t len = std::strlen(name);
        if (len > 4 && std::strcmp(name + len - 4, ".lua") == 0)
        {
            std::string path = prefix + name;
            lua_State* L = luaL_newstate();
            if (!L) continue;

            luaL_openlibs(L);
            register_mko_table(L);

            if (load_lua_script(L, path))
            {
                sStates.push_back(L);
                if (sHost)
                {
                    std::string msg = "mkolua: loaded " + path;
                    sHost->log(MKO_LOG_INFO, msg.c_str());
                }
            }
            else
            {
                lua_close(L);
            }
        }
    }
    closedir(d);
#endif
}

// ------------------------------------------------------------------
// MkoPluginInterface callbacks
// ------------------------------------------------------------------

static int mkolua_init(const MkoHostInterface* host)
{
    sHost = host;
    if (!sHost || sHost->version != MKO_PLUGIN_API_VERSION)
    {
        if (sHost) sHost->log(MKO_LOG_ERROR, "mkolua: host API version mismatch");
        return -1;
    }

    scan_and_load_scripts(sHost->get_plugin_dir());
    return 0;
}

static void mkolua_shutdown(void)
{
    for (lua_State* L : sStates)
    {
        lua_close(L);
    }
    sStates.clear();
    sHost = nullptr;
}

static int mkolua_on_message(const char* msg_name, const char* llsd_notation)
{
    if (!sHost || sStates.empty()) return 0;

    for (lua_State* L : sStates)
    {
        lua_getglobal(L, "__mko_on_message_ref");
        if (!lua_isnumber(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }

        int ref = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }

        lua_pushstring(L, msg_name);
        lua_pushstring(L, llsd_notation);

        if (lua_pcall(L, 2, 1, 0) == 0)
        {
            int handled = lua_toboolean(L, -1) ? 1 : 0;
            lua_pop(L, 1);
            if (handled) return 1;
        }
        else
        {
            if (sHost) sHost->log(MKO_LOG_WARN, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    return 0;
}

// ------------------------------------------------------------------

extern "C" MKO_PLUGIN_EXPORT const MkoPluginInterface* mko_get_interface(void)
{
    static MkoPluginInterface sIface = { 0 };
    if (sIface.version == 0)
    {
        sIface.version = MKO_PLUGIN_API_VERSION;
        sIface.name = "mkolua";
        sIface.get_version = mkolua_get_version;
        sIface.init = mkolua_init;
        sIface.shutdown = mkolua_shutdown;
        sIface.on_message = mkolua_on_message;
    }
    return &sIface;
}
