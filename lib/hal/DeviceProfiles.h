#pragma once
#ifdef ENABLE_BLE

#include <cstdint>
#include <string>

/**
 * HID Device Profiles for Page Turner Devices
 * 
 * Supports multiple page turner devices with their specific keycodes.
 * Each device profile maps button keycodes to page navigation actions.
 */

namespace DeviceProfiles {

// Standard HID Consumer Page codes (preferred, auto-detected first)
constexpr uint8_t STANDARD_PAGE_UP = 0xE9;      // Consumer Page: Page Up
constexpr uint8_t STANDARD_PAGE_DOWN = 0xEA;    // Consumer Page: Page Down

// Common keyboard/navigation page-turn keycodes seen on BLE clickers/remotes
constexpr uint8_t KEYBOARD_PAGE_UP = 0x4B;
constexpr uint8_t KEYBOARD_PAGE_DOWN = 0x4E;
constexpr uint8_t KEYBOARD_UP_ARROW = 0x52;
constexpr uint8_t KEYBOARD_DOWN_ARROW = 0x51;
constexpr uint8_t KEYBOARD_LEFT_ARROW = 0x50;
constexpr uint8_t KEYBOARD_RIGHT_ARROW = 0x4F;
constexpr uint8_t KEYBOARD_SPACE = 0x2C;
constexpr uint8_t KEYBOARD_ENTER = 0x28;
constexpr uint8_t KEYBOARD_VOLUME_UP = 0x80;
constexpr uint8_t KEYBOARD_VOLUME_DOWN = 0x81;

struct DeviceProfile {
  const char* name;           // Device name for display
  const char* macPrefix;      // MAC address prefix to identify device (or nullptr)
  uint8_t pageUpCode;         // HID keycode for page up/previous
  uint8_t pageDownCode;       // HID keycode for page down/next
  bool isConsumerPage;        // If true, these are Consumer Page codes (0xE9, 0xEA range)
  uint8_t reportByteIndex;    // Which byte in HID report contains the keycode
};

// Known device profiles (database of popular page turners)
constexpr DeviceProfile KNOWN_DEVICES[] = {
    // IINE Game Brick - specific keycodes in byte[4]
  // Common prefix seen on IINE_control devices; name fallback still handles variants.
  {"IINE Game Brick", "ce:61:73", 0x09, 0x07, false, 4},
    
    // MINI_KEYBOARD - standard keyboard page codes in byte[2]
    // Note: MAC prefix set to nullptr to allow ANY compatible keyboard
    {"MINI_KEYBOARD", nullptr, 0x4B, 0x4E, false, 2},
    
    // Kobo Elipsa 2E remote (common page turner) - Consumer Page
    {"Kobo Remote", nullptr, STANDARD_PAGE_UP, STANDARD_PAGE_DOWN, true, 2},
    
    // Generic Free2-style device pattern - standard HID Consumer Page
    {"Free2 Style", nullptr, STANDARD_PAGE_UP, STANDARD_PAGE_DOWN, true, 2},
};

constexpr int KNOWN_DEVICES_COUNT = sizeof(KNOWN_DEVICES) / sizeof(KNOWN_DEVICES[0]);

/**
 * Find a device profile by MAC address or device name
 * Returns nullptr if not found in known devices
 */
const DeviceProfile* findDeviceProfile(const char* macAddress, const char* deviceName);

/**
 * Check if keycodes match standard HID Consumer Page codes
 * (0xE9 = Page Up, 0xEA = Page Down)
 */
bool isStandardConsumerPageCode(uint8_t code);

/**
 * Check if a keycode is a commonly used page-turn keycode across
 * generic BLE clickers/remotes in keyboard or consumer report modes.
 */
bool isCommonPageTurnCode(uint8_t code);

/**
 * Convert a keycode to page-turn direction for generic devices.
 * Returns true if mapped; false if the keycode should be ignored.
 * Sets pageForward=true for next-page, false for previous-page.
 */
bool mapCommonCodeToDirection(uint8_t code, bool& pageForward);

/**
 * Get a user-configured device profile from settings
 * Returns nullptr if no custom profile is set
 */
const DeviceProfile* getCustomProfile();

/**
 * Set a user-configured device profile in settings
 */
void setCustomProfile(uint8_t pageUpCode, uint8_t pageDownCode, uint8_t reportByteIndex);

/**
 * Clear custom profile and revert to auto-detection
 */
void clearCustomProfile();

}  // namespace DeviceProfiles
#endif // ENABLE_BLE
