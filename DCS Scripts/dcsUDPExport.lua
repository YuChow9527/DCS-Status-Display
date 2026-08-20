local HOST = "192.168.40.110"
local PORT = 5000
local UPDATE_INTERVAL = 0.033

local udp
local lastSendTime = -1

local function ensureSocket()
    if udp then
        return true
    end
    local socketLoaded, socketLibrary = pcall(require, "socket")
    if not socketLoaded then
        return false
    end
    local socket = socketLibrary.udp()
    local peerOk = pcall(function() socket:setpeername(HOST, PORT) end)
    if not peerOk then
        return false
    end
    udp = socket
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

    local frameOk = pcall(function()
        local modelTime = Export.LoGetModelTime() or 0
        if lastSendTime >= 0 and modelTime >= lastSendTime and (modelTime - lastSendTime) < UPDATE_INTERVAL then
            return
        end
        lastSendTime = modelTime

        local selfData = Export.LoGetSelfData()
        if not selfData then
            return
        end

        local acName = selfData.Name

        local baro = Export.LoGetAltitudeAboveSeaLevel()
        local radar = Export.LoGetAltitudeAboveGroundLevel()
        local ias = Export.LoGetIndicatedAirSpeed()
        local tas = Export.LoGetTrueAirSpeed()
        local vs = Export.LoGetVerticalVelocity()
        local mach = Export.LoGetMachNumber()
        local aoa = Export.LoGetAngleOfAttack()
        local hdg = selfData.Heading
        local magYaw = Export.LoGetMagneticYaw()

        local latLongAlt = selfData.LatLongAlt
        local lat = latLongAlt.Lat
        local lon = latLongAlt.Long

        local accel = Export.LoGetAccelerationUnits()
        local ax = accel.x
        local ay = accel.y
        local az = accel.z

        local snares = Export.LoGetSnares()
        local chaff = snares.chaff
        local flare = snares.flare

        local msg = string.format(
            "DCS|ALT_BARO=%s|ALT_RADAR=%s|IAS=%s|TAS=%s|VS=%s|MACH=%s|HDG=%s|MHDG=%s|AX=%s|AY=%s|AZ=%s|LAT=%s|LON=%s|AC=%s|CHAFF=%s|FLARE=%s|AOA=%s",
            tostring(baro), tostring(radar), tostring(ias), tostring(tas), tostring(vs), tostring(mach), tostring(hdg), tostring(magYaw), tostring(ax), tostring(ay), tostring(az), tostring(lat), tostring(lon), tostring(acName), tostring(chaff), tostring(flare), tostring(aoa)
        )

        local sent, sendError = udp:send(msg)
        if not sent then
            error(sendError or "UDP send failed")
        end
    end)
    if not frameOk then
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
