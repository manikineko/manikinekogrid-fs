/**
 * @file mko_plugin_api.h
 * @brief Manikineko Online protocol plugin C API.
 *
 * A protocol-level plugin is a shared library (.so / .dll / .dylib) that
 * exports a single symbol: mko_get_interface(). The viewer loads the
 * library at startup, calls init(), then forwards every dispatched message
 * to on_message(). Returning a non-zero value from on_message() marks the
 * message as handled and skips the default viewer handler.
 *
 * The host interface provides basic services (logging, local message
 * dispatch, and future Lua access points).
 */

#ifndef MKO_PLUGIN_API_H
#define MKO_PLUGIN_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define MKO_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
  #define MKO_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
  #define MKO_PLUGIN_EXPORT
#endif

#define MKO_PLUGIN_API_VERSION 8

typedef enum
{
    MKO_LOG_DEBUG,
    MKO_LOG_INFO,
    MKO_LOG_WARN,
    MKO_LOG_ERROR
} MkoLogLevel;

typedef enum
{
    MKO_SHADER_VERTEX,
    MKO_SHADER_FRAGMENT,
    MKO_SHADER_GEOMETRY,
    MKO_SHADER_COUNT
} MkoShaderType;

typedef struct
{
    const char* name;            /* e.g. "deferred/diffuseV"                 */
    MkoShaderType type;          /* vertex / fragment / geometry             */
    const char* source;          /* full GLSL source for this stage          */
    const char* defines;         /* optional "#define FOO 1\n#define BAR 2"   */
} MkoShaderDesc;

/* A settings tab a plugin can add to the viewer's Preferences dialog
 * (installed under the Graphics preference panel). Tabs are keyed by a
 * stable id; the label is shown to the user. */
typedef struct
{
    const char* id;              /* stable tab id, e.g. "rtx"                 */
    const char* label;           /* display label, e.g. "RTX"                 */
} MkoSettingsTabDesc;

/* Richer setting descriptor. Extends the legacy 4-argument
 * register_setting() with tab placement, enum options and slider bounds.
 * Supported types: "boolean", "string", "float", "integer", "enum". */
typedef struct
{
    const char* name;            /* setting key, e.g. "MkoRtxQuality"         */
    const char* default_value;  /* persisted as a string                     */
    const char* label;           /* user-visible label                        */
    const char* type;            /* boolean | string | float | integer | enum */
    const char* tab_id;          /* settings tab id, NULL = default "plugins" */
    const char* options;         /* enum only: pipe-separated, e.g. "Low|High" */
    double min_value;            /* float/integer only: lower bound            */
    double max_value;            /* float/integer only: upper bound            */
} MkoSettingDesc2;

/* HTML overlay descriptor. A plugin can embed HTML content or a web
 * page in the viewer as a lightweight floater rendered by the viewer's
 * media (web) engine. Overlays are keyed by a stable id; re-registering
 * an existing id updates it in place.
 * If both url and html are given the url wins. */
typedef struct
{
    const char* id;              /* stable overlay id, e.g. "grid-news"        */
    const char* title;           /* floater title                             */
    const char* url;             /* page to load, or NULL                     */
    const char* html;            /* inline HTML to render, or NULL            */
    int x;                       /* left position in px, -1 = auto-center     */
    int y;                       /* top position in px, -1 = auto-center      */
    int width;                   /* overlay size in px                        */
    int height;                  /* overlay size in px                        */
    double opacity;              /* floater opacity 0..1, <=0 = default       */
    int visible;                 /* nonzero = show immediately               */
    int closable;                /* nonzero = user may close the floater      */
} MkoHtmlOverlayDesc;

/* Message contract (delivered to every plugin's on_message):
 *
 * MkoScriptEditorSaved { "id", "language", "content" }
 *     Sent when the user saves in the embedded script editor opened
 *     via open_script_editor().
 *
 * MkoHttpResult { "url", "status", "body" }
 *     Sent when an http_request() completes. "status" is the HTTP
 *     status code (0 on transport failure).
 *
 * MkoSettingChanged { "name", "value" }
 *     Sent when a plugin setting is changed (UI or API).
 *
 * MkoOverlayClosed { "id" }
 *     Sent when the user closes an HTML overlay floater. */

/* Forward declarations */
typedef struct MkoHostInterface MkoHostInterface;
typedef struct MkoPluginInterface MkoPluginInterface;

/* Services the viewer exposes to every plugin. */
struct MkoHostInterface
{
    int version;                       /* Set to MKO_PLUGIN_API_VERSION. */
    const char* (*get_name)(void);     /* Returns the viewer's display name. */
    void (*log)(MkoLogLevel level, const char* msg);
    int (*send_message)(const char* msg_name, const char* llsd_notation);
    void* (*app_ptr)(const char* name);
    const char* (*get_plugin_dir)(void); /* Absolute path to the plugin dir. */
    void (*show_notification)(const char* message);
    void (*chat)(const char* message, int chat_type);
    int (*get_setting)(const char* name, char* out, size_t out_len);
    int (*set_setting)(const char* name, const char* value);
    int (*register_setting)(const char* name, const char* default_value, const char* label, const char* type);
    int (*register_settings_tab)(const MkoSettingsTabDesc* tab);
    int (*register_setting2)(const MkoSettingDesc2* setting);
    const char* (*get_graphics_info)(void); /* LLSD notation: vendor/renderer/glsl_version */
    int (*register_shader)(const MkoShaderDesc* desc);
    int (*unregister_shader)(const char* name, MkoShaderType type);
    /* API v8: HTML overlay support */
    int (*register_html_overlay)(const MkoHtmlOverlayDesc* desc);
    int (*unregister_html_overlay)(const char* id);
    int (*show_html_overlay)(const char* id, int visible);
    int (*navigate_html_overlay)(const char* id, const char* url);
    /* API v8: embedded script editor */
    int (*open_script_editor)(const char* id, const char* title,
                              const char* language, const char* content);
    int (*close_script_editor)(const char* id);
    /* API v8: health & region stats */
    int (*get_avatar_health)(void);
    int (*get_avatar_health_max)(void);
    const char* (*get_region_stats)(void); /* LLSD notation */
    /* API v8: async HTTP; the result is delivered to every plugin as
     * message msg_name with an "MkoHttpResult" style payload. */
    int (*http_request)(const char* url, const char* method,
                        const char* body, const char* msg_name);
};

/* Plugin entry point. A plugin must export a function named
 * mko_get_interface with this exact signature. */
struct MkoPluginInterface
{
    int version;                       /* Set to MKO_PLUGIN_API_VERSION. */
    const char* name;
    const char* (*get_version)(void);
    int (*init)(const MkoHostInterface* host);
    void (*shutdown)(void);
    /* Return a non-zero value to mark the message as handled. */
    int (*on_message)(const char* msg_name, const char* llsd_notation);
};

typedef const MkoPluginInterface* (*MkoGetInterfaceFunc)(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MKO_PLUGIN_API_H */
