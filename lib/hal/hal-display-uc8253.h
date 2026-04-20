#pragma once
// hal-display-uc8253.h — HalDisplayDriver implementation for UC8253 (GDEY037T03).
// Panel: 416×240 B/W e-ink. Init sequences ported from Good-Display vendor sample
// (A32-GDEY037T03-FP4G-20240815). OTP-driven: no LUT upload needed for stock quality.
// Only compiled under -DBOARD_MURPHY.

#ifdef BOARD_MURPHY

#include <Arduino.h>

#include "hal-display-driver.h"

class HalDisplayUC8253 final : public HalDisplayDriver {
 public:
  // Panel geometry — 416 wide × 240 tall (landscape, UC8253 native orientation).
  static constexpr uint16_t PANEL_W    = PANEL_WIDTH;
  static constexpr uint16_t PANEL_H    = PANEL_HEIGHT;
  static constexpr uint32_t FRAME_BYTES = (uint32_t)PANEL_W * PANEL_H / 8;  // 12 480

  HalDisplayUC8253();
  ~HalDisplayUC8253() override = default;

  // HalDisplayDriver interface
  void begin() override;
  uint16_t getWidth()  const override { return PANEL_W; }
  uint16_t getHeight() const override { return PANEL_H; }
  uint8_t* getFrameBuffer() override  { return frameBuffer_; }

  void clearScreen(uint8_t color = 0xFF) override;
  void drawImage(const uint8_t* data, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool fromProgmem = false) override;
  void drawImageTransparent(const uint8_t* data, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            bool fromProgmem = false) override;

  void displayBuffer(RefreshMode mode, bool turnOffScreen) override;
  void refreshDisplay(RefreshMode mode, bool turnOffScreen) override;
  void deepSleep() override;

  void copyGrayscaleBuffers(const uint8_t* lsbBuf, const uint8_t* msbBuf) override;
  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuf) override;
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuf) override;
  void cleanupGrayscaleBuffers(const uint8_t* bwBuf) override;
  void displayGrayBuffer(bool turnOffScreen) override;

 private:
  // UC8253 register commands (UC8253 datasheet §6 + vendor sample cross-reference)
  static constexpr uint8_t CMD_PSR   = 0x00;  // Panel Setting Register
  static constexpr uint8_t CMD_PON   = 0x04;  // Power ON
  static constexpr uint8_t CMD_POF   = 0x02;  // Power OFF
  static constexpr uint8_t CMD_DSLP  = 0x07;  // Deep Sleep
  static constexpr uint8_t CMD_DTM1  = 0x10;  // Data Transmission 1 (OLD frame)
  static constexpr uint8_t CMD_DRF   = 0x12;  // Display Refresh
  static constexpr uint8_t CMD_DTM2  = 0x13;  // Data Transmission 2 (NEW frame)
  static constexpr uint8_t CMD_CDI   = 0x50;  // VCOM and Data Interval
  static constexpr uint8_t CMD_PTIN  = 0x91;  // Partial mode IN
  static constexpr uint8_t CMD_PTL   = 0x90;  // Partial window setting
  static constexpr uint8_t CMD_CCSET = 0xE0;  // Cascade Setting (fast/partial/4G modes)
  static constexpr uint8_t CMD_TSSET = 0xE5;  // Temperature Sensor Setting (LUT select)

  // Init helpers (named after vendor functions)
  void initFull();
  void initFast();
  void initPartial();
  void initFourGray();

  // Core transfer helpers
  void writeFullFrame(const uint8_t* oldFrame, const uint8_t* newFrame);
  void triggerRefreshAndPowerOff();

  // Encode 4-gray 2bpp input into two 1bpp bitplanes (DTM1 + DTM2).
  // Input: 24 960-byte buffer (2 bpp × 416 × 240).
  // Output: written directly to panel via SPI (no extra heap allocation).
  void writeGrayFrames(const uint8_t* grayBuf);

  // Frame buffers
  uint8_t frameBuffer_[FRAME_BYTES];  // current (new) 1bpp frame
  uint8_t oldFrame_[FRAME_BYTES];     // previous frame — fed to DTM1 for ghost suppression

  // 4-gray compositing planes (each 12 480 bytes)
  uint8_t grayLsb_[FRAME_BYTES];  // LSB plane (populated by copyGrayscaleLsbBuffers)
  uint8_t grayMsb_[FRAME_BYTES];  // MSB plane (populated by copyGrayscaleMsbBuffers)
};

#endif  // BOARD_MURPHY
