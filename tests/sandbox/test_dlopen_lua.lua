-- Test: Lua tries to bypass sandbox via require (C module loading)
-- and os.execute (shell command)

-- Normal env check
local home = os.getenv("HOME")
print("  " .. (home and "[OK     ] HOME: " .. home or "[BLOCKED] HOME"))

local secret = os.getenv("AWS_SECRET_KEY")
print("  " .. (secret and "[OK     ] AWS_SECRET_KEY: " .. secret or "[BLOCKED] AWS_SECRET_KEY"))

-- Try to load a C module via require (calls dlopen)
local ok, err = pcall(function()
    -- Try loading the 'io' C module directly — already loaded, but
    -- a custom .so would be blocked by dlopen interception
    local ffi_ok, ffi = pcall(require, "ffi")
    if ffi_ok then
        print("  [OK     ] require('ffi'): loaded (ESCAPED!)")
    else
        print("  [BLOCKED] require('ffi'): " .. tostring(ffi))
    end
end)

-- Try os.execute (goes through execve)
local ok2, err2 = pcall(function()
    local result = os.execute("whoami")
    if result then
        print("  [OK     ] os.execute('whoami'): ran")
    else
        print("  [BLOCKED] os.execute('whoami')")
    end
end)
if not ok2 then
    print("  [BLOCKED] os.execute: " .. tostring(err2))
end

-- Try io.popen
local ok3, err3 = pcall(function()
    local f = io.popen("id")
    if f then
        local out = f:read("*a")
        f:close()
        if out and #out > 0 then
            print("  [OK     ] io.popen('id'): " .. out:gsub("\n",""))
        else
            print("  [BLOCKED] io.popen('id'): empty")
        end
    else
        print("  [BLOCKED] io.popen('id')")
    end
end)
if not ok3 then
    print("  [BLOCKED] io.popen: " .. tostring(err3))
end
