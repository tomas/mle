local M = {}

M.name = "Trailing Lines"
M.pver = "1.0"

local function remove_trailing_lines()
  trim_count = 0
  line_count = get_line_count()

  for i = 0, line_count-1, 1 do
    line = get_buffer_at_line(i)
    col = string.find(line, "([ \t]+)$")
    if col then
      trim_count = trim_count+1
      delete_chars_at_line(i, col-1)
    end
  end

  return trim_count
end

function M.before_cmd_save()
  res = remove_trailing_lines()
  return res
end

return M