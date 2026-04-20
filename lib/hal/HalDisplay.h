#pragma once
// HalDisplay.h — public facade for e-ink display operations.
// Variant selected at compile time: X4 (SSD1677) or Murphy (UC8253) via BOARD_MURPHY.
// All callers (activities, GfxRenderer, etc.) include only this header — never the
// variant-specific driver headers.

#include <Arduino.h>
#include <memory>

#include "hal-display-driver.h"

class HalDisplay {
 public:
  // Refresh modes — kept in sync with HalDisplayDriver::RefreshMode.
  enum RefreshMode {
    FULL_REFRESH,
    HALF_REFRESH,
    FAST_REFRESH,
  };

  HalDisplay();
  ~HalDisplay();

  // Initialize the display hardware and selected driver.
  void begin();

  // Panel geometry — sourced from PANEL_WIDTH/PANEL_HEIGHT build flags (not hard-coded).
  // Defaults: 800/480 (X4) when flags absent; 416/240 when -DBOARD_MURPHY.
  static constexpr uint16_t DISPLAY_WIDTH       = PANEL_WIDTH;
  static constexpr uint16_t DISPLAY_HEIGHT      = PANEL_HEIGHT;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE         = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;

  // Frame buffer operations
  void clearScreen(uint8_t color = 0xFF) const;
  void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool fromProgmem = false) const;
  void drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w,
                            uint16_t h, bool fromProgmem = false) const;

  void displayBuffer(RefreshMode mode = RefreshMode::FAST_REFRESH, bool turnOffScreen = false);
  void refreshDisplay(RefreshMode mode = RefreshMode::FAST_REFRESH, bool turnOffScreen = false);

  // Power management
  void deepSleep();

  // Access to frame buffer (owned by driver)
  uint8_t* getFrameBuffer() const;

  // 4-gray grayscale buffer management
  void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer);
  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer);
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer);
  void cleanupGrayscaleBuffers(const uint8_t* bwBuffer);
  void displayGrayBuffer(bool turnOffScreen = false);

  // Runtime geometry passthrough (returns driver's actual values)
  uint16_t getDisplayWidth() const;
  uint16_t getDisplayHeight() const;
  uint16_t getDisplayWidthBytes() const;
  uint32_t getBufferSize() const;

 private:
  std::unique_ptr<HalDisplayDriver> driver_;

  // Convert HalDisplay::RefreshMode to HalDisplayDriver::RefreshMode.
  static HalDisplayDriver::RefreshMode toDriverMode(RefreshMode mode);
};

extern HalDisplay display;
