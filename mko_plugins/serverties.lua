-- Server-ties plugin for Manikineko Online.
--
-- Connects the viewer to Manikineko grid services using the plugin API's
-- async HTTP support. On login it fetches the grid's client configuration
-- (a small LLSD document) and applies the server-driven options it is
-- allowed to touch:
--
--   health_max      -> MkoHealthMax plugin setting (health bar scale)
--   news_url        -> shown as an HTML overlay (grid news page)
--   suggest_pack    -> forwarded to the RTX plugin as a suggestion
--
-- Everything is delivered over plain HTTP(S) from your grid services,
-- so the viewer core stays fully OpenSim / Second Life compatible.

local CONFIG_FETCHED = false

local function apply_server_config(config)
    -- health_max: server-side health system option
    local health_max = string.match(config, "'health_max':i(%d+)")
    if health_max then
        mko.set_setting("MkoHealthMax", health_max)
        mko.log(1, "[serverties] server set health max to " .. health_max)
    end

    -- news_url: grid news page as an HTML overlay
    local news_url = string.match(config, "'news_url':'([^']*)'")
    if news_url and news_url ~= "" then
        mko.register_html_overlay({
            id = "grid_news",
            title = "Manikineko Online",
            url = news_url,
            width = 420,
            height = 300,
            visible = false,
            closable = true,
        })
        mko.show_notification("Grid news is available. Run /news to open it.")
    end

    -- suggest_pack: recommended shader pack for this grid
    local pack = string.match(config, "'suggest_pack':'([^']*)'")
    if pack and pack ~= "" then
        local msg = "{'pack':'" .. pack .. "','forced':b0}"
        mko.send_message("MkoServerShaderPolicy", msg)
    end
end

mko.register_on_message(function(msg_name, llsd_notation)
    if msg_name == "AgentStateUpdate" and not CONFIG_FETCHED then
        CONFIG_FETCHED = true
        -- The grid services URL is configurable; point this at your
        -- Robust/server endpoint that serves the client config.
        local grid_cfg_url = mko.get_setting("MkoServerConfigURL", "")
        if grid_cfg_url ~= "" then
            mko.http_request(grid_cfg_url, "GET", nil, "MkoServerConfigResult")
        end
        return false
    end

    if msg_name == "MkoServerConfigResult" then
        local status = string.match(llsd_notation, "'status':i(%d+)") or "0"
        local body = string.match(llsd_notation, "'body_notation':'(.-)'") or ""
        if status == "200" and body ~= "" then
            apply_server_config(body)
        else
            mko.log(2, "[serverties] failed to fetch server config (status " ..
                     tostring(status) .. ")")
        end
        return true
    end

    if msg_name == "MkoHealthCommand" then
        local cmd = string.match(llsd_notation, "'command':'([^']*)'") or ""
        if cmd == "news" then
            mko.show_html_overlay("grid_news", true)
            return true
        end
        return false
    end

    return false
end)

mko.register_setting("MkoServerConfigURL", "", "Grid client-config URL (server ties)", "string")

mko.log(1, "[serverties] plugin loaded")
