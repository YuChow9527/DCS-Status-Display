#include <Arduino.h>
#include "display.hpp"
#include "network.hpp"
#include "fonts/RobotoMono_SemiBold_10.h"
#include "fonts/RobotoMono_SemiBold_14.h"
DCSData dcsData;
SemaphoreHandle_t dcsDataMutex = nullptr;

static constexpr uint16_t kLabelColor = 0xFFE0; // TFT_YELLOW
static constexpr uint16_t kValueColor = 0x07E0; // TFT_GREEN
static constexpr uint16_t kMachColor = 0xF81F;  // TFT_PINK
static constexpr uint16_t kUnitColor = 0x07FF;  // TFT_CYAN
static constexpr uint16_t kWarnColor = 0xF800;  // TFT_RED

static constexpr int kLayoutGap = 8;
static constexpr int kLayoutRight = 470;
static constexpr int kLayoutLeft = 10;

static void drawRow(int y, const char *label, const char *value, const char *unit)
{
    int uw = 0;
    if (unit[0] != '\0')
        uw = displaySprite.textWidth(unit);

    int valueRight = (unit[0] != '\0') ? kLayoutRight - uw - kLayoutGap : kLayoutRight;

    displaySprite.setTextDatum(textdatum_t::middle_left);
    displaySprite.setTextColor(kLabelColor, TFT_BLACK);
    displaySprite.drawString(label, kLayoutLeft, y);

    displaySprite.setTextDatum(textdatum_t::middle_right);
    displaySprite.setTextColor(kValueColor, TFT_BLACK);
    displaySprite.drawString(value, valueRight, y);

    if (unit[0] != '\0')
    {
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kUnitColor, TFT_BLACK);
        displaySprite.drawString(unit, valueRight + kLayoutGap, y);
    }
}

static void drawStatusScreen()
{
    DCSData d;
    if (dcsDataMutex != nullptr && xSemaphoreTake(dcsDataMutex, 20))
    {
        d = dcsData;
        xSemaphoreGive(dcsDataMutex);
    }

    bool stale = !d.valid || (millis() - d.lastUpdate) > 2000;

    displaySprite.fillSprite(TFT_BLACK);

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

        displaySprite.setFont(&RobotoMono_SemiBold10pt7b);

        const int lineH = displaySprite.fontHeight();
        const int gap = 4;
        int ndW = displaySprite.textWidth("NO DATA");
        int ipW = displaySprite.textWidth(ipLine);
        int bw = max(ndW, ipW);
        int bh = lineH * 2 + gap;

        bx += dx;
        by += dy;
        if (bx <= 0 || bx + bw >= 480) { dx = -dx; bx = constrain(bx, 0, 480 - bw); }
        if (by <= 0 || by + bh >= 320) { dy = -dy; by = constrain(by, 0, 320 - bh); }

        int cx = bx + bw / 2;
        displaySprite.setTextDatum(textdatum_t::top_center);
        displaySprite.setTextColor(TFT_RED, TFT_BLACK);
        displaySprite.drawString("NO DATA", cx, by);
        displaySprite.setTextColor(ipColor, TFT_BLACK);
        displaySprite.drawString(ipLine, cx, by + lineH + gap);

        displaySprite.setFont(&RobotoMono_SemiBold14pt7b);
        displaySprite.pushSprite(0, 0);
        return;
    }

    displaySprite.setFont(&RobotoMono_SemiBold14pt7b);

    bool altMetric = d.altMetric;
    bool spdMetric = d.spdMetric;
    const char *altUnit = d.altUnit;
    const char *spdUnit = d.spdUnit;

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", d.baroAlt);
    drawRow(14, "ALT", buf, altUnit);

    snprintf(buf, sizeof(buf), "%.1f", d.radarAlt);
    float raltThreshold = altMetric ? 300.0f : 1000.0f;
    if (d.radarAlt < raltThreshold)
    {
        int vRight = kLayoutRight - displaySprite.textWidth(altUnit) - kLayoutGap;
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kLabelColor, TFT_BLACK);
        displaySprite.drawString("RALT", kLayoutLeft, 40);
        displaySprite.setTextDatum(textdatum_t::middle_right);
        displaySprite.setTextColor(kWarnColor, TFT_BLACK);
        displaySprite.drawString(buf, vRight, 40);
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kUnitColor, TFT_BLACK);
        displaySprite.drawString(altUnit, vRight + kLayoutGap, 40);
    }
    else
    {
        drawRow(40, "RALT", buf, altUnit);
    }

    snprintf(buf, sizeof(buf), "%.1f", d.ias);
    float iasLowThreshold = spdMetric ? 550.0f : 300.0f;
    if (d.ias < iasLowThreshold)
    {
        int vRight = kLayoutRight - displaySprite.textWidth(spdUnit) - kLayoutGap;
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kLabelColor, TFT_BLACK);
        displaySprite.drawString("IAS", kLayoutLeft, 66);
        displaySprite.setTextDatum(textdatum_t::middle_right);
        displaySprite.setTextColor(kWarnColor, TFT_BLACK);
        displaySprite.drawString(buf, vRight, 66);
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kUnitColor, TFT_BLACK);
        displaySprite.drawString(spdUnit, vRight + kLayoutGap, 66);
    }
    else
    {
        drawRow(66, "IAS", buf, spdUnit);
    }

    snprintf(buf, sizeof(buf), "%.1f", d.tas);
    drawRow(92, "TAS", buf, spdUnit);

    snprintf(buf, sizeof(buf), "%.1f", d.vs);
    drawRow(118, "V/S", buf, d.vsUnit);

    snprintf(buf, sizeof(buf), "%.3f", d.mach);
    {
        uint16_t color = (d.mach > 1.0f) ? kWarnColor : kMachColor;
        int vRight = kLayoutRight - displaySprite.textWidth("M") - kLayoutGap;
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kLabelColor, TFT_BLACK);
        displaySprite.drawString("MACH", kLayoutLeft, 144);
        displaySprite.setTextDatum(textdatum_t::middle_right);
        displaySprite.setTextColor(color, TFT_BLACK);
        displaySprite.drawString(buf, vRight, 144);
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kUnitColor, TFT_BLACK);
        displaySprite.drawString("M", vRight + kLayoutGap, 144);
    }

    snprintf(buf, sizeof(buf), "%.2f", d.gForce);
    if (d.gForce > 6.0f)
    {
        int vRight = kLayoutRight - displaySprite.textWidth("G") - kLayoutGap;
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kLabelColor, TFT_BLACK);
        displaySprite.drawString("G FORCE", kLayoutLeft, 170);
        displaySprite.setTextDatum(textdatum_t::middle_right);
        displaySprite.setTextColor(kWarnColor, TFT_BLACK);
        displaySprite.drawString(buf, vRight, 170);
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kUnitColor, TFT_BLACK);
        displaySprite.drawString("G", vRight + kLayoutGap, 170);
    }
    else
    {
        drawRow(170, "G FORCE", buf, "G");
    }

    float hdg = fmodf(d.heading, 360.0f);
    if (hdg < 0) hdg += 360.0f;
    snprintf(buf, sizeof(buf), "%03.0f", hdg);
    drawRow(196, "HDG", buf, "DEG");

    float mhdg = fmodf(d.mhdg, 360.0f);
    if (mhdg < 0) mhdg += 360.0f;
    snprintf(buf, sizeof(buf), "%03.0f", mhdg);
    drawRow(222, "MHDG", buf, "DEG");

    snprintf(buf, sizeof(buf), "%.2f", d.aoa);
    drawRow(248, "AOA", buf, "DEG");

    snprintf(buf, sizeof(buf), "%.0f", d.chaff);
    if (d.chaff < 10.0f)
    {
        int vRight = kLayoutRight - kLayoutGap;
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kLabelColor, TFT_BLACK);
        displaySprite.drawString("CHAF", kLayoutLeft, 274);
        displaySprite.setTextDatum(textdatum_t::middle_right);
        displaySprite.setTextColor(kWarnColor, TFT_BLACK);
        displaySprite.drawString(buf, vRight, 274);
    }
    else
    {
        drawRow(274, "CHAF", buf, "");
    }

    snprintf(buf, sizeof(buf), "%.0f", d.flare);
    if (d.flare < 10.0f)
    {
        int vRight = kLayoutRight - kLayoutGap;
        displaySprite.setTextDatum(textdatum_t::middle_left);
        displaySprite.setTextColor(kLabelColor, TFT_BLACK);
        displaySprite.drawString("FLAR", kLayoutLeft, 300);
        displaySprite.setTextDatum(textdatum_t::middle_right);
        displaySprite.setTextColor(kWarnColor, TFT_BLACK);
        displaySprite.drawString(buf, vRight, 300);
    }
    else
    {
        drawRow(300, "FLAR", buf, "");
    }

    displaySprite.pushSprite(0, 0);
}

static void displayTask(void *)
{
    while (true)
    {
        drawStatusScreen();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup()
{
    initScreen();
    initDisplaySprite();
    displaySprite.setFont(&RobotoMono_SemiBold14pt7b);
    dcsDataMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(networkTask, "network", 16384, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(displayTask, "display", 16384, nullptr, 2, nullptr, 1);
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
