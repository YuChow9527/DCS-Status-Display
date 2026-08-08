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
      cfg.freq_write = 15000000;
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

inline LGFX_S3_ILI9488 thisTFT;
inline LGFX_Sprite default_screen(&thisTFT);

inline void initscreen()
{
  thisTFT.init();
  thisTFT.invertDisplay(true);
  thisTFT.fillScreen(TFT_BLACK);
  thisTFT.startWrite();
}

inline void initdefaultsprite()
{
  default_screen.setPsram(true);
  default_screen.setColorDepth(16);
  default_screen.createSprite(480, 320);
  default_screen.fillSprite(TFT_BLACK);
  default_screen.setSwapBytes(false);
}
