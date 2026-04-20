// hal-display-uc8253-spi.cpp — SPI + GPIO I/O implementation for UC8253.
// Vendor sample used bit-banged CS+DC per byte; we do the same via Arduino
// digitalWrite so the logic is direct and auditable against Display_EPD_W21_spi.cpp.

#ifdef BOARD_MURPHY

#include "hal-display-uc8253-spi.h"

#include <Logging.h>

namespace UC8253Spi {

static const SPISettings kSpiSettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0);

void begin() {
  // Configure control GPIOs before starting the SPI bus.
  pinMode(EPD_DC,   OUTPUT);
  pinMode(EPD_CS,   OUTPUT);
  pinMode(EPD_RST,  OUTPUT);
  pinMode(EPD_BUSY, INPUT);

  // Deassert CS; DC defaults to command (LOW).
  digitalWrite(EPD_CS, HIGH);
  digitalWrite(EPD_DC, LOW);
  digitalWrite(EPD_RST, HIGH);

  // Initialise HW SPI with Murphy's pin map.
  SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI, EPD_CS);
}

void writeCmd(uint8_t cmd) {
  digitalWrite(EPD_DC, LOW);   // DC=LOW → command
  digitalWrite(EPD_CS, LOW);
  SPI.beginTransaction(kSpiSettings);
  SPI.transfer(cmd);
  SPI.endTransaction();
  digitalWrite(EPD_CS, HIGH);
}

void writeData(const uint8_t* data, size_t len) {
  if (!data || len == 0) return;
  digitalWrite(EPD_DC, HIGH);  // DC=HIGH → data
  digitalWrite(EPD_CS, LOW);
  SPI.beginTransaction(kSpiSettings);
  // transfer() accepts non-const pointer (Arduino API limitation); cast is safe.
  SPI.writeBytes(data, len);
  SPI.endTransaction();
  digitalWrite(EPD_CS, HIGH);
}

void resetPulse() {
  // Vendor spec: RST LOW ≥10 ms, then HIGH ≥10 ms.
  digitalWrite(EPD_RST, LOW);
  delay(10);
  digitalWrite(EPD_RST, HIGH);
  delay(10);
}

void waitBusy(uint32_t timeoutMs) {
  // UC8253 BUSY is active-LOW: LOW=busy, HIGH=ready.
  const uint32_t deadline = millis() + timeoutMs;
  while (digitalRead(EPD_BUSY) == LOW) {
    if (millis() > deadline) {
      LOG_ERR("UC8253", "waitBusy timeout after %u ms — aborting", timeoutMs);
      return;
    }
    delay(1);
  }
}

}  // namespace UC8253Spi

#endif  // BOARD_MURPHY
