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
    if not first then -- empty line, continue
      return false
    elseif first < 2 then
      prepend_buffer_at_line(line_number, comment)
    else
      insert_buffer_at_line(line_number, comment, first - 1)
    end
  end
end

function plugin.toggle()
  lines = 0

  if has_selection() then
    selection = get_selection() -- start line, start col, end line, end col
    local first_line = selection[0]
    local last_line = selection[3] == 0 and selection[2]-1 or selection[2]
    for i = first_line, last_line, 1 do
      lines = lines + 1
      toggle_comment_on(i)
    end
  else
    toggle_comment_on(current_line_number())
    lines = 1
  end

  return lines
end

function plugin.boot()
  register_function("toggle")
  add_keybinding("C-/", "toggle")
  before("save", "toggle")
end

return plugin