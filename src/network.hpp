#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "display.hpp"

const char *ssid = "JohnConnor_V3";
const char *password = "zy634218@";

const uint16_t DCS_UDP_PORT = 5000;

struct DCSData
{
    float baroAlt = 0.0f;   // m
    float radarAlt = 0.0f;  // m
    float ias = 0.0f;       // m/s
    float tas = 0.0f;       // m/s
    float vs = 0.0f;        // m/s
    float mach = 0.0f;
    float heading = 0.0f;   // deg
    float mhdg = 0.0f;      // deg
    float ax = 0.0f;        // m/s²
    float ay = 0.0f;        // m/s²
    float az = 0.0f;        // m/s²
    char aircraft[40] = {};
    bool valid = false;
    uint32_t lastUpdate = 0;
    uint32_t frameId = 0;
};

extern DCSData dcsData;
extern SemaphoreHandle_t dcsDataMutex;
extern bool wifiConnectFailed;

bool netconnect()
{
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("DCS-Display-Exporter");
    WiFi.setSleep(false);
    WiFi.begin(ssid, password);

    default_screen.fillSprite(TFT_BLACK);
    default_screen.setTextColor(TFT_WHITE);
    default_screen.setCursor(10, 20);
    default_screen.println("Connecting WiFi...");
    default_screen.pushSprite(0, 0);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        if (millis() - start > 20000)
            break;
    }
    return WiFi.status() == WL_CONNECTED;
}

static void parseDCSPacket(const char *line, DCSData &out)
{
    float baro = NAN, radar = NAN, ias = NAN, tas = NAN, vs = NAN, mach = NAN, hdg = NAN, mhdg = NAN, ax = NAN, ay = NAN, az = NAN;
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
                baro = atof(val);
            else if (strcmp(tok, "ALT_RADAR") == 0)
                radar = atof(val);
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
        }
        tok = strtok(NULL, "|\r\n");
    }
    free(dup);

    out.baroAlt = baro;
    out.radarAlt = radar;
    out.ias = ias;
    out.tas = tas;
    out.vs = vs;
    out.mach = mach;
    out.heading = hdg;
    out.mhdg = mhdg;
    out.ax = ax;
    out.ay = ay;
    out.az = az;
    strncpy(out.aircraft, aircraft, sizeof(out.aircraft) - 1);
}

void networkTask(void *param)
{
    WiFiUDP udp;

    const uint32_t WIFI_RETRY_MS = 60000;   // retry once per minute
    const int WIFI_MAX_ATTEMPTS = 5;        // give up after 5 minutes
    uint32_t lastWifiAttempt = 0;
    int wifiFailCount = 0;
    bool wasConnected = true;

    char buf[512];
    uint32_t frameCounter = 0;
    bool udpBound = false;
    while (true)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            wifiConnectFailed = false;
            wifiFailCount = 0;
            if (!udpBound)
            {
                udp.stop();
                udp.begin(WiFi.localIP(), DCS_UDP_PORT);
                udpBound = true;
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
                wifiFailCount = 0;
            }
            if (wifiFailCount >= WIFI_MAX_ATTEMPTS)
            {
                wifiConnectFailed = true;
            }
            else
            {
                uint32_t now = millis();
                if (now - lastWifiAttempt >= WIFI_RETRY_MS)
                {
                    lastWifiAttempt = now;
                    wifiFailCount++;
                    WiFi.begin(ssid, password);
                }
            }
        }

        int len = udp.parsePacket();
        if (len > 0)
        {
            int n = udp.read(buf, sizeof(buf) - 1);
            if (n > 0 && n == len)
            {
                buf[n] = '\0';
                DCSData parsed;
                parseDCSPacket(buf, parsed);
                parsed.valid = true;
                parsed.lastUpdate = millis();
                parsed.frameId = ++frameCounter;
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
