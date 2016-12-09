-- local api = require('plugins/api')
local ffi = require('ffi')
ffi.cdef([[
  void printf(const char * fmt, ...);
]])

local plugin   = {}
plugin.name    = "Upper"
plugin.version = "1.3"

function plugin.run(text)
  ffi.C.printf("integer value: %d\n", 10)
  test(1)
  return tostring(text):upper()
end

return plugin