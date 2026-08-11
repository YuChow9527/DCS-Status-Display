local HOST = "192.168.40.110"
local PORT = 5000
local UPDATE_INTERVAL = 0.033

local udp
local lastSendTime = -1

local function ensureSocket()
    if udp then
        return true
    end
    local ok, sock = pcall(require, "socket")
    if not ok then
        return false
    end
    local u = sock.udp()
    local okp = pcall(function() u:setpeername(HOST, PORT) end)
    if not okp then
        return false
    end
    udp = u
    return true
end

package.path = package.path .. ";" .. lfs.currentdir() .. "/LuaSocket/?.lua"
package.cpath = package.cpath .. ";" .. lfs.currentdir() .. "/LuaSocket/?.dll"

local callbacks = {}

function callbacks.onSimulationStart()
    lastSendTime = -1
    ensureSocket()
end

function callbacks.onSimulationFrame()
    if not ensureSocket() then
        return
    end

    local okF, errF = pcall(function()
        local t = Export.LoGetModelTime() or 0
        if lastSendTime >= 0 and t >= lastSendTime and (t - lastSendTime) < UPDATE_INTERVAL then
            return
        end
        lastSendTime = t

        local selfData = Export.LoGetSelfData()
        if not selfData then
            return
        end

        local acName = selfData.Name or ""

        local baro = Export.LoGetAltitudeAboveSeaLevel() or 0
        local radar = Export.LoGetAltitudeAboveGroundLevel() or 0
        local ias = Export.LoGetIndicatedAirSpeed() or 0
        local tas = Export.LoGetTrueAirSpeed() or 0
        local vs = Export.LoGetVerticalVelocity() or 0
        local mach = Export.LoGetMachNumber() or 0
        local hdg = math.deg(selfData.Heading or 0) % 360
        local magYaw = Export.LoGetMagneticYaw() or 0
        if math.abs(magYaw) > 6.29 then
            magYaw = magYaw % 360
        else
            magYaw = math.deg(magYaw) % 360
        end

        local accel = Export.LoGetAccelerationUnits() or {}
        local ay = accel.y or 0

        local snares = Export.LoGetSnares()
        local chaff = 0
        local flare = 0
        if snares then
            chaff = snares.chaff or 0
            flare = snares.flare or 0
        end

        local msg = string.format(
            "DCS|ALT_BARO=%.1f|ALT_RADAR=%.1f|IAS=%.2f|TAS=%.2f|VS=%.1f|MACH=%.3f|HDG=%.1f|MHDG=%.1f|AX=%.3f|AY=%.3f|AZ=%.3f|AC=%s|CHAFF=%.0f|FLARE=%.0f",
            baro, radar, ias, tas, vs, mach, hdg, magYaw, 0, ay, 0, acName, chaff, flare
        )

        udp:send(msg)
    end)
    if not okF then
        if udp then
            pcall(function() udp:close() end)
            udp = nil
        end
    end
end

function callbacks.onSimulationStop()
    if udp then
        pcall(function() udp:close() end)
        udp = nil
    end
end

DCS.setUserCallbacks(callbacks)
