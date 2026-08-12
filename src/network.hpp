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

static void parseDCSPacket(const char *line, DCSData &out)
{
    float baroAlt = NAN, radarAlt = NAN, ias = NAN, tas = NAN, vs = NAN;
    float mach = NAN, hdg = NAN, mhdg = NAN, ax = NAN, ay = NAN, az = NAN;
    float chaff = 0, flare = 0;
    char aircraft[sizeof(out.aircraft)] = {};

    char *dup = strdup(line);
    if (dup == NULL)
        return;

    char *tok = strtok(dup, "|\r\n");
    while (tok != NULL)
    {
        char *eq = strchr(tok, '=');
        if (eq != NULL)
        {
            *eq = '\0';
            const char *val = eq + 1;
            if (strcmp(tok, "ALT_BARO") == 0)
                baroAlt = atof(val);
            else if (strcmp(tok, "ALT_RADAR") == 0)
                radarAlt = atof(val);
            else if (strcmp(tok, "IAS") == 0)
                ias = atof(val);
            else if (strcmp(tok, "TAS") == 0)
                tas = atof(val);
            else if (strcmp(tok, "VS") == 0)
                vs = atof(val);
            else if (strcmp(tok, "MACH") == 0)
                mach = atof(val);
            else if (strcmp(tok, "HDG") == 0)
                hdg = atof(val);
            else if (strcmp(tok, "MHDG") == 0)
                mhdg = atof(val);
            else if (strcmp(tok, "AX") == 0)
                ax = atof(val);
            else if (strcmp(tok, "AY") == 0)
                ay = atof(val);
            else if (strcmp(tok, "AZ") == 0)
                az = atof(val);
            else if (strcmp(tok, "AC") == 0)
                strncpy(aircraft, val, sizeof(aircraft) - 1);
            else if (strcmp(tok, "CHAFF") == 0)
                chaff = atof(val);
            else if (strcmp(tok, "FLARE") == 0)
                flare = atof(val);
        }
        tok = strtok(NULL, "|\r\n");
    }
    free(dup);

    out.baroAlt = baroAlt;
    out.radarAlt = radarAlt;
    out.ias = ias;
    out.tas = tas;
    out.vs = vs;
    out.mach = mach;
    out.heading = hdg * DEG_PER_RAD;
    out.mhdg = mhdg * DEG_PER_RAD;
    out.gForce = ay;
    out.chaff = chaff;
    out.flare = flare;
    strncpy(out.aircraft, aircraft, sizeof(out.aircraft) - 1);
}

void networkTask(void *param)
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
                udp.begin(WiFi.localIP(), DCS_UDP_PORT);
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
            lastUdpPacketMs = millis();
            int n = udp.read(buf, min(len, (int)(sizeof(buf) - 1)));
            if (n > 0)
            {
                buf[n] = '\0';
                DCSData parsed;
                parseDCSPacket(buf, parsed);
                parsed.valid = true;
                parsed.lastUpdate = millis();

                parsed.altMetric = dcs_units::altitudeIsMetric(parsed.aircraft);
                parsed.spdMetric = dcs_units::speedIsMetric(parsed.aircraft);

                if (!parsed.altMetric)
                {
                    parsed.baroAlt = dcs_units::mToFt(parsed.baroAlt);
                    parsed.radarAlt = dcs_units::mToFt(parsed.radarAlt);
                }
                strncpy(parsed.altUnit, parsed.altMetric ? "M" : "FT", sizeof(parsed.altUnit) - 1);

                if (parsed.spdMetric)
                {
                    parsed.ias = dcs_units::msToKmh(parsed.ias);
                    parsed.tas = dcs_units::msToKmh(parsed.tas);
                }
                else
                {
                    parsed.ias = dcs_units::msToKts(parsed.ias);
                    parsed.tas = dcs_units::msToKts(parsed.tas);
                    parsed.vs = dcs_units::msToFpm(parsed.vs);
                }
                strncpy(parsed.spdUnit, parsed.spdMetric ? "KM/H" : "KTS", sizeof(parsed.spdUnit) - 1);
                strncpy(parsed.vsUnit, parsed.spdMetric ? "M/S" : "FPM", sizeof(parsed.vsUnit) - 1);
                if (dcsDataMutex != NULL)
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
