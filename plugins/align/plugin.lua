local plugin   = {}
plugin.name    = "Align Tokens"
plugin.version = "1.0"

local lines = {}

local function rtrim(s)
  local n = #s
  while n > 0 and s:find("^%s", n) do n = n - 1 end
  return s:sub(1, n)
end

local function get_line(i)
  if lines[i] then
    return lines[i]
  else
    line = rtrim(get_buffer_at_line(i))
    lines[i] = line
    return line
  end
end

local function find_position_of_farthest_token(pattern, first_line, last_line)

  last = -1
  for i = first_line, last_line, 1 do
    line = get_line(i)
    a, b = string.find(line, pattern)
    if b and b > last then last = b end
  end

  return last
end

local function align_tokens(first_line, last_line)

  last_space_after_string = "[^%s]+%s+"

  -- first col is the last space before the second non-space block
  -- for example, in " foo: bar" it would be the 6th char, just before the 'b'
  -- in something like " :foo  => 'bar'" it would be the 7th char, just before the '='
  first_col  = find_position_of_farthest_token(last_space_after_string, first_line, last_line)
  if first_col <= 0 then return first_col end

  for i = first_line, last_line, 1 do
    line = get_line(i)
    a, b = string.find(line, last_space_after_string)

    if a and b < first_col then
      offset = first_col - b
      insert_buffer_at_line(i, string.rep(" ", offset), b - 1)

      x, second_col = string.find(line, "[^%s]+%s+[^%s]+%s") -- first space after second block
      if second_col then -- let's find the position of the second block of text

        c, d = string.find(line, "[^%s]+%s+[^%s]+%s+") -- last space in second block of space
        if c and d > second_col + offset then
          diff = d - second_col
          delete_chars_at_line(i, second_col+offset, diff)
        end
      end
    end
  end

  for k in pairs(lines) do lines[k] = nil end

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
