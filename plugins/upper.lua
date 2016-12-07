-- local api = require('plugins/api')
local ffi = require('ffi')
ffi.cdef([[
  void printf(const char * fmt, ...);
]])

local M = {}

M.name  = "Upper"
M.ptype = "text"
M.pver  = "1.3"

function M.run(text)
  ffi.C.printf("integer value: %d\n", 10)
  test(1)
  return tostring(text):upper()
end

return M