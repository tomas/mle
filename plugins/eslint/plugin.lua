local plugin = {}
plugin.name = "ESlint Wrapper"
plugin.version = "1.0"

local function script_path()
  return debug.getinfo(1).source:match("@?(.*/)")
end

local function exec(cmd)
  local handle = io.popen(string.format("%s 2>&1", cmd))
  local result = handle:read("*a")
  trimmed, idx = result:gsub("%s+$", "")
  handle:close()
  return trimmed
end

local current_error = -1
local error_count = 0
local errors = {}
local lines  = {}

local function parse_result(output)
  count = 0

  for x, y, type, err in string.gmatch(output, "(%d+):(%d+)%s+(%w+)%s+([^\n]+)") do
    count = count + 1
    lines[count]  = x
    errors[count] = err
  end

  error_count = count
  return count
end

--[[
local function reset_lines()
  line_count = get_line_count()
  for i = 1, line_count, 1 do
    set_line_bg_color(i, 0)
  end
end
]]--

local function paint_errors(color_number)
  for i = 1, error_count, 1 do
    line_number = lines[i]
    -- if color_number == 0 then print("----------", line_number, color_number) end
    set_line_bg_color(line_number-1, color_number)
  end
end

local function clean_errors()
  paint_errors(0)
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

plugin.error_message = function(direction)
  current_error = current_error + direction
  reason = errors[current_error]
  prompt = string.format("[%d/%d errors]", current_error, errors.size(), reason)
  return prompt
end

plugin.on_prompt_input = function(key)
  if key == 'down' then
    prompt = error_message(1)
  elseif key == 'up' then
    prompt = error_message(-1)
  else
    return "cancel_prompt"
  end

  return string.format("set_prompt:%s;goto_line:%d", prompt, lines[current_error])
end

plugin.check_current_file = function()
  filename = current_file_path()
  if not filename or string.len(filename) == 0 then
    return 0
  end

  -- print("Checking file", filename)
  local command = "eslint"
  local config  = "eslintrc"
  local options = string.format('-c "%s%s" 2>&1', script_path(), config)

  cmd = string.format('%s %s "%s"', command, options, filename)
  out = exec(cmd)

  if string.find(out, "not found") then
    open_horizontal_pane(3, "Hello hello hello")
    return 0
  end

  -- title = string.format("ESLint: %s", filename)
  -- open_new_tab(title, out)

  if error_count > 0 then clean_errors() end

  parse_result(out)
  if error_count == 0 then
    show_message("No errors!")
  else
    -- print(error_count, "errors found.")
    paint_errors(2)
    goto_line(number)
    prompt = error_message(1)
    show_prompt(prompt, plugin.on_prompt_input)
  end
end

plugin.boot = function()
  register_function("check_current_file")
  add_keybinding("CS-L", "check_current_file")
  before("grep", "check_current_file")
end

return plugin