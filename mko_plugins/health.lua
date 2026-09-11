-- Health system plugin for Manikineko Online.
--
-- Chat commands (type in nearby chat):
--   /health         open the Health & Region dashboard
--   /regionstats    print current region stats to nearby chat
--
-- The dashboard reads the server-driven avatar health (legacy "Health"
-- message, fully SL/OpenSim compatible) plus live region statistics.

local function open_health_floater()
    mko.send_message("MkoOpenFloater", "{'name':'mko_health'}")
end

local function handle_command(cmd)
    if cmd == "dashboard" then
        open_health_floater()
        return true
    end
    if cmd == "regionstats" then
        local health = mko.get_avatar_health() or 0
        local max = mko.get_avatar_health_max() or 100
        mko.chat("Health: " .. health .. " / " .. max, 1)
        local stats = mko.get_region_stats() or ""
        -- Pull a couple of well-known values out of the LLSD notation.
        local dilation = string.match(stats, "'time_dilation':r([%d%.%-]+)") or "?"
        local fps = string.match(stats, "'fps':r([%d%.%-]+)") or "?"
        local agents = string.match(stats, "'agents_main':i(%d+)") or "?"
        local objects = string.match(stats, "'objects':i(%d+)") or "?"
        mko.chat("Region: dilation " .. dilation .. ", sim FPS " .. fps ..
                 ", agents " .. agents .. ", objects " .. objects, 1)
        return true
    end
    return false
end

mko.register_on_message(function(msg_name, llsd_notation)
    if msg_name == "MkoHealthCommand" then
        local cmd = string.match(llsd_notation, "'command':'([^']*)'") or ""
        return handle_command(cmd)
    end
    return false
end)

mko.log(1, "[health] plugin loaded; /health for dashboard, /regionstats for chat stats")
