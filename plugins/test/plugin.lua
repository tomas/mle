local plugin   = {}
plugin.name    = "Lower"
plugin.version = "1.0"

function plugin.after_cmd_toggle_mouse_mode(text)
  res = eon_indent()
  -- print("Result: %s\n", res)
  return tostring(text):lower()
end

return plugin