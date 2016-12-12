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

local current_error = -1
local errors = []
local lines  = []

plugin.error_message = function(direction)
  current_error = current_error + direction
  reason = errors[current_error]
  prompt = string.format("[%d/%d errors]", number, errors.size(), reason)
  return prompt
end
  

plugin.on_prompt_input = function(key)
  if key == 'down' then
    prompt = error_message(+1)
  elseif key == 'up' then
    prompt = error_message(-1)
  else
    return "cancel_prompt"
  end

  return string.format("set_prompt:%s;goto_line:%d", prompt, lines[current_error])
end

plugin.check_current_file = function()
  filename = current_file_path()
  if not filename then return 0 end

  local command = "eslint"
  local config  = "eslintrc"
  local options = string.format('-c "%s%s"', script_path(), config)

  cmd = string.format('%s %s "%s"', command, options, filename)
  out = exec(cmd)

  -- title = string.format("ESLint: %s", filename)
  -- open_new_tab(title, out)
  
  count = parse_result(out)
  if count == 0
    show_message("No errors!")
  else
    goto_line(number)
    prompt = error_message(1)
    show_prompt(prompt, plugin.on_prompt_input)
  end
end

plugin.boot = function()
  register_function("check_current_file")
  add_keybinding("CS-L", "check_current_file")
  after("git.commit_changes", "check_current_file")
end

return plugin