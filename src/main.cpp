#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "display.hpp"
#include "network.hpp"
#include "fonts/RobotoMono_SemiBold_10.h"
#include "fonts/RobotoMono_SemiBold_16.h"
DCSData dcsData;
SemaphoreHandle_t dcsDataMutex = NULL;

#define LABEL_COLOR 0xFFE0  // TFT_YELLOW
#define VALUE_COLOR 0x07E0  // TFT_GREEN
#define MACH_COLOR  0xF81F  // TFT_PINK
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
        static int bx = 60, by = 60;
        static int dx = 4, dy = 2;
        static bool init = false;
        if (!init)
        {
            bx = random(40, 400);
            by = random(40, 240);
            dx = random(2) ? 2 : -2;
            dy = random(2) ? 1 : -1;
            init = true;
        }

        char ipLine[32] = {};
        uint16_t ipColor = TFT_YELLOW;
        if (WiFi.status() == WL_CONNECTED)
        {
            snprintf(ipLine, sizeof(ipLine), "IP: %s", WiFi.localIP().toString().c_str());
            ipColor = TFT_GREEN;
        }
        else
        {
            strncpy(ipLine, "Connecting", sizeof(ipLine) - 1);
        }

        default_screen.setFont(&RobotoMono_SemiBold10pt7b);

        const int lineH = default_screen.fontHeight();
        const int gap = 4;
        int ndW = default_screen.textWidth("NO DATA");
        int ipW = default_screen.textWidth(ipLine);
        int bw = max(ndW, ipW);
        int bh = lineH * 2 + gap;

        bx += dx;
        by += dy;
        if (bx <= 0 || bx + bw >= 480) { dx = -dx; bx = constrain(bx, 0, 480 - bw); }
        if (by <= 0 || by + bh >= 320) { dy = -dy; by = constrain(by, 0, 320 - bh); }

        int cx = bx + bw / 2;
        default_screen.setTextDatum(textdatum_t::top_center);
        default_screen.setTextColor(TFT_RED, TFT_BLACK);
        default_screen.drawString("NO DATA", cx, by);
        default_screen.setTextColor(ipColor, TFT_BLACK);
        default_screen.drawString(ipLine, cx, by + lineH + gap);

        default_screen.setFont(&RobotoMono_SemiBold16pt7b);
        default_screen.pushSprite(0, 0);
        return;
    }

    bool altMetric = d.altMetric;
    bool spdMetric = d.spdMetric;
    const char *altUnit = d.altUnit;
    const char *spdUnit = d.spdUnit;

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", d.baroAlt);
    drawRow(20, "ALT", buf, altUnit);

    snprintf(buf, sizeof(buf), "%.1f", d.radarAlt);
    float raltThreshold = altMetric ? 300.0f : 1000.0f;
    if (d.radarAlt < raltThreshold)
    {
        int vRight = 470 - default_screen.textWidth(altUnit) - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("RALT", 10, 48);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 48);
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString(altUnit, vRight + 8, 48);
    }
    else
    {
        drawRow(48, "RALT", buf, altUnit);
    }

    snprintf(buf, sizeof(buf), "%.1f", d.ias);
    float iasLowThreshold = spdMetric ? 550.0f : 300.0f;
    if (d.ias < iasLowThreshold)
    {
        int vRight = 470 - default_screen.textWidth(spdUnit) - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("IAS", 10, 76);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 76);
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString(spdUnit, vRight + 8, 76);
    }
    else
    {
        drawRow(76, "IAS", buf, spdUnit);
    }

    snprintf(buf, sizeof(buf), "%.1f", d.tas);
    drawRow(104, "TAS", buf, spdUnit);

    snprintf(buf, sizeof(buf), "%.1f", d.vs);
    drawRow(132, "V/S", buf, d.vsUnit);

    snprintf(buf, sizeof(buf), "%.3f", d.mach);
    if (d.mach > 1.0f)
    {
        int vRight = 470 - default_screen.textWidth("MACH") - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("MACH", 10, 160);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 160);
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString("MACH", vRight + 8, 160);
    }
    else
    {
        int vRight = 470 - default_screen.textWidth("MACH") - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("MACH", 10, 160);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(MACH_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 160);
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString("MACH", vRight + 8, 160);
    }

    snprintf(buf, sizeof(buf), "%.2f", d.gForce);
    if (d.gForce > 6.0f)
    {
        int vRight = 470 - default_screen.textWidth("G") - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("G FORCE", 10, 188);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 188);
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(UNIT_COLOR, TFT_BLACK);
        default_screen.drawString("G", vRight + 8, 188);
    }
    else
    {
        drawRow(188, "G FORCE", buf, "G");
    }

    float hdg = fmodf(d.heading, 360.0f);
    if (hdg < 0) hdg += 360.0f;
    snprintf(buf, sizeof(buf), "%03.0f", hdg);
    drawRow(216, "HDG", buf, "DEG");

    float mhdg = fmodf(d.mhdg, 360.0f);
    if (mhdg < 0) mhdg += 360.0f;
    snprintf(buf, sizeof(buf), "%03.0f", mhdg);
    drawRow(244, "MHDG", buf, "DEG");

    snprintf(buf, sizeof(buf), "%.0f", d.chaff);
    if (d.chaff < 10.0f)
    {
        int vRight = 470 - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("CHAF", 10, 272);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 272);
    }
    else
    {
        drawRow(272, "CHAF", buf, "");
    }

    snprintf(buf, sizeof(buf), "%.0f", d.flare);
    if (d.flare < 10.0f)
    {
        int vRight = 470 - 8;
        default_screen.setTextDatum(textdatum_t::middle_left);
        default_screen.setTextColor(LABEL_COLOR, TFT_BLACK);
        default_screen.drawString("FLAR", 10, 300);
        default_screen.setTextDatum(textdatum_t::middle_right);
        default_screen.setTextColor(WARN_COLOR, TFT_BLACK);
        default_screen.drawString(buf, vRight, 300);
    }
    else
    {
        drawRow(300, "FLAR", buf, "");
    }

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
    dcsDataMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(networkTask, "network", 16384, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(displayTask, "display", 16384, NULL, 2, NULL, 1);
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
