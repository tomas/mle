local plugin = {}
plugin.name = "ESlint Wrapper"
plugin.version = "1.0"

local function script_path()
  return debug.getinfo(1).source:match("@?(.*/)")
end

local function exec(cmd)
  local handle = io.popen(cmd)
  local result = handle:read("*a")
  trimmed, idx = result:gsub("%s+$", "")
  handle:close()
  return trimmed
end

--[[
plugin.on_item_select = function(tab_id, line)
  x, y = line:match("^%s*(%d+):(%d+)%s")

  title    = get_tab_title(tab_id)
  filename = line:match("ESLint: (.+)")
  tab_id   = get_last_tab_with_title(filename)

  if tab_id then move_cursor_to_position(tag_id, x, y) end
end
]]--

plugin.run_eslint = function()
  filename = current_file_path()
  -- filename = '/home/tomas/code/packages/js/needle/lib/needle.js'
  if not filename then return 0 end

  local command = "eslint"
  local config  = "eslintrc"
  local options = string.format('-c "%s%s"', script_path(), config)

  cmd   = string.format('%s %s "%s"', command, options, filename)
  -- print("running", cmd)
  out   = exec(cmd)
  title = string.format("ESLint: %s", filename)
  -- print(out)
  open_new_tab(title, out)
end

plugin.on_boot = function()
  add_keybinding("CS-L", "run_eslint")
end

plugin.before_cmd_search = function()
  plugin.run_eslint()
end

return plugin