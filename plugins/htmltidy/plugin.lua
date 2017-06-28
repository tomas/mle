require('htmltidy')

local plugin = {}
plugin.name = "HTMLTidy"
plugin.version = "1.0"

local function tidy_html()
  -- filename = current_file_path()
  -- if not filename or string.len(filename) == 0 then
  --   return 0
  -- end

  buf = get_buffer()

  -- f = (io.open('test.html', 'r'))
  -- r = f:read("*all")
  -- f:close()

  local c = htmltidy.new()
  c:parse(buf)
  local x = c:toTable()
  -- this one prints the whole table. It's a great function
  -- don't miss out.
  if table.print then
    table.print(x)
  end

  c:setOpt(htmltidy.opt.XhtmlOut, true)
  local cleaned = c:saveToString()

  -- print(cleaned)
  set_buffer(cleaned)
end

plugin.boot = function()
  register_function("tidy_html")
  add_keybinding("CS-T", "tidy_html")
end

return plugin
