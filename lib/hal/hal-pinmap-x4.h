#pragma once
// hal-pinmap-x4.h — GPIO assignments for Xteink X4 (ESP32-C3).
// Included by HalGPIO.h when BOARD_MURPHY is NOT defined.
// All EPD pin constants previously lived inline in HalGPIO.h — kept here for clarity.

// E-ink display SPI (custom pins, not hardware SPI defaults on C3)
#define EPD_SCLK 8   // SPI Clock
#define EPD_MOSI 10  // SPI MOSI
#define EPD_CS   21  // Chip Select
#define EPD_DC   4   // Data/Command
#define EPD_RST  5   // Reset
#define EPD_BUSY 6   // Busy (active-LOW on SSD1677: busy=LOW, ready=HIGH)

// Shared MISO between SD card and display
#define SPI_MISO 7

// Battery / USB sense
#define BAT_GPIO0  0   // Battery ADC (X4)
#define UART0_RXD  20  // USB connection detect (HIGH when USB connected)
