#pragma once

#include <cstdint>
#include <cstring>

// Standard HID Consumer Page codes for page navigation
constexpr uint8_t CONSUMER_PAGE_UP = 0xE9;
constexpr uint8_t CONSUMER_PAGE_DOWN = 0xEA;

// Device-specific HID report profile — determines how to extract keycodes
struct DeviceProfile {
  const char* name;           // Human-readable device name
  const char* macPrefix;      // MAC address prefix for matching (nullptr = match any)
  uint8_t pageUpCode;         // HID keycode for "previous page"
  uint8_t pageDownCode;       // HID keycode for "next page"
  bool isConsumerPage;        // Uses Consumer Control page codes?
  uint8_t reportByteIndex;    // Which byte in the HID report contains the keycode
};

// Known device database — ordered by specificity (most specific first)
static const DeviceProfile KNOWN_DEVICES[] = {
    {"IINE Game Brick", nullptr, 0x09, 0x07, false, 4},
    {"Mini_Keyboard", nullptr, 0x4E, 0x4B, false, 2},
    {"Kobo Elipsa 2E", nullptr, CONSUMER_PAGE_UP, CONSUMER_PAGE_DOWN, true, 2},
    {"Free2 Style", nullptr, CONSUMER_PAGE_UP, CONSUMER_PAGE_DOWN, true, 2},
};
static constexpr int KNOWN_DEVICES_COUNT = sizeof(KNOWN_DEVICES) / sizeof(KNOWN_DEVICES[0]);

// Generic fallback profile — standard keyboard at byte[2]
static const DeviceProfile GENERIC_PROFILE = {"Generic HID", nullptr, 0x4B, 0x4E, false, 2};

// Find a matching device profile by MAC address prefix or device name
inline const DeviceProfile* findDeviceProfile(const char* mac, const char* name) {
  for (int i = 0; i < KNOWN_DEVICES_COUNT; i++) {
    const auto& p = KNOWN_DEVICES[i];
    // Match by MAC prefix if specified
    if (p.macPrefix && mac && strncmp(mac, p.macPrefix, strlen(p.macPrefix)) == 0) {
      return &KNOWN_DEVICES[i];
    }
    // Match by device name substring
    if (p.name && name && strstr(name, p.name) != nullptr) {
      return &KNOWN_DEVICES[i];
    }
  }
  return &GENERIC_PROFILE;
}
