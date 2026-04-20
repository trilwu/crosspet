#pragma once
// hal-display-ssd1677-adapter.h — wraps the X4's EInkDisplay (SSD1677) behind HalDisplayDriver.
// Only compiled when BOARD_MURPHY is NOT defined (X4 build path).
// Activities never include this; HalDisplay.cpp selects it at compile time.

#ifndef BOARD_MURPHY

#include <EInkDisplay.h>

#include "hal-display-driver.h"

class HalDisplaySSD1677Adapter final : public HalDisplayDriver {
 public:
  // Pin constructor — mirrors EInkDisplay(sclk, mosi, cs, dc, rst, busy)
  HalDisplaySSD1677Adapter(uint8_t sclk, uint8_t mosi, uint8_t cs, uint8_t dc, uint8_t rst, uint8_t busy)
      : eink_(sclk, mosi, cs, dc, rst, busy) {}

  void begin() override { eink_.begin(); }

  uint16_t getWidth() const override { return EInkDisplay::DISPLAY_WIDTH; }
  uint16_t getHeight() const override { return EInkDisplay::DISPLAY_HEIGHT; }

  uint8_t* getFrameBuffer() override { return eink_.getFrameBuffer(); }

  void clearScreen(uint8_t color) override { eink_.clearScreen(color); }

  void drawImage(const uint8_t* data, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool fromProgmem) override {
    eink_.drawImage(data, x, y, w, h, fromProgmem);
  }

  void drawImageTransparent(const uint8_t* data, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            bool fromProgmem) override {
    eink_.drawImageTransparent(data, x, y, w, h, fromProgmem);
  }

  void displayBuffer(RefreshMode mode, bool turnOffScreen) override {
    eink_.displayBuffer(toEinkMode(mode), turnOffScreen);
  }

  void refreshDisplay(RefreshMode mode, bool turnOffScreen) override {
    eink_.refreshDisplay(toEinkMode(mode), turnOffScreen);
  }

  void deepSleep() override { eink_.deepSleep(); }

  void copyGrayscaleBuffers(const uint8_t* lsb, const uint8_t* msb) override {
    eink_.copyGrayscaleBuffers(lsb, msb);
  }

  void copyGrayscaleLsbBuffers(const uint8_t* lsb) override { eink_.copyGrayscaleLsbBuffers(lsb); }
  void copyGrayscaleMsbBuffers(const uint8_t* msb) override { eink_.copyGrayscaleMsbBuffers(msb); }
  void cleanupGrayscaleBuffers(const uint8_t* bw) override { eink_.cleanupGrayscaleBuffers(bw); }

  void displayGrayBuffer(bool turnOffScreen) override { eink_.displayGrayBuffer(turnOffScreen); }

  // X4-specific: notify driver of X3 panel variant (called before begin()).
  void setDisplayX3() { eink_.setDisplayX3(); }

  // X4-specific: request a full resync on next displayBuffer() call.
  void requestResync(int count = 0) { eink_.requestResync(count); }

 private:
  static EInkDisplay::RefreshMode toEinkMode(RefreshMode m) {
    switch (m) {
      case RefreshMode::FULL_REFRESH: return EInkDisplay::FULL_REFRESH;
      case RefreshMode::HALF_REFRESH: return EInkDisplay::HALF_REFRESH;
      case RefreshMode::FAST_REFRESH: return EInkDisplay::FAST_REFRESH;
      default:                        return EInkDisplay::FAST_REFRESH;
    }
  }

  EInkDisplay eink_;  // owns the SSD1677 driver
};

#endif  // !BOARD_MURPHY
