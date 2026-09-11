-- VS Code for Web (Monaco) script editor demo plugin for Manikineko Online.
--
-- The viewer now opens the embedded Monaco editor automatically when an
-- LSL/OSSL script is loaded. This plugin listens for script editor events
-- and can extend or log them.

local function llsd_get_string(llsd_notation, key)
    -- Extract a single-quoted string value for 'key' from LLSD notation,
    -- handling \' and \\ escapes. Keys and values are assumed to be simple
    -- quoted strings as emitted by the viewer's LLSD notation serializer.
    local prefix = "'" .. key .. "':'"
    local start = string.find(llsd_notation, prefix, 1, true)
    if not start then return "" end

    local i = start + string.len(prefix)
    local out = {}
    local s = llsd_notation
    local len = string.len(s)

    while i <= len do
        local c = string.sub(s, i, i)
        if c == "\\" then
            i = i + 1
            local n = string.sub(s, i, i)
            if n == "'" then
                table.insert(out, "'")
            elseif n == "\\" then
                table.insert(out, "\\")
            elseif n == "n" then
                table.insert(out, "\n")
            elseif n == "r" then
                table.insert(out, "\r")
            elseif n == "t" then
                table.insert(out, "\t")
            elseif n == "x" then
                local hex = string.sub(s, i + 1, i + 2)
                local byte = tonumber(hex, 16)
                if byte then
                    table.insert(out, string.char(byte))
                    i = i + 2
                else
                    table.insert(out, "\\x")
                end
            else
                -- Unknown escape, keep as-is.
                table.insert(out, "\\" .. n)
            end
        elseif c == "'" then
            break
        else
            table.insert(out, c)
        end
        i = i + 1
    end

    return table.concat(out)
end

mko.register_on_message(function(msg_name, llsd_notation)
    if msg_name == "MkoScriptEditorOpen" then
        local id = llsd_get_string(llsd_notation, "id")
        local title = llsd_get_string(llsd_notation, "title")
        local language = llsd_get_string(llsd_notation, "language")

        mko.log(1, "[vscode_editor] script opened: '" .. title .. "' (" .. language .. ")")
        -- The viewer opens the Monaco editor automatically; plugins can also
        -- call mko.open_script_editor(id, title, language, content) if desired.
        return false
    end

    if msg_name == "MkoScriptEditorSaved" then
        local id = llsd_get_string(llsd_notation, "id")
        local language = llsd_get_string(llsd_notation, "language")
        local content = llsd_get_string(llsd_notation, "content")

        mko.log(1, "[vscode_editor] script '" .. id .. "' saved (" ..
                 tostring(string.len(content)) .. " chars, language=" .. language .. ")")
        mko.show_notification("VS Code editor saved '" .. id .. "'.")
        return false
    end

    return false
end)

mko.log(1, "[vscode_editor] plugin loaded")
