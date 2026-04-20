#pragma once
// hal-display-driver.h — pure-virtual interface for all e-ink display backends.
// Implemented by: hal-display-ssd1677-adapter (X4) and hal-display-uc8253 (Murphy).
// HalDisplay is the sole consumer; activities never include this header directly.

#include <Arduino.h>

// Geometry defaults for X4 (800×480); Murphy overrides via -DPANEL_WIDTH / -DPANEL_HEIGHT.
#ifndef PANEL_WIDTH
#define PANEL_WIDTH 800
#endif
#ifndef PANEL_HEIGHT
#define PANEL_HEIGHT 480
#endif

class HalDisplayDriver {
 public:
  // Refresh modes — same semantics as HalDisplay::RefreshMode (must stay in sync).
  enum class RefreshMode : uint8_t {
    FULL_REFRESH,
    HALF_REFRESH,
    FAST_REFRESH,
  };

  virtual ~HalDisplayDriver() = default;

  // Initialize hardware and load firmware defaults.
  virtual void begin() = 0;

  // Panel geometry (may differ per variant).
  virtual uint16_t getWidth() const = 0;
  virtual uint16_t getHeight() const = 0;

  // Frame buffer (driver owns allocation; caller gets raw pointer).
  virtual uint8_t* getFrameBuffer() = 0;

  // Fill frame buffer with `color` and refresh.
  virtual void clearScreen(uint8_t color = 0xFF) = 0;

  // Copy an image tile into the frame buffer at (x, y).
  virtual void drawImage(const uint8_t* data, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                         bool fromProgmem = false) = 0;

  // Same as drawImage but skips 0xFF (white) pixels (transparency).
  virtual void drawImageTransparent(const uint8_t* data, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                    bool fromProgmem = false) = 0;

  // Push frame buffer to panel and optionally power down afterwards.
  virtual void displayBuffer(RefreshMode mode, bool turnOffScreen) = 0;
  virtual void refreshDisplay(RefreshMode mode, bool turnOffScreen) = 0;

  // Power off the display panel (deep sleep).
  virtual void deepSleep() = 0;

  // 4-gray: accept two 1-bpp planes (lsb, msb) and composite into driver internal buffer.
  virtual void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) = 0;
  virtual void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) = 0;
  virtual void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) = 0;
  virtual void cleanupGrayscaleBuffers(const uint8_t* bwBuffer) = 0;

  // Flush the internal 4-gray composite buffer to the panel.
  virtual void displayGrayBuffer(bool turnOffScreen) = 0;
};
