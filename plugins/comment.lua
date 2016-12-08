local plugin   = {}
plugin.name    = "Comment Lines"
plugin.version = "1.0"

local function toggle_comment_on(line_number)
  line = get_buffer_at_line(line_number)
  regex   = "// ?%s"
  comment = "// "
  column  = string.find(line, regex)

  if column then
    count = string.len(comment)
    delete_chars_at_line(line_number, column-1, count)
  else
    first = string.find(line, "[^ \t]")
    if not first or first < 2 then
      prepend_buffer_at_line(line_number, comment)
    else
      insert_buffer_at_line(line_number, comment, first - 1)
    end
  end
end

function plugin.onload()
  add_keybinding("C-/", "toggle_comment")
end

function plugin.before_cmd_grep()
  if has_selection() then
    selection = get_selection() -- start line, start col, end line, end col
    local first_line = selection[0]
    local last_line = selection[3] == 0 and selection[2]-1 or selection[2]
    for i = first_line, last_line, 1 do
      toggle_comment_on(i)
    end
  else
    toggle_comment_on(current_line_number())
  end

  return res
end

return plugin