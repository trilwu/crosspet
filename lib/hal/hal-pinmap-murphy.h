#pragma once
// hal-pinmap-murphy.h — GPIO assignments for Murphy board (ESP32-S3 + GDEY037T03).
// Included by HalGPIO.h when -DBOARD_MURPHY is set.
//
// TENTATIVE: pin-to-role mapping not yet bench-verified (phase 04 confirms).
// DC↔BUSY swap (GPIO47/48) is the most likely ambiguity — see phase 04 notes.

// E-ink display SPI (UC8253 on GDEY037T03)
#define EPD_CS   16  // Chip Select
#define EPD_MOSI 17  // SPI MOSI
#define EPD_SCLK 18  // SPI Clock
#define EPD_MISO 19  // MISO (unused for UC8253 write-only, or BS pin tied LOW)
#define EPD_RST  22  // Reset
#define EPD_DC   47  // Data/Command — TENTATIVE (may swap with GPIO48)
#define EPD_BUSY 48  // Busy active-LOW — TENTATIVE (may swap with GPIO47)

// SD card SPI — S3 has dedicated FSPI; share bus with display or use separate host.
// Assigned conservatively to avoid conflict with display; phase 06 finalises.
#define SD_MISO  37
#define SD_MOSI  35
#define SD_SCLK  36
#define SD_CS    34

// Battery / USB sense (Murphy-specific; update after board schematic review)
#define BAT_GPIO0  1   // ADC1_CH0 on S3 — TENTATIVE
#define UART0_RXD  44  // USB UART RX (S3 default)
