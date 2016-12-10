local plugin   = {}
plugin.name    = "Remove Trailing Spaces on Save"
plugin.version = "1.0"

function plugin.remove_trailing_spaces()
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

function plugin.boot()
  -- register_function("remove_trailing_spaces")
  before("save", "remove_trailing_spaces")
  -- after("git.commit_changes", "remove_trailing_spaces")
end

return plugin