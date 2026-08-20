#pragma once

#include <LovyanGFX.hpp>

class LGFX_S3_ILI9488 : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 _panel;
    lgfx::Bus_SPI _bus;

public:
    LGFX_S3_ILI9488()
    {
        {
            auto cfg = _bus.config();
            cfg.spi_host = SPI3_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 25000000;
            cfg.pin_sclk = 12;
            cfg.pin_mosi = 11;
            cfg.pin_miso = -1;
            cfg.pin_dc = 2;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.use_lock = true;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.rgb_order = 0;
            cfg.pin_cs = 10;
            cfg.pin_rst = 4;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_rotation = 1;
            _panel.config(cfg);
        }
        setPanel(&_panel);
    }
};

inline LGFX_S3_ILI9488 displayDevice;
inline LGFX_Sprite displaySprite(&displayDevice);

inline void initScreen()
{
    displayDevice.init();
    displayDevice.invertDisplay(true);
    displayDevice.fillScreen(TFT_BLACK);
    displayDevice.startWrite();
}

inline void initDisplaySprite()
{
    displaySprite.setPsram(true);
    displaySprite.setColorDepth(16);
    displaySprite.createSprite(480, 320);
    displaySprite.fillSprite(TFT_BLACK);
    displaySprite.setSwapBytes(false);
}
