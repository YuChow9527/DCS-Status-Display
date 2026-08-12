local HOST = "192.168.1.100"
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

    local okF = pcall(function()
        local modelTime = Export.LoGetModelTime() or 0
        if lastSendTime >= 0 and modelTime >= lastSendTime and (modelTime - lastSendTime) < UPDATE_INTERVAL then
            return
        end
        lastSendTime = modelTime

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
        local hdg = selfData.Heading or 0
        local magYaw = Export.LoGetMagneticYaw() or 0

        local latLongAlt = selfData.LatLongAlt or {}
        local lat = latLongAlt.Lat or 0
        local lon = latLongAlt.Long or 0

        local accel = Export.LoGetAccelerationUnits() or {}
        local ax = accel.x or 0
        local ay = accel.y or 0
        local az = accel.z or 0

        local snares = Export.LoGetSnares()
        local chaff = 0
        local flare = 0
        if snares then
            chaff = snares.chaff or 0
            flare = snares.flare or 0
        end

        local msg = string.format(
            "DCS|ALT_BARO=%.2f|ALT_RADAR=%.2f|IAS=%.3f|TAS=%.3f|VS=%.2f|MACH=%.4f|HDG=%.4f|MHDG=%.4f|AX=%.4f|AY=%.4f|AZ=%.4f|LAT=%.6f|LON=%.6f|AC=%s|CHAFF=%.0f|FLARE=%.0f",
            baro, radar, ias, tas, vs, mach, hdg, magYaw, ax, ay, az, lat, lon, acName, chaff, flare
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
