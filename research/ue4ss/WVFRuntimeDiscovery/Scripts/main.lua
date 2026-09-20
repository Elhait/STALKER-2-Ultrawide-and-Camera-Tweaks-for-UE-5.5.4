-- WVFRuntimeDiscovery — bounded, read-only reference-mod probe
--
-- This script intentionally searches only exact candidate class names. It does
-- not recursively enumerate UObject properties and does not write values.

local MODULE = "WVFRuntimeDiscovery"

local function valid(object)
    if object == nil then return false end
    local ok, result = pcall(function() return object:IsValid() end)
    return ok and result == true
end

local function safe(fn, fallback)
    local ok, result = pcall(fn)
    if not ok or result == nil then return fallback end
    return result
end

local function text(value)
    return tostring(value == nil and "<nil>" or value)
end

local function objectName(object)
    if not valid(object) then return "<invalid>" end
    return text(safe(function() return object:GetFullName() end, "<unnamed>"))
end

local function className(object)
    if not valid(object) then return "<invalid>" end
    return text(safe(function() return object:GetClass():GetFullName() end, "<class-unavailable>"))
end

local function optionalCall(object, method)
    if not valid(object) then return "<invalid>" end
    return text(safe(function() return object[method](object):GetFullName() end, "<unavailable>"))
end

local function dumpCandidate(classNameToFind)
    local objects = safe(function() return FindAllOf(classNameToFind) end, nil)
    if objects == nil then
        print(MODULE .. " " .. classNameToFind .. ": FindAllOf failed\n")
        return
    end
    local count = 0
    for _, object in ipairs(objects) do
        if valid(object) then
            count = count + 1
            print(MODULE .. " match class=" .. classNameToFind ..
                " object=" .. objectName(object) ..
                " reflectedClass=" .. className(object) ..
                " owner=" .. optionalCall(object, "GetOwner") ..
                " outer=" .. optionalCall(object, "GetOuter") .. "\n")
        end
    end
    print(MODULE .. " summary class=" .. classNameToFind .. " liveMatches=" .. count .. "\n")
end

local function runDiscovery()
    print(MODULE .. " BEGIN manual F9 exact WVF discovery; read-only; no property writes\n")
    dumpCandidate("WVF_Actor_C")
    dumpCandidate("WVF_C")
    print(MODULE .. " END manual F9 exact WVF discovery\n")
end

-- F9 is also the S3 weapon-visible snapshot in AnimScriptInstanceS0S4Capture.
-- Keep this probe manual and aligned with that observation point; do not run
-- during InitMap and do not consume any key below F7.
RegisterKeyBind(0x78, function()
    ExecuteInGameThread(runDiscovery)
end) -- F9

print(MODULE .. " loaded: F9=manual exact WVF discovery; no init-time scan\n")
