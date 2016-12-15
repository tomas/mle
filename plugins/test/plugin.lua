local plugin   = {}
plugin.name    = "Lower"
plugin.version = "1.0"

function plugin.after_cmd_toggle_mouse_mode(text)
  res = eon_indent()
  -- print("Result: %s\n", res)
  return tostring(text):lower()
end

function plugin.on_prompt_callback(action)
  print("prompt callback on lua!", action)
end

function plugin.start_review()
  goto_line(10)
  start_nav("String one .... ", plugin.on_prompt_callback)
end

function plugin.boot()
  --body = get_url("http://bootlog.org/foo")
  -- print(body)
  
  register_function("start_review")
  add_keybinding("M-j", "start_review")

  foo = get_option("name")
  -- print(foo, type(foo))

  boo = get_option("online")
  -- print(boo, type(boo))

  num = get_option("number")
  -- print(num, type(num))
end

return plugin