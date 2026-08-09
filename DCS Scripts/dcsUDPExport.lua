local HOST = "192.168.40.110"
local PORT = 5000
local UPDATE_INTERVAL = 0.033  -- 30 Hz

local udp
local lastSendTime = -1
local sentCount = 0
local lastLogTime = -1
local logFile = nil

local function logWrite(msg)
    if logFile then
        logFile:write(os.date("%H:%M:%S ") .. msg .. "\n")
        logFile:flush()
    end
end

local function ensureLog()
    if not logFile then
        logFile = io.open(lfs.writedir() .. "/Logs/DCS-udp-export.log", "a")
    end
end

-- Rebuilds the UDP socket if missing (self-heals after slot/aircraft changes
-- where onSimulationStart is not called again).
local function ensureSocket()
    if udp then
        return true
    end
    ensureLog()
    logWrite("opening socket")
    local ok, sock = pcall(require, "socket")
    if not ok then
        logWrite("ERROR: require socket failed: " .. tostring(sock))
        return false
    end
    local u = sock.udp()
    local okp = pcall(function() u:setpeername(HOST, PORT) end)
    if not okp then
        logWrite("ERROR: setpeername failed")
        return false
    end
    udp = u
    logWrite("udp socket ready -> " .. HOST .. ":" .. PORT)
    return true
end

package.path = package.path .. ";" .. lfs.currentdir() .. "/LuaSocket/?.lua"
package.cpath = package.cpath .. ";" .. lfs.currentdir() .. "/LuaSocket/?.dll"

ensureLog()
logWrite("=== DCS UDP export hook loaded ===")

local callbacks = {}

function callbacks.onSimulationStart()
    lastSendTime = -1
    lastLogTime = -1
    logWrite("onSimulationStart")
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
            if lastLogTime < 0 or (t - lastLogTime) >= 5 then
                lastLogTime = t
                logWrite("no self data (player not in aircraft?)")
            end
            return
        end

        local acName = selfData.Name or ""

        -- Raw values as exported by DCS: altitudes in meters, speeds in m/s,
        -- mach dimensionless, headings in degrees. Unit conversion is done on
        -- the ESP32 side.
        local baro = Export.LoGetAltitudeAboveSeaLevel() or 0      -- m
        local radar = Export.LoGetAltitudeAboveGroundLevel() or 0  -- m
        local ias = Export.LoGetIndicatedAirSpeed() or 0           -- m/s
        local tas = Export.LoGetTrueAirSpeed() or 0                -- m/s
        local vs = Export.LoGetVerticalVelocity() or 0             -- m/s
        local mach = Export.LoGetMachNumber() or 0
        local hdg = math.deg(selfData.Heading or 0) % 360
        local magYaw = Export.LoGetMagneticYaw() or 0
        if math.abs(magYaw) > 6.29 then
            magYaw = magYaw % 360
        else
            magYaw = math.deg(magYaw) % 360
        end

        local accel = Export.LoGetAccelerationUnits() or {}
        local ax = accel.x or 0
        local ay = accel.y or 0
        local az = accel.z or 0

        local msg = string.format(
            "DCS|ALT_BARO=%.1f|ALT_RADAR=%.1f|IAS=%.2f|TAS=%.2f|VS=%.1f|MACH=%.3f|HDG=%.1f|MHDG=%.1f|AX=%.3f|AY=%.3f|AZ=%.3f|AC=%s",
            baro, radar, ias, tas, vs, mach, hdg, magYaw, ax, ay, az, acName
        )

        udp:send(msg)
        sentCount = sentCount + 1
        if lastLogTime < 0 or (t - lastLogTime) >= 5 then
            lastLogTime = t
            logWrite(string.format("sent %d msgs (ac=%s)", sentCount, acName))
        end
    end)
    if not okF then
        logWrite("ERROR frame: " .. tostring(errF))
        if udp then
            pcall(function() udp:close() end)
            udp = nil
        end
    end
end

function callbacks.onSimulationStop()
    logWrite("onSimulationStop")
    if udp then
        pcall(function() udp:close() end)
        udp = nil
    end
    if logFile then
        logFile:close()
        logFile = nil
    end
end

DCS.setUserCallbacks(callbacks)
