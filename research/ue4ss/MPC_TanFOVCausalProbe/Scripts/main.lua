---@diagnostic disable: undefined-global
-- Research-only Batch 5.2 probe.
-- F9 at the known post-EXIT weapon-present point performs exactly one
-- controlled MPC_FOV.TanFOV mutation and reads the active value back.
-- It intentionally does not restore the value: the visual result must be
-- observed after the confirmed intervention until the next reload.

local MODULE = "MPC_TanFOVCausalProbe"
local MPC_PATH = "/Game/_Stalker_2/Materials/MPC/MPC_FOV.MPC_FOV"
local KISMET_LIBRARY_PATH = "/Script/Engine.Default__KismetMaterialLibrary"
local PARAMETER = "TanFOV"
local MUTATION_FACTOR = 1.25
local ABSOLUTE_FALLBACK = 0.25

local interventionUsed = false

local function safe(fn, fallback)
    local ok, value = pcall(fn)
    if ok and value ~= nil then return value end
    return fallback
end

local function unwrap(value)
    if type(value) ~= "userdata" then return value end
    local inner
    local ok = pcall(function() inner = value:get() end)
    if ok and inner ~= nil then return inner end
    return value
end

local function text(value)
    value = unwrap(value)
    return tostring(value == nil and "<nil>" or value)
end

local function objectName(object)
    return text(safe(function() return object:GetFullName() end, "<unavailable>"))
end

local function objectClass(object)
    return text(safe(function() return object:GetClass():GetFullName() end, "<unavailable>"))
end

local function staticFind(path)
    local object = safe(function() return StaticFindObject(path) end, nil)
    return unwrap(object)
end

local function resolveParameterName()
    local helpers = rawget(_G, "UEHelpers")
    if helpers and type(helpers.FindOrAddFName) == "function" then
        local name = safe(function() return helpers.FindOrAddFName(PARAMETER) end, nil)
        if name ~= nil then return name end
    end
    return safe(function() return FName(PARAMETER) end, nil)
end

local function resolveContext()
    return unwrap(safe(function() return FindFirstOf("PlayerController") end, nil))
end

local function resolveWeaponIdentity()
    local character = unwrap(safe(function() return FindFirstOf("BP_Stalker2Character_C") end, nil))
    if character == nil then
        return "Character=<missing>\nWeaponMesh=<missing>\nAnimScriptInstance=<missing>"
    end

    local mesh = unwrap(safe(function() return character:GetWeaponInHandsMeshComponent() end, nil))
    if mesh == nil then
        return "Character=" .. objectName(character) ..
            "\nWeaponMesh=<missing>\nAnimScriptInstance=<missing>"
    end

    local anim = unwrap(safe(function() return mesh:GetAnimInstance() end, nil))
    return "Character=" .. objectName(character) ..
        "\nWeaponMesh=" .. objectName(mesh) ..
        "\nWeaponMeshClass=" .. objectClass(mesh) ..
        "\nAnimScriptInstance=" .. (anim == nil and "<missing>" or objectName(anim)) ..
        "\nAnimScriptInstanceClass=" .. (anim == nil and "<missing>" or objectClass(anim))
end

local function readScalar(context, library, collection, parameterName)
    local value = safe(function()
        return library:GetScalarParameterValue(context, collection, parameterName)
    end, nil)
    value = tonumber(unwrap(value))
    if value == nil or value ~= value then return nil end
    return value
end

local function runIntervention()
    print("[" .. MODULE .. "] BEGIN F9 TanFOV intervention\n")
    if interventionUsed then
        print("[" .. MODULE .. "] STOP: intervention already used in this process\n")
        return
    end
    interventionUsed = true

    print("[" .. MODULE .. "] Target=" .. MPC_PATH .. "\n")
    print("[" .. MODULE .. "] Parameter=" .. PARAMETER .. "\n")
    print("[" .. MODULE .. "] IdentityBefore:\n" .. resolveWeaponIdentity() .. "\n")

    local context = resolveContext()
    local library = staticFind(KISMET_LIBRARY_PATH)
    local collection = staticFind(MPC_PATH)
    local parameterName = resolveParameterName()
    if context == nil or library == nil or collection == nil or parameterName == nil then
        print("[" .. MODULE .. "] MutationState=UNAVAILABLE\n")
        print("[" .. MODULE .. "] context=" .. text(context) ..
            " library=" .. text(library) ..
            " collection=" .. text(collection) ..
            " parameterName=" .. text(parameterName) .. "\n")
        print("[" .. MODULE .. "] END F9 TanFOV intervention\n")
        return
    end

    local before = readScalar(context, library, collection, parameterName)
    print("[" .. MODULE .. "] TanFOV.Before=" .. text(before) .. "\n")
    if before == nil then
        print("[" .. MODULE .. "] MutationState=READ_BEFORE_FAILED\n")
        print("[" .. MODULE .. "] END F9 TanFOV intervention\n")
        return
    end

    local afterWrite = before * MUTATION_FACTOR
    if math.abs(afterWrite - before) < 0.000001 then
        afterWrite = before + ABSOLUTE_FALLBACK
    end
    print("[" .. MODULE .. "] TanFOV.Requested=" .. text(afterWrite) .. "\n")

    local writeOk, writeResult = pcall(function()
        return library:SetScalarParameterValue(context, collection, parameterName, afterWrite)
    end)
    if not writeOk then
        print("[" .. MODULE .. "] MutationState=WRITE_FAILED error=" .. text(writeResult) .. "\n")
        print("[" .. MODULE .. "] END F9 TanFOV intervention\n")
        return
    end

    local after = readScalar(context, library, collection, parameterName)
    print("[" .. MODULE .. "] TanFOV.After=" .. text(after) .. "\n")
    print("[" .. MODULE .. "] MutationState=" ..
        (after ~= nil and math.abs(after - afterWrite) < 0.000001
            and "CONFIRMED"
            or "READBACK_MISMATCH") .. "\n")
    print("[" .. MODULE .. "] IdentityAfter:\n" .. resolveWeaponIdentity() .. "\n")
    print("[" .. MODULE .. "] Observe framing now; do not press F9 again.\n")
    print("[" .. MODULE .. "] END F9 TanFOV intervention\n")
end

RegisterKeyBind(0x78, runIntervention) -- F9
print("[" .. MODULE .. "] loaded: F9=one TanFOV intervention; no polling; no restore\n")
