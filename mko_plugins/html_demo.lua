-- HTML embedding demo plugin for Manikineko Online.
--
-- Shows how a plugin can embed arbitrary HTML in the viewer using the
-- plugin API's HTML overlay support. Content is rendered by the viewer's
-- media (web) engine in a plugin-controlled floater.

local OVERLAY_HTML = [[
<html>
<head><style>
body { font-family: sans-serif; background: #1e1e2e; color: #cdd6f4;
       margin: 24px; }
h1 { color: #89b4fa; }
.panel { background: #313244; border-radius: 8px; padding: 12px 16px;
         margin: 8px 0; }
.ok { color: #a6e3a1; }
.note { color: #f9e2af; }
</style></head>
<body>
<h1>Manikineko Online</h1>
<div class="panel"><span class="ok">Grid status: online</span></div>
<div class="panel">This page is <b>embedded HTML</b> served by a Lua
plugin through the MKO plugin API - no viewer rebuild required.</div>
<div class="panel note">Plugins can register overlays with inline HTML
or remote URLs, and re-position or hide them at any time.</div>
</body>
</html>
]]

mko.register_on_message(function(msg_name, llsd_notation)
    if msg_name == "MkoHealthCommand" then
        local cmd = string.match(llsd_notation, "'command':'([^']*)'") or ""
        if cmd == "html" then
            mko.register_html_overlay({
                id = "html_demo",
                title = "Plugin HTML Demo",
                html = OVERLAY_HTML,
                width = 420,
                height = 320,
                visible = true,
                closable = true,
            })
            return true
        end
        return false
    end
    return false
end)

mko.log(1, "[html_demo] plugin loaded; /health html... coming via /health html")
