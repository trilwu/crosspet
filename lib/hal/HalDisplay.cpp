// HalDisplay.cpp — selects the correct display driver at compile time and
// forwards all public calls through the HalDisplayDriver interface.
// X4 (default): SSD1677 via EInkDisplay symlink wrapped in HalDisplaySSD1677Adapter.
// Murphy (-DBOARD_MURPHY): UC8253 native driver.

#include "HalDisplay.h"

#include "HalGPIO.h"

#ifdef BOARD_MURPHY
#  include "hal-display-uc8253.h"
#else
#  include "hal-display-ssd1677-adapter.h"
#endif

// Global singleton
HalDisplay display;

// ── construction ─────────────────────────────────────────────────────────────

HalDisplay::HalDisplay() {
#ifdef BOARD_MURPHY
  driver_ = std::make_unique<HalDisplayUC8253>();
#else
  driver_ = std::make_unique<HalDisplaySSD1677Adapter>(
      EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
#endif
}

HalDisplay::~HalDisplay() = default;

// ── mode conversion ───────────────────────────────────────────────────────────

HalDisplayDriver::RefreshMode HalDisplay::toDriverMode(RefreshMode mode) {
  switch (mode) {
    case FULL_REFRESH: return HalDisplayDriver::RefreshMode::FULL_REFRESH;
    case HALF_REFRESH: return HalDisplayDriver::RefreshMode::HALF_REFRESH;
    case FAST_REFRESH: return HalDisplayDriver::RefreshMode::FAST_REFRESH;
    default:           return HalDisplayDriver::RefreshMode::FAST_REFRESH;
  }
}

// ── lifecycle ─────────────────────────────────────────────────────────────────

void HalDisplay::begin() {
#ifndef BOARD_MURPHY
  // X4/X3 variant: handle device-specific init before driver begin().
  auto* ssd = static_cast<HalDisplaySSD1677Adapter*>(driver_.get());
  if (gpio.deviceIsX3()) {
    ssd->setDisplayX3();
  }
  driver_->begin();
  // Request resync after events that may leave display in unknown state.
  const auto wakeupReason = gpio.getWakeupReason();
  if (wakeupReason == HalGPIO::WakeupReason::PowerButton ||
      wakeupReason == HalGPIO::WakeupReason::AfterFlash  ||
      wakeupReason == HalGPIO::WakeupReason::Other) {
    ssd->requestResync();
  }
#else
  driver_->begin();
#endif
}

// ── frame buffer ops ──────────────────────────────────────────────────────────

void HalDisplay::clearScreen(uint8_t color) const {
  driver_->clearScreen(color);
}

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h, bool fromProgmem) const {
  driver_->drawImage(imageData, x, y, w, h, fromProgmem);
}

void HalDisplay::drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y,
                                      uint16_t w, uint16_t h, bool fromProgmem) const {
  driver_->drawImageTransparent(imageData, x, y, w, h, fromProgmem);
}

void HalDisplay::displayBuffer(RefreshMode mode, bool turnOffScreen) {
#ifndef BOARD_MURPHY
  // X3 half-refresh needs a resync signal before the update.
  if (gpio.deviceIsX3() && mode == HALF_REFRESH) {
    static_cast<HalDisplaySSD1677Adapter*>(driver_.get())->requestResync(1);
  }
#endif
  driver_->displayBuffer(toDriverMode(mode), turnOffScreen);
}

void HalDisplay::refreshDisplay(RefreshMode mode, bool turnOffScreen) {
#ifndef BOARD_MURPHY
  if (gpio.deviceIsX3() && mode == HALF_REFRESH) {
    static_cast<HalDisplaySSD1677Adapter*>(driver_.get())->requestResync(1);
  }
#endif
  driver_->refreshDisplay(toDriverMode(mode), turnOffScreen);
}

void HalDisplay::deepSleep() { driver_->deepSleep(); }

uint8_t* HalDisplay::getFrameBuffer() const { return driver_->getFrameBuffer(); }

// ── 4-gray ────────────────────────────────────────────────────────────────────

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsb, const uint8_t* msb) {
  driver_->copyGrayscaleBuffers(lsb, msb);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsb) {
  driver_->copyGrayscaleLsbBuffers(lsb);
}

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msb) {
  driver_->copyGrayscaleMsbBuffers(msb);
}

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bw) {
  driver_->cleanupGrayscaleBuffers(bw);
}

void HalDisplay::displayGrayBuffer(bool turnOffScreen) {
  driver_->displayGrayBuffer(turnOffScreen);
}

// ── geometry passthrough ──────────────────────────────────────────────────────

uint16_t HalDisplay::getDisplayWidth()      const { return driver_->getWidth(); }
uint16_t HalDisplay::getDisplayHeight()     const { return driver_->getHeight(); }
uint16_t HalDisplay::getDisplayWidthBytes() const { return driver_->getWidth() / 8; }
uint32_t HalDisplay::getBufferSize()        const {
  return static_cast<uint32_t>(driver_->getWidth() / 8) * driver_->getHeight();
}
