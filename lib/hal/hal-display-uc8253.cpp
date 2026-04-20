// hal-display-uc8253.cpp — UC8253 driver implementation for GDEY037T03 (416×240).
// Init sequences verbatim from Good-Display vendor sample Display_EPD_W21.cpp
// (A32-GDEY037T03-FP4G-20240815). OTP handles PSR/PWR/BTST/PLL/TRES/LUT defaults —
// only PON + CDI (+ CCSET/TSSET for fast variants) are required.

#ifdef BOARD_MURPHY

#include "hal-display-uc8253.h"

#include <Logging.h>
#include <string.h>

#include "hal-display-uc8253-spi.h"

// ── helpers ──────────────────────────────────────────────────────────────────

static inline void cmd(uint8_t c)                    { UC8253Spi::writeCmd(c); }
static inline void dat(uint8_t d)                    { UC8253Spi::writeData(d); }
static inline void dat(const uint8_t* d, size_t n)  { UC8253Spi::writeData(d, n); }
static inline void rst()                             { UC8253Spi::resetPulse(); }
static inline void busy()                            { UC8253Spi::waitBusy(); }

// ── construction ─────────────────────────────────────────────────────────────

HalDisplayUC8253::HalDisplayUC8253() {
  memset(frameBuffer_, 0xFF, sizeof(frameBuffer_));
  memset(oldFrame_,    0xFF, sizeof(oldFrame_));
  memset(grayLsb_,     0xFF, sizeof(grayLsb_));
  memset(grayMsb_,     0xFF, sizeof(grayMsb_));
}

// ── init sequences (ported verbatim from vendor EPD_Init* functions) ─────────

void HalDisplayUC8253::initFull() {
  rst();
  cmd(CMD_PON);  // Power ON
  busy();
  cmd(CMD_CDI); dat(0x97);  // VBDW mode: VBD=1001, DDX=01, CDI=0111
}

void HalDisplayUC8253::initFast() {
  rst();
  cmd(CMD_PON); busy();
  cmd(CMD_CCSET); dat(0x02);  // Cascade: special mode
  cmd(CMD_TSSET); dat(0x5F);  // Forced temp → selects ~1.5 s OTP waveform
}

void HalDisplayUC8253::initPartial() {
  rst();
  cmd(CMD_PON); busy();
  cmd(CMD_CCSET); dat(0x02);
  cmd(CMD_TSSET); dat(0x6E);  // Forced temp → selects partial OTP waveform (~0.3 s)
  cmd(CMD_CDI);   dat(0xD7);  // VBDF mode for partial
}

void HalDisplayUC8253::initFourGray() {
  rst();
  cmd(CMD_PSR); dat(0x1F);    // PSR: LUT_REG=0(OTP), KW=1, UD=1, SHL=1, SHD_N=1, RST_N=1
  cmd(CMD_PON); busy();
  cmd(CMD_CCSET); dat(0x02);
  cmd(CMD_TSSET); dat(0x5A);  // Forced temp → selects 4-gray OTP waveform
}

// ── frame transfer ────────────────────────────────────────────────────────────

void HalDisplayUC8253::writeFullFrame(const uint8_t* oldFrame, const uint8_t* newFrame) {
  // DTM1 (0x10) = OLD frame for ghost suppression
  cmd(CMD_DTM1);
  dat(oldFrame, FRAME_BYTES);

  // DTM2 (0x13) = NEW frame (the image to display)
  cmd(CMD_DTM2);
  dat(newFrame, FRAME_BYTES);
}

void HalDisplayUC8253::triggerRefreshAndPowerOff() {
  cmd(CMD_DRF);   // Display Refresh — ≥1 ms setup required before BUSY poll
  delay(1);       // vendor spec: 200 µs minimum; 1 ms is safe
  busy();
  cmd(CMD_POF); busy();  // Power OFF: mandatory after each refresh per vendor note
}

// ── 4-gray encoder ────────────────────────────────────────────────────────────
// Input: 24 960-byte 2bpp buffer (2 pixels per byte pair, MSB-left).
// Encoding table (from vendor EPD_WhiteScreen_ALL_4G):
//   2-bit input | color  | DTM1 bit | DTM2 bit
//   11 (0xC0)   | white  |    1     |    1
//   10 (0x80)   | gray1  |    1     |    0
//   01 (0x40)   | gray2  |    0     |    1
//   00 (0x00)   | black  |    0     |    0
void HalDisplayUC8253::writeGrayFrames(const uint8_t* grayBuf) {
  // Build and send DTM1 (old/lsb plane) byte by byte to avoid 24 KB temp buffer.
  cmd(CMD_DTM1);
  for (uint32_t i = 0; i < FRAME_BYTES; i++) {
    uint8_t out = 0;
    // Each output byte is built from 2 consecutive input bytes (8 pixels total).
    for (uint8_t j = 0; j < 2; j++) {
      uint8_t src = grayBuf[i * 2 + j];
      for (uint8_t k = 0; k < 4; k++) {
        uint8_t px = src & 0xC0;
        // DTM1 bit: 1 for white or gray1, 0 for gray2 or black
        out |= ((px == 0xC0 || px == 0x80) ? 1 : 0);
        if (!(j == 1 && k == 3)) { src <<= 2; out <<= 1; }
      }
    }
    dat(out);
  }

  // Build and send DTM2 (new/msb plane).
  cmd(CMD_DTM2);
  for (uint32_t i = 0; i < FRAME_BYTES; i++) {
    uint8_t out = 0;
    for (uint8_t j = 0; j < 2; j++) {
      uint8_t src = grayBuf[i * 2 + j];
      for (uint8_t k = 0; k < 4; k++) {
        uint8_t px = src & 0xC0;
        // DTM2 bit: 1 for white or gray2, 0 for gray1 or black
        out |= ((px == 0xC0 || px == 0x40) ? 1 : 0);
        if (!(j == 1 && k == 3)) { src <<= 2; out <<= 1; }
      }
    }
    dat(out);
  }
}

// ── HalDisplayDriver public API ───────────────────────────────────────────────

void HalDisplayUC8253::begin() {
  UC8253Spi::begin();
  LOG_INF("UC8253", "begin: GDEY037T03 %ux%u", PANEL_W, PANEL_H);
  initFull();
}

void HalDisplayUC8253::clearScreen(uint8_t color) {
  memset(frameBuffer_, color, sizeof(frameBuffer_));
  displayBuffer(RefreshMode::FULL_REFRESH, true);
}

void HalDisplayUC8253::drawImage(const uint8_t* data, uint16_t x, uint16_t y,
                                  uint16_t w, uint16_t h, bool fromProgmem) {
  // Blit tile into frame buffer (1bpp, MSB-left, row-major).
  const uint16_t wBytes = w / 8;
  for (uint16_t row = 0; row < h; row++) {
    const uint8_t* src = data + row * wBytes;
    uint8_t* dst = frameBuffer_ + (y + row) * (PANEL_W / 8) + x / 8;
    if (fromProgmem) {
      for (uint16_t b = 0; b < wBytes; b++) dst[b] = pgm_read_byte(src + b);
    } else {
      memcpy(dst, src, wBytes);
    }
  }
}

void HalDisplayUC8253::drawImageTransparent(const uint8_t* data, uint16_t x, uint16_t y,
                                             uint16_t w, uint16_t h, bool fromProgmem) {
  const uint16_t wBytes = w / 8;
  for (uint16_t row = 0; row < h; row++) {
    const uint8_t* src = data + row * wBytes;
    uint8_t* dst = frameBuffer_ + (y + row) * (PANEL_W / 8) + x / 8;
    for (uint16_t b = 0; b < wBytes; b++) {
      const uint8_t byte = fromProgmem ? pgm_read_byte(src + b) : src[b];
      dst[b] &= byte;  // white pixels (0xFF) are transparent; black overwrites
    }
  }
}

void HalDisplayUC8253::displayBuffer(RefreshMode mode, bool turnOffScreen) {
  switch (mode) {
    case RefreshMode::FULL_REFRESH:
      initFull();
      break;
    case RefreshMode::HALF_REFRESH:
      // HALF_REFRESH maps to fast on UC8253 (no separate half-refresh waveform)
      initFast();
      break;
    case RefreshMode::FAST_REFRESH:
      initFast();
      break;
  }
  writeFullFrame(oldFrame_, frameBuffer_);
  memcpy(oldFrame_, frameBuffer_, FRAME_BYTES);
  triggerRefreshAndPowerOff();
  if (!turnOffScreen) {
    // Re-init to standby so panel is ready for next write without full power-on.
    initFull();
  }
}

void HalDisplayUC8253::refreshDisplay(RefreshMode mode, bool turnOffScreen) {
  // Alias — same semantics as displayBuffer for UC8253.
  displayBuffer(mode, turnOffScreen);
}

void HalDisplayUC8253::deepSleep() {
  cmd(CMD_POF); busy();   // Power OFF
  cmd(CMD_DSLP); dat(0xA5);  // Deep Sleep — 0xA5 is the UC8253 check code
  LOG_INF("UC8253", "deepSleep entered");
}

// 4-gray buffer management ─────────────────────────────────────────────────────

void HalDisplayUC8253::copyGrayscaleBuffers(const uint8_t* lsbBuf, const uint8_t* msbBuf) {
  memcpy(grayLsb_, lsbBuf, FRAME_BYTES);
  memcpy(grayMsb_, msbBuf, FRAME_BYTES);
}

void HalDisplayUC8253::copyGrayscaleLsbBuffers(const uint8_t* lsbBuf) {
  memcpy(grayLsb_, lsbBuf, FRAME_BYTES);
}

void HalDisplayUC8253::copyGrayscaleMsbBuffers(const uint8_t* msbBuf) {
  memcpy(grayMsb_, msbBuf, FRAME_BYTES);
}

void HalDisplayUC8253::cleanupGrayscaleBuffers(const uint8_t* bwBuf) {
  // After 4-gray display, restore a clean 1bpp frame so subsequent B/W ops work.
  if (bwBuf) {
    memcpy(frameBuffer_, bwBuf, FRAME_BYTES);
    memcpy(oldFrame_,    bwBuf, FRAME_BYTES);
  }
  memset(grayLsb_, 0xFF, FRAME_BYTES);
  memset(grayMsb_, 0xFF, FRAME_BYTES);
}

void HalDisplayUC8253::displayGrayBuffer(bool turnOffScreen) {
  // Build combined 2bpp buffer from LSB + MSB planes for the vendor 4-gray encoder.
  // 24 960 bytes = 2 × FRAME_BYTES — allocated on stack is too large; use static.
  static uint8_t grayBuf[FRAME_BYTES * 2];
  for (uint32_t i = 0; i < FRAME_BYTES; i++) {
    // Pack 4 pixels from lsb byte + 4 pixels from msb byte into 8 2bpp pixels.
    // Pixel n: bit (7-n) of lsb → bit1, bit (7-n) of msb → bit0.
    for (uint8_t bit = 0; bit < 4; bit++) {
      const uint8_t lbit = (grayLsb_[i] >> (7 - bit)) & 1;
      const uint8_t mbit = (grayMsb_[i] >> (7 - bit)) & 1;
      grayBuf[i * 2 + 0] |= ((lbit << 1 | mbit) << (6 - bit * 2));
    }
    for (uint8_t bit = 0; bit < 4; bit++) {
      const uint8_t lbit = (grayLsb_[i] >> (3 - bit)) & 1;
      const uint8_t mbit = (grayMsb_[i] >> (3 - bit)) & 1;
      grayBuf[i * 2 + 1] |= ((lbit << 1 | mbit) << (6 - bit * 2));
    }
  }

  initFourGray();
  writeGrayFrames(grayBuf);
  triggerRefreshAndPowerOff();
  if (!turnOffScreen) {
    initFull();
  }
}

#endif  // BOARD_MURPHY
