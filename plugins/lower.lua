local M = {}

M.name  = "Lower"
M.ptype = "text"
M.pver  = "1.0"

function M.run(text)
  return tostring(text):lower()
end

function M.after_cmd_toggle_mouse_mode(text)
  eon_indent()
  return tostring(text):lower()
end

return M