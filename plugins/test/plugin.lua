local plugin   = {}
plugin.name    = "Lower"
plugin.version = "1.0"

function plugin.after_cmd_toggle_mouse_mode(text)
  res = eon_indent()
  -- print("Result: %s\n", res)
  return tostring(text):lower()
end

function plugin.boot()
  --body = get_url("http://bootlog.org/foo")
  -- print(body)
  
  foo = get_option("name")
  -- print(foo, type(foo))

  boo = get_option("online")
  -- print(boo, type(boo))

  num = get_option("number")
  -- print(num, type(num))
end

return plugin