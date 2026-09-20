-- Research-only S0-S4 capture. Read-only; no hooks or property writes.

local MODULE = "AnimScriptInstanceS0S4Capture"

local function safe(fn, fallback)
    local ok, value = pcall(fn)
    if ok and value ~= nil then return value end
    return fallback
end

local function text(value)
    return tostring(value == nil and "<nil>" or value)
end

local function objectName(object)
    return text(safe(function() return object:GetFullName() end, "<unavailable>"))
end

local function objectClass(object)
    return text(safe(function() return object:GetClass():GetFullName() end, "<unavailable>"))
end

local function read(object, field)
    return text(safe(function() return object[field] end, "<unavailable>"))
end

local function capture(label)
    -- S1 is the known weapon-absent cinematic state. Do not call native mesh
    -- accessors here: pcall cannot catch an engine-side access violation.
    if label == "S1_CINEMATIC" or label == "S2_POST_EXIT_NO_WEAPON" then
        print("[" .. MODULE .. "] State=" .. label .. "\n")
        print("[" .. MODULE .. "] WeaponMesh: NOT PRESENT\n")
        print("[" .. MODULE .. "] AnimScriptInstance: NOT PRESENT\n")
        return
    end

    local character = safe(function() return FindFirstOf("BP_Stalker2Character_C") end, nil)
    print("[" .. MODULE .. "] State=" .. label .. "\n")

    if character == nil then
        print("[" .. MODULE .. "] Character: NOT PRESENT\n")
        return
    end

    local mesh = safe(function() return character:GetWeaponInHandsMeshComponent() end, nil)
    if mesh == nil then
        print("[" .. MODULE .. "] WeaponMesh: NOT PRESENT\n")
        print("[" .. MODULE .. "] AnimScriptInstance: NOT PRESENT\n")
        return
    end

    print("[" .. MODULE .. "] WeaponMesh: Present=true\n")
    print("[" .. MODULE .. "] WeaponMesh.FullName=" .. objectName(mesh) .. "\n")
    print("[" .. MODULE .. "] WeaponMesh.Class=" .. objectClass(mesh) .. "\n")

    local anim = safe(function() return mesh:GetAnimInstance() end, nil)
    if anim == nil then
        print("[" .. MODULE .. "] AnimScriptInstance: NOT PRESENT\n")
        return
    end

    print("[" .. MODULE .. "] AnimScriptInstance: Present=true\n")
    print("[" .. MODULE .. "] AnimScriptInstance.FullName=" .. objectName(anim) .. "\n")
    print("[" .. MODULE .. "] AnimScriptInstance.Class=" .. objectClass(anim) .. "\n")
    local aimingData = safe(function() return anim.AimingData end, nil)
    print("[" .. MODULE .. "] AimingData=" .. text(aimingData) .. "\n")
    if aimingData ~= nil then
        print("[" .. MODULE .. "] AimingData.bAiming=" .. read(aimingData, "bAiming") .. "\n")
        print("[" .. MODULE .. "] AimingData.AimAlpha=" .. read(aimingData, "AimAlpha") .. "\n")
        print("[" .. MODULE .. "] AimingData.OffsetAimAlpha=" .. read(aimingData, "OffsetAimAlpha") .. "\n")
        print("[" .. MODULE .. "] AimingData.AimState=" .. read(aimingData, "AimState") .. "\n")
    end
end

RegisterKeyBind(0x76, function() capture("S1_CINEMATIC") end) -- F7
RegisterKeyBind(0x77, function() capture("S2_POST_EXIT_NO_WEAPON") end) -- F8
RegisterKeyBind(0x78, function() capture("S3_POST_EXIT_WEAPON") end) -- F9
RegisterKeyBind(0x79, function() capture("S4_ADS_ACTIVE") end) -- F10
RegisterKeyBind(0x7A, function() capture("S5_POST_ADS") end) -- F11

print("[" .. MODULE .. "] loaded: F7=S1 cinematic; F8=S2 post-EXIT no weapon; F9=S3 weapon visible; F10=S4 ADS; F11=S5 post-ADS\n")
