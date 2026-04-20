#pragma once
// hal-display-uc8253-spi.h — SPI + GPIO I/O primitives for the UC8253 e-ink controller.
// Handles CS/DC as bit-banged GPIO (matching vendor sample pattern) with Arduino HW SPI bus.
// Only compiled under -DBOARD_MURPHY.

#ifdef BOARD_MURPHY

#include <Arduino.h>
#include <SPI.h>

#include "hal-pinmap-murphy.h"

namespace UC8253Spi {

// SPI clock: 10 MHz (vendor default; safe across all OTP batches).
// Can be raised to 20 MHz after successful bench bring-up in phase 04.
static constexpr uint32_t SPI_CLOCK_HZ = 10000000UL;

// Initialise HW SPI bus and configure DC/CS/RST/BUSY GPIOs.
void begin();

// Assert CS LOW, transfer one command byte with DC=LOW, deassert CS HIGH.
void writeCmd(uint8_t cmd);

// Assert CS LOW, transfer `len` data bytes with DC=HIGH, deassert CS HIGH.
void writeData(const uint8_t* data, size_t len);

// Convenience: send a single data byte.
inline void writeData(uint8_t byte) { writeData(&byte, 1); }

// Pulse RST LOW for 10 ms then HIGH for 10 ms (vendor-specified reset).
void resetPulse();

// Poll BUSY pin until HIGH (ready). Aborts with a log message after `timeoutMs`.
// UC8253 BUSY is active-LOW: LOW=busy, HIGH=ready.
void waitBusy(uint32_t timeoutMs = 5000);

}  // namespace UC8253Spi

#endif  // BOARD_MURPHY
