#include <HalGPIO.h>
#include <SPI.h>

void HalGPIO::begin() {
  inputMgr.begin();
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);
  pinMode(UART0_RXD, INPUT);
}

void HalGPIO::update() {
  // Save previous virtual button state for release detection
  previousVirtualButtonEvents = virtualButtonEvents;

  // Promote queued virtual buttons to current frame, then clear queue.
  // This gives activities exactly one frame to see wasPressed() before auto-release.
  virtualButtonEvents = virtualButtonQueue;
  virtualButtonQueue = 0;

  inputMgr.update();
}

bool HalGPIO::isPressed(uint8_t buttonIndex) const { return inputMgr.isPressed(buttonIndex); }

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
  return inputMgr.wasPressed(buttonIndex) || (virtualButtonEvents & (1 << buttonIndex));
}

bool HalGPIO::wasAnyPressed() const { return inputMgr.wasAnyPressed() || (virtualButtonEvents > 0); }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
  // Virtual release = was pressed last frame but not this frame
  const uint8_t virtualRelease = previousVirtualButtonEvents & ~virtualButtonEvents;
  return inputMgr.wasReleased(buttonIndex) || (virtualRelease & (1 << buttonIndex));
}

bool HalGPIO::wasAnyReleased() const {
  const uint8_t virtualRelease = previousVirtualButtonEvents & ~virtualButtonEvents;
  return inputMgr.wasAnyReleased() || (virtualRelease > 0);
}

unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }

void HalGPIO::injectButtonPress(uint8_t buttonIndex) {
  // Queue the button for next update() cycle — one-shot pulse
  virtualButtonQueue |= (1 << buttonIndex);
}

bool HalGPIO::isUsbConnected() const {
  // U0RXD/GPIO20 reads HIGH when USB is connected
  return digitalRead(UART0_RXD) == HIGH;
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
  const bool usbConnected = isUsbConnected();
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  if ((wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected) ||
      (wakeupCause == ESP_SLEEP_WAKEUP_GPIO && resetReason == ESP_RST_DEEPSLEEP && usbConnected)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
}
