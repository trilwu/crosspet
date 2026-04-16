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
constexpr uint8_t STANDARD_PAGE_UP = 0xE9;      // Consumer Page: Volume Up (used as page up by some devices)
constexpr uint8_t STANDARD_PAGE_DOWN = 0xEA;    // Consumer Page: Volume Down (used as page down by some devices)

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

// Free2 rolling keycodes (confirmed from upstream crosspoint-reader-ble)
// Free2 devices cycle through keycodes on each press to avoid OS duplicate-key suppression.
// Direction is determined by group membership, not by a single fixed code.
constexpr uint8_t FREE2_FORWARD_A = 0x1C;
constexpr uint8_t FREE2_FORWARD_B = 0xC4;
constexpr uint8_t FREE2_FORWARD_C = 0x6C;
constexpr uint8_t FREE2_FORWARD_D = 0xBC;
constexpr uint8_t FREE2_BACK_A = 0xB4;
constexpr uint8_t FREE2_BACK_B = 0x0E;
constexpr uint8_t FREE2_BACK_C = 0x66;
constexpr uint8_t FREE2_BACK_D = 0x16;

struct DeviceProfile {
  const char* name;           // Device name for display
  const char* macPrefix;      // MAC address prefix to identify device (or nullptr)
  uint8_t pageUpCode;         // HID keycode for page up/previous
  uint8_t pageDownCode;       // HID keycode for page down/next
  bool isConsumerPage;        // If true, these are Consumer Page codes (0xE9, 0xEA range)
  uint8_t reportByteIndex;    // Which byte in HID report contains the keycode
  bool strictProfile;         // If true, per-device custom profile won't override this entry
};

// Known device profiles (database of popular page turners)
constexpr DeviceProfile KNOWN_DEVICES[] = {
    // IINE Game Brick V2 — MAC prefix 60:4d:ec, non-standard report
    {"IINE Game Brick V2", "60:4d:ec", 0x09, 0x07, false, 2, true},

    // IINE Game Brick — byte[4] with bitmask press-state
    {"IINE Game Brick", nullptr, 0x09, 0x07, false, 4, true},

    // MINI_KEYBOARD — standard keyboard page codes in byte[2]
    {"MINI_KEYBOARD", nullptr, 0x4B, 0x4E, false, 2, false},

    // Kobo Remote — Consumer Page
    {"Kobo Remote", nullptr, STANDARD_PAGE_UP, STANDARD_PAGE_DOWN, true, 2, false},

    // Generic Free2-style device — Consumer Page
    {"Free2 Style", nullptr, STANDARD_PAGE_UP, STANDARD_PAGE_DOWN, true, 2, false},

    // Free2-M page turner
    {"Free2-M", nullptr, 0x02, 0x01, false, 2, false},

    // Free3-M page turner
    {"Free3-M", nullptr, 0x02, 0x01, false, 2, false},
};

constexpr int KNOWN_DEVICES_COUNT = sizeof(KNOWN_DEVICES) / sizeof(KNOWN_DEVICES[0]);

/**
 * Find a device profile by MAC address or device name.
 * Checks per-device custom profiles before falling back to KNOWN_DEVICES.
 * Returns nullptr if not found.
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
 * Includes Free2 rolling codes, SPACE, ENTER, UP/DOWN arrows.
 */
bool isCommonPageTurnCode(uint8_t code);

/**
 * Convert a keycode to page-turn direction for generic devices.
 * Returns true if mapped; false if the keycode should be ignored.
 * Sets pageForward=true for next-page, false for previous-page.
 * Includes Free2 rolling codes.
 */
bool mapCommonCodeToDirection(uint8_t code, bool& pageForward);

/**
 * Get a user-configured global device profile from settings.
 * Returns nullptr if no custom profile is set.
 */
const DeviceProfile* getCustomProfile();

/**
 * Set a user-configured global device profile in settings.
 */
void setCustomProfile(uint8_t pageUpCode, uint8_t pageDownCode, uint8_t reportByteIndex);

/**
 * Clear global custom profile and revert to auto-detection.
 */
void clearCustomProfile();

/**
 * Get per-device custom profile from SD card file.
 * Returns true and populates outProfile if a saved profile exists for this MAC.
 * Returns false if no custom profile is saved for this MAC address.
 */
bool getCustomProfileForDevice(const std::string& macAddress, DeviceProfile& outProfile);

/**
 * Save a per-device custom profile (persists across reboots via SD card).
 * Stored in /.crosspoint/ble_device_profiles.txt — one line per device.
 */
void setCustomProfileForDevice(const std::string& macAddress, uint8_t pageUpCode,
                                uint8_t pageDownCode, uint8_t reportByteIndex);

/**
 * Clear per-device custom profile for a specific MAC address.
 */
void clearCustomProfileForDevice(const std::string& macAddress);

}  // namespace DeviceProfiles
#endif // ENABLE_BLE
