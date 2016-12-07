local M = {}

M.name  = "Lower"
M.ptype = "text"
M.pver  = "1.0"

function M.run(text)
  return tostring(text):lower()
end

return M