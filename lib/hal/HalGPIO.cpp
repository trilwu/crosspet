#include <HalGPIO.h>
#include <SPI.h>

void HalGPIO::begin() {
  inputMgr.begin();
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);
  pinMode(UART0_RXD, INPUT);
}

void HalGPIO::update() {
  inputMgr.update();

  // Detect BLE button press/release edges
  uint8_t currentBle = bleState;
  bleWasPressed = currentBle & ~prevBleState;   // newly pressed bits
  bleWasReleased = prevBleState & ~currentBle;  // newly released bits
  prevBleState = currentBle;
}

bool HalGPIO::isPressed(uint8_t buttonIndex) const {
  return inputMgr.isPressed(buttonIndex) || (prevBleState & (1 << buttonIndex));
}

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
  return inputMgr.wasPressed(buttonIndex) || (bleWasPressed & (1 << buttonIndex));
}

bool HalGPIO::wasAnyPressed() const { return inputMgr.wasAnyPressed() || (bleWasPressed != 0); }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
  return inputMgr.wasReleased(buttonIndex) || (bleWasReleased & (1 << buttonIndex));
}

bool HalGPIO::wasAnyReleased() const { return inputMgr.wasAnyReleased() || (bleWasReleased != 0); }

unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }

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