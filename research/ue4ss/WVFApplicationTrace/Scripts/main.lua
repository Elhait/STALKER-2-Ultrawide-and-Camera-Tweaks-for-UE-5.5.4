-- WVFApplicationTrace — bounded, read-only execution probe
--
-- This script registers exact lifecycle/Blueprint hooks. Blueprint UFunctions
-- are registered only after NotifyOnNewObject reports the WVF class instance,
-- because the first startup attempt ran before those functions were loaded.
-- It does not enumerate objects, poll, or write UObject properties.

local MODULE = "WVFApplicationTrace"
local installedHooks = {}

local function unwrap(value)
    if type(value) == "userdata" then
        local ok, inner = pcall(function() return value:get() end)
        if ok and inner ~= nil then return inner end
    end
    return value
end

local function safeText(value, fallback)
    local ok, result = pcall(function() return tostring(value) end)
    if ok and result ~= nil then return result end
    return fallback or "<unavailable>"
end

local function safeObjectText(object, method, fallback)
    object = unwrap(object)
    if object == nil then return fallback or "<nil>" end
    local ok, result = pcall(function()
        local member = object[method]
        if member == nil then return nil end
        local value = member(object)
        if value == nil then return nil end
        return value:GetFullName()
    end)
    if ok and result ~= nil then return safeText(result, fallback) end
    return fallback or "<unavailable>"
end

local function objectClass(object)
    object = unwrap(object)
    if object == nil then return "<nil>" end
    local ok, result = pcall(function()
        return object:GetClass():GetFullName()
    end)
    if ok and result ~= nil then return safeText(result, "<class-unavailable>") end
    return "<class-unavailable>"
end

local function callbackLog(label, self, ...)
    local argumentCount = select("#", ...)
    local firstArgument = "<none>"
    local firstArgumentValue = "<unavailable>"
    if argumentCount > 0 then
        firstArgumentValue = unwrap(select(1, ...))
        firstArgument = safeText(firstArgumentValue, "<unavailable>")
    end

    print(MODULE ..
        " event=" .. label ..
        " self=" .. safeObjectText(self, "GetFullName", "<unavailable>") ..
        " class=" .. objectClass(self) ..
        " owner=" .. safeObjectText(self, "GetOwner", "<unavailable>") ..
        " outer=" .. safeObjectText(self, "GetOuter", "<unavailable>") ..
        " argc=" .. safeText(argumentCount, "<unavailable>") ..
        " arg1=" .. firstArgument ..
        " entryPoint=" .. safeText(tonumber(firstArgumentValue), "<not-integer>") .. "\n")
end

local function register(label, path)
    if installedHooks[label] then return end

    local ok, errorText = pcall(function()
        RegisterHook(path, function(self, ...)
            callbackLog(label, self, ...)
        end)
    end)

    if ok then
        installedHooks[label] = true
        print(MODULE .. " registered label=" .. label .. " path=" .. path .. "\n")
    else
        print(MODULE .. " registration_failed label=" .. label ..
            " path=" .. path .. " error=" .. safeText(errorText, "<unavailable>") .. "\n")
    end
end

local function logNewObject(label, object)
    print(MODULE ..
        " object_created label=" .. label ..
        " object=" .. safeObjectText(object, "GetFullName", "<unavailable>") ..
        " class=" .. objectClass(object) ..
        " owner=" .. safeObjectText(object, "GetOwner", "<unavailable>") ..
        " outer=" .. safeObjectText(object, "GetOuter", "<unavailable>") .. "\n")
end

local function onWvfObject(label, object)
    logNewObject(label, object)

    if label == "WVF_C" then
        register("WVF_C.OnWorldBeginPlay", "/Weapon_Viewmodel_FOV/WVF.WVF_C:OnWorldBeginPlay")
        register("WVF_C.ExecuteUbergraph_WVF", "/Weapon_Viewmodel_FOV/WVF.WVF_C:ExecuteUbergraph_WVF")
    elseif label == "WVF_Actor_C" then
        register("WVF_Actor_C.ReceiveBeginPlay", "/Weapon_Viewmodel_FOV/WVF_Actor.WVF_Actor_C:ReceiveBeginPlay")
        register("WVF_Actor_C.ExecuteUbergraph_WVF_Actor", "/Weapon_Viewmodel_FOV/WVF_Actor.WVF_Actor_C:ExecuteUbergraph_WVF_Actor")
    end
end

-- Native ClientRestart is available at startup and anchors save/world reload.
register("ClientRestart", "/Script/Engine.PlayerController:ClientRestart")

-- Wait for the exact WVF instances; this is event-driven and does not poll.
local okBootstrap, errorBootstrap = pcall(function()
    NotifyOnNewObject("/Weapon_Viewmodel_FOV/WVF.WVF_C", function(object)
        onWvfObject("WVF_C", object)
    end)
    NotifyOnNewObject("/Weapon_Viewmodel_FOV/WVF_Actor.WVF_Actor_C", function(object)
        onWvfObject("WVF_Actor_C", object)
    end)
end)

if okBootstrap then
    print(MODULE .. " registered object-created bootstrap for WVF_C and WVF_Actor_C\n")
else
    print(MODULE .. " object-created bootstrap failed error=" ..
        safeText(errorBootstrap, "<unavailable>") .. "\n")
end

print(MODULE .. " loaded: late exact hooks; read-only; no polling; no property writes\n")
