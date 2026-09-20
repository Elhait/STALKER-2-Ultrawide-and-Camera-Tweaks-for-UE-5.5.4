-- Ground-truth logger for DialogueBoundary correlation.
-- Read-only: observes APC::IsInStaticDialog() and logs only state edges.

local UEHelpers = require("UEHelpers")
local SAMPLE_MS = 50
local lastState = nil
local sequence = 0

local function emit(message)
    print("[DIALOG_ORACLE] " .. message .. "\n")
end

local function valid(object)
    if object == nil then return false end
    local ok, result = pcall(function() return object:IsValid() end)
    return ok and result == true
end

LoopAsync(SAMPLE_MS, function()
    -- Map/controller transitions can temporarily make UEHelpers.GetPlayer()
    -- unavailable. Skip this sample and let LoopAsync retry on the next tick.
    local playerOk, player = pcall(function()
        return UEHelpers.GetPlayer()
    end)
    if not playerOk then
        return false
    end

    if not valid(player) then
        return false
    end

    local ok, state = pcall(function()
        return player:IsInStaticDialog()
    end)
    if not ok then
        emit("ERROR IsInStaticDialog unavailable=" .. tostring(state))
        return true
    end

    local stateText = state == true and "true" or "false"
    if stateText ~= lastState then
        sequence = sequence + 1
        emit("seq=" .. sequence .. " IsInStaticDialog=" .. stateText)
        lastState = stateText
    end
    return false
end)

emit("loaded; read-only edge polling interval_ms=" .. SAMPLE_MS)
