#include <Arduino.h>
#include <cmath>
#include <LovyanGFX.hpp>
#include "display.hpp"
#include "network.hpp"
#include "units.hpp"
#include "fonts/RobotoMono_SemiBold_16.h"
DCSData dcsData;
SemaphoreHandle_t dcsDataMutex = NULL;
bool wifiConnectFailed = false;

#define LABEL_COLOR 0xFFE0  // TFT_YELLOW
#define VALUE_COLOR 0x07E0  // TFT_GREEN
#define UNIT_COLOR  0x07FF  // TFT_CYAN
#define WARN_COLOR  0xF800  // TFT_RED

static void drawRow(int y, const char *label, const char *value, const char *unit)
{
    const int gap = 8;
    const int rightEdge = 470;

    int uw = 0;
    if (unit[0] != '\0')
    {
        uw = default_screen.textWidth(unit);
    }

    int valueRight = (unit[0] != '\0') ? rightEdge - uw - gap : rightEdge;

    default_screen.setTextDatum(textdatum_t::middle_left);
    default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
    default_screen.drawString(label, 10, y);

    default_screen.setTextDatum(textdatum_t::middle_right);
    default_screen.setTextColor(VALUE_COLOR, TFT_BLACK);
    default_screen.drawString(value, valueRight, y);

    if (unit[0] != '\0')
    {
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString(unit, valueRight + gap, y);
    }
}

static void drawStatusScreen()
{
    DCSData d;
    if (dcsDataMutex != NULL && xSemaphoreTake(dcsDataMutex, 20))
    {
        d = dcsData;
        xSemaphoreGive(dcsDataMutex);
    }

    bool stale = !d.valid || (millis() - d.lastUpdate) > 2000;

    default_screen.fillSprite(TFT_BLACK);

    if (stale)
    {
        default_screen.setTextDatum(textdatum_t::top_center);
        default_screen.setTextColor(TFT_RED, TFT_BLACK);
        default_screen.drawString("NO DATA", 240, 42);
        default_screen.setTextDatum(textdatum_t::middle_center);
        default_screen.setTextColor(TFT_YELLOW, TFT_BLACK);
        default_screen.drawString("WAITING FOR DCS", 240, 140);
        if (WiFi.status() == WL_CONNECTED)
        {
            default_screen.setTextColor(TFT_GREEN, TFT_BLACK);
            default_screen.drawString("IP: " + String(WiFi.localIP().toString()), 240, 190);
        }
        else if (wifiConnectFailed)
        {
            default_screen.setTextColor(TFT_RED, TFT_BLACK);
            default_screen.drawString("Network Connect Failed", 240, 190);
        }
        else
        {
            default_screen.setTextColor(TFT_YELLOW, TFT_BLACK);
            default_screen.drawString("Network Connecting", 240, 190);
        }
        default_screen.pushSprite(0, 0);
        return;
    }

    bool altMetric = d.altMetric;
    bool spdMetric = d.spdMetric;
    const char *altUnit = d.altUnit;
    const char *spdUnit = d.spdUnit;

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", d.baroAlt);
    drawRow(54, "ALT", buf, altUnit);

    snprintf(buf, sizeof(buf), "%.1f", d.radarAlt);
    float raltThreshold = altMetric ? 300.0f : 1000.0f;
    if (d.radarAlt < raltThreshold)
    {
        int vRight = 470 - default_screen.textWidth(altUnit) - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("RALT", 10, 82);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 82);
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString(altUnit, vRight + 8, 82);
    }
    else
    {
        drawRow(82, "RALT", buf, altUnit);
    }

    snprintf(buf, sizeof(buf), "%.1f", d.ias);
    float iasLowThreshold = spdMetric ? 550.0f : 300.0f;
    if (d.ias < iasLowThreshold)
    {
        int vRight = 470 - default_screen.textWidth(spdUnit) - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("IAS", 10, 110);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 110);
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString(spdUnit, vRight + 8, 110);
    }
    else
    {
        drawRow(110, "IAS", buf, spdUnit);
    }

    snprintf(buf, sizeof(buf), "%.1f", d.tas);
    drawRow(138, "TAS", buf, spdUnit);

    snprintf(buf, sizeof(buf), "%.1f", d.vs);
    drawRow(166, "V/S", buf, d.vsUnit);

    snprintf(buf, sizeof(buf), "%.3f", d.mach);
    if (d.mach > 1.0f)
    {
        int vRight = 470 - default_screen.textWidth("MACH") - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("MACH", 10, 194);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 194);
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString("MACH", vRight + 8, 194);
    }
    else
    {
        drawRow(194, "MACH", buf, "MACH");
    }

    snprintf(buf, sizeof(buf), "%.2f", d.gForce);
    if (d.gForce > 6.0f)
    {
        int vRight = 470 - default_screen.textWidth("G") - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("G", 10, 222);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 222);
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString("G", vRight + 8, 222);
    }
    else
    {
        drawRow(222, "G", buf, "G");
    }

    float hdg = fmodf(d.heading, 360.0f);
    if (hdg < 0) hdg += 360.0f;
    snprintf(buf, sizeof(buf), "%03.0f", hdg);
    drawRow(250, "HDG", buf, "DEG");

    float mhdg = fmodf(d.mhdg, 360.0f);
    if (mhdg < 0) mhdg += 360.0f;
    snprintf(buf, sizeof(buf), "%03.0f", mhdg);
    drawRow(278, "MHDG", buf, "DEG");

    default_screen.pushSprite(0, 0);
}

static void displayTask(void *param)
{
    while (true)
    {
        drawStatusScreen();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup()
{
    initscreen();
    initdefaultsprite();
    default_screen.setFont(&RobotoMono_SemiBold16pt7b);
    netconnect();
    dcsDataMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(networkTask, "network", 8192, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(displayTask, "display", 16384, NULL, 2, NULL, 1);
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
