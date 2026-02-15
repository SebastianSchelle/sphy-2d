local M = {}

dbg.logI("Init module {{{}}}", "engine")
dbg.logE("Error {}", 5)

function M.test(a)
    dbg.logW("Test function {}", a)
end

return M