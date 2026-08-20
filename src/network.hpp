#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "units.hpp"
#include "wifi_config.h"

constexpr uint16_t DCS_UDP_PORT = 5000;
constexpr float DEG_PER_RAD = 57.29578f;

struct DCSData
{
    float baroAlt = 0.0f;
    float radarAlt = 0.0f;
    float ias = 0.0f;
    float tas = 0.0f;
    float vs = 0.0f;
    float mach = 0.0f;
    float aoa = 0.0f;
    float heading = 0.0f;
    float mhdg = 0.0f;
    float gForce = 0.0f;
    float chaff = 0.0f;
    float flare = 0.0f;
    char aircraft[40] = {};
    char altUnit[8] = {};
    char spdUnit[8] = {};
    char vsUnit[8] = {};
    bool altMetric = false;
    bool spdMetric = false;
    bool valid = false;
    uint32_t lastUpdate = 0;
};

extern DCSData dcsData;
extern SemaphoreHandle_t dcsDataMutex;

static bool parseFloat(const char *text, float &value)
{
    char *end = nullptr;
    float parsed = strtof(text, &end);
    if (end == text || *end != '\0' || !isfinite(parsed))
        return false;

    value = parsed;
    return true;
}

static bool parseDcsPacket(const char *line, DCSData &out)
{
    float baroAlt = NAN, radarAlt = NAN, ias = NAN, tas = NAN, vs = NAN;
    float mach = NAN, aoa = NAN, hdg = NAN, mhdg = NAN, ax = NAN, ay = NAN, az = NAN;
    float chaff = 0, flare = 0;
    char aircraft[sizeof(out.aircraft)] = {};

    // Parse a bounded local copy so packets do not allocate from the heap.
    char packet[512] = {};
    size_t packetLength = strnlen(line, sizeof(packet) - 1);
    memcpy(packet, line, packetLength);

    char *tok = strtok(packet, "|\r\n");
    if (tok == nullptr || strcmp(tok, "DCS") != 0)
        return false;

    while (tok != nullptr)
    {
        char *eq = strchr(tok, '=');
        if (eq != nullptr)
        {
            *eq = '\0';
            const char *val = eq + 1;
            if (strcmp(tok, "ALT_BARO") == 0)
            {
                if (!parseFloat(val, baroAlt))
                    return false;
            }
            else if (strcmp(tok, "ALT_RADAR") == 0)
            {
                if (!parseFloat(val, radarAlt))
                    return false;
            }
            else if (strcmp(tok, "IAS") == 0)
            {
                if (!parseFloat(val, ias))
                    return false;
            }
            else if (strcmp(tok, "TAS") == 0)
            {
                if (!parseFloat(val, tas))
                    return false;
            }
            else if (strcmp(tok, "VS") == 0)
            {
                if (!parseFloat(val, vs))
                    return false;
            }
            else if (strcmp(tok, "MACH") == 0)
            {
                if (!parseFloat(val, mach))
                    return false;
            }
            else if (strcmp(tok, "AOA") == 0)
            {
                if (!parseFloat(val, aoa))
                    return false;
            }
            else if (strcmp(tok, "HDG") == 0)
            {
                if (!parseFloat(val, hdg))
                    return false;
            }
            else if (strcmp(tok, "MHDG") == 0)
            {
                if (!parseFloat(val, mhdg))
                    return false;
            }
            else if (strcmp(tok, "AX") == 0)
            {
                if (!parseFloat(val, ax))
                    return false;
            }
            else if (strcmp(tok, "AY") == 0)
            {
                if (!parseFloat(val, ay))
                    return false;
            }
            else if (strcmp(tok, "AZ") == 0)
            {
                if (!parseFloat(val, az))
                    return false;
            }
            else if (strcmp(tok, "AC") == 0)
                strncpy(aircraft, val, sizeof(aircraft) - 1);
            else if (strcmp(tok, "CHAFF") == 0)
            {
                if (!parseFloat(val, chaff))
                    return false;
            }
            else if (strcmp(tok, "FLARE") == 0)
            {
                if (!parseFloat(val, flare))
                    return false;
            }
        }
        tok = strtok(nullptr, "|\r\n");
    }

    if (!isfinite(baroAlt) || !isfinite(radarAlt) || !isfinite(ias) ||
        !isfinite(tas) || !isfinite(vs) || !isfinite(mach) ||
        !isfinite(aoa) || !isfinite(hdg) || !isfinite(mhdg) ||
        !isfinite(ax) || !isfinite(ay) || !isfinite(az))
    {
        return false;
    }

    out.baroAlt = baroAlt;
    out.radarAlt = radarAlt;
    out.ias = ias;
    out.tas = tas;
    out.vs = vs;
    out.mach = mach;
    // DCS returns AOA in degrees for the current export implementation.
    out.aoa = aoa;
    out.heading = hdg * DEG_PER_RAD;
    out.mhdg = mhdg * DEG_PER_RAD;
    out.gForce = ay;
    out.chaff = chaff;
    out.flare = flare;
    strncpy(out.aircraft, aircraft, sizeof(out.aircraft) - 1);
    return true;
}

void networkTask(void *)
{
    WiFiUDP udp;

    const uint32_t WIFI_RETRY_MS = 60000;
    const uint32_t UDP_INACTIVITY_TIMEOUT_MS = 10000;
    uint32_t lastWifiAttempt = 0;
    bool wasConnected = false;

    WiFi.mode(WIFI_STA);
    WiFi.setHostname("DCS-Display-Exporter");
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    char buf[512];
    uint32_t lastUdpPacketMs = 0;
    bool udpBound = false;
    while (true)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            if (!udpBound)
            {
                udp.stop();
                const bool udpBindOk = udp.begin(DCS_UDP_PORT);
                if (!udpBindOk)
                {
                    udpBound = false;
                    vTaskDelay(pdMS_TO_TICKS(100));
                    continue;
                }

                udpBound = true;
                lastUdpPacketMs = millis();
            }
            if (!wasConnected)
                wasConnected = true;
        }
        else
        {
            if (wasConnected)
            {
                wasConnected = false;
                udpBound = false;
                lastWifiAttempt = millis();
            }
            uint32_t now = millis();
            if (now - lastWifiAttempt >= WIFI_RETRY_MS)
            {
                lastWifiAttempt = now;
                WiFi.begin(WIFI_SSID, WIFI_PASS);
            }
        }

        if (udpBound && millis() - lastUdpPacketMs >= UDP_INACTIVITY_TIMEOUT_MS)
        {
            udpBound = false;
            continue;
        }

        int len = udp.parsePacket();
        if (len > 0)
        {
            int n = udp.read(buf, min(len, (int)(sizeof(buf) - 1)));
            if (n > 0)
            {
                buf[n] = '\0';
                DCSData parsed;
                if (!parseDcsPacket(buf, parsed))
                {
                    vTaskDelay(pdMS_TO_TICKS(5));
                    continue;
                }

                lastUdpPacketMs = millis();
                parsed.valid = true;
                parsed.lastUpdate = millis();

                parsed.altMetric = dcs_units::altitudeIsMetric(parsed.aircraft);
                parsed.spdMetric = dcs_units::speedIsMetric(parsed.aircraft);

                if (!parsed.altMetric)
                {
                    parsed.baroAlt = dcs_units::metersToFeet(parsed.baroAlt);
                    parsed.radarAlt = dcs_units::metersToFeet(parsed.radarAlt);
                }
                strncpy(parsed.altUnit, parsed.altMetric ? "M" : "FT", sizeof(parsed.altUnit) - 1);

                if (parsed.spdMetric)
                {
                    parsed.ias = dcs_units::metersPerSecondToKilometersPerHour(parsed.ias);
                    parsed.tas = dcs_units::metersPerSecondToKilometersPerHour(parsed.tas);
                }
                else
                {
                    parsed.ias = dcs_units::metersPerSecondToKnots(parsed.ias);
                    parsed.tas = dcs_units::metersPerSecondToKnots(parsed.tas);
                    parsed.vs = dcs_units::metersPerSecondToFeetPerMinute(parsed.vs);
                }
                strncpy(parsed.spdUnit, parsed.spdMetric ? "KM/H" : "KTS", sizeof(parsed.spdUnit) - 1);
                strncpy(parsed.vsUnit, parsed.spdMetric ? "M/S" : "FPM", sizeof(parsed.vsUnit) - 1);
                if (dcsDataMutex != nullptr)
                {
                    if (xSemaphoreTake(dcsDataMutex, 10))
                    {
                        dcsData = parsed;
                        xSemaphoreGive(dcsDataMutex);
                    }
                }
                else
                {
                    dcsData = parsed;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
