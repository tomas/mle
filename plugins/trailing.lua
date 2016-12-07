local M = {}

function M.rtrim(s)
  local n = #s
  while n > 0 and s:find("^%s", n) do n = n - 1 end
  return s:sub(1, n)
end

function M.remove_trailing_lines()
  trim_count = 0
  line_count = get_line_count()
  -- print(string.format("lines: %d\n", line_count))

  for i=1, line_count, 1 do
    line = get_buffer_at_line(i-1)
    for str in string.gmatch(line, "([ ]+)$") do
      trim_count = trim_count+1
      trimmed = M.rtrim(line)
      -- len = string.len(line) - string.len(trimmed)
      -- print(string.format("line %d, trimmed %d", i, len))
      set_buffer_at_line(i-1, trimmed)
    end
  end

  return trim_count
end


M.name  = "Trailing Lines"
M.ptype = "text"
M.pver  = "1.0"

function M.before_cmd_save()
  res = M.remove_trailing_lines()
  return res
end

return M