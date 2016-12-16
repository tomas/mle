local plugin   = {}
plugin.name    = "Align Tokens"
plugin.version = "1.0"

local function find_position_of_farthest_token(first_line, last_line)

  last = -1
  for i = first_line, last_line, 1 do
    line  = get_buffer_at_line(i)
    a, b = string.find(line, "[^%s]+%s*")
    if b and b > last then last = b end
  end

  return last
end

local function align_tokens(first_line, last_line)

  col = find_position_of_farthest_token(first_line, last_line)
  if col <= 0 then return col end
  
  for i = first_line, last_line, 1 do
    line = get_buffer_at_line(i)
    a, b = string.find(line, "[^%s]+%s*")
    if a and b < col then
      offset = col - b
      insert_buffer_at_line(i, string.rep(" ", offset), b - 1)
    end
  end
end

function plugin.toggle()
  if has_selection() then
    selection = get_selection() -- start line, start col, end line, end col
    local first_line = selection[0]
    local last_line  = selection[2]
    return align_tokens(first_line, last_line)
  end
end

function plugin.boot()
  register_function("toggle")
  add_keybinding("CM-a", "toggle")
end

return plugin
