local plugin = {}
plugin.name  = "Git Commit File Changes"
plugin.version = "1.0"

local git = {}
git.file_changed = function(filename)
  out = exec(string.format("git status | grep %s", filename))
end

git.add = function(filename)
  exec(string.format("git add %s", filename))
end

git.commit = function(msg)
  exec(string.format("git commit -m '%s'", msg))
end

plugin.commit_file = function()
  file = get_current_file()
  if git.file_changed(file.name) then
    default_msg = string.format("Update %s", file.name)
    commit_msg  = show_prompt("Commit message:", default_msg)
    if not commit_msg then -- cancelled
      return 0
    else
      git.add(file.name)
      return git.commit(commit_msg)
    end
  end
end

return plugin