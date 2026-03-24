#ifdef ENABLE_BLE
#include "DeviceProfiles.h"

#include <cstdio>
#include <cstring>
#include <Logging.h>
#include <HalStorage.h>

namespace {
constexpr const char* CUSTOM_PROFILE_FILE = "/.crosspoint/ble_custom_profile.txt";

DeviceProfiles::DeviceProfile customProfile = {"Custom BLE Remote", nullptr, 0x00, 0x00, false, 2};
bool customProfileLoaded = false;

void loadCustomProfileFromStorage() {
  if (customProfileLoaded) {
    return;
  }
  customProfileLoaded = true;

  if (!Storage.exists(CUSTOM_PROFILE_FILE)) {
    return;
  }

  String content = Storage.readFile(CUSTOM_PROFILE_FILE);
  if (content.isEmpty()) {
    return;
  }

  unsigned int up = 0;
  unsigned int down = 0;
  if (sscanf(content.c_str(), "%u,%u", &up, &down) == 2 && up <= 0xFF && down <= 0xFF && up != 0 && down != 0) {
    customProfile.pageUpCode = static_cast<uint8_t>(up);
    customProfile.pageDownCode = static_cast<uint8_t>(down);
    LOG_INF("DEV", "Loaded custom BLE profile: up=0x%02X down=0x%02X", customProfile.pageUpCode,
            customProfile.pageDownCode);
  }
}
}  // namespace

namespace DeviceProfiles {

const DeviceProfile* findDeviceProfile(const char* macAddress, const char* deviceName) {
  // First, try to find by MAC address prefix (case-insensitive comparison)
  if (macAddress) {
    for (int i = 0; i < KNOWN_DEVICES_COUNT; i++) {
      if (KNOWN_DEVICES[i].macPrefix) {
        // Case-insensitive MAC prefix check
        const char* prefix = KNOWN_DEVICES[i].macPrefix;
        size_t prefixLen = strlen(prefix);
        bool matches = true;
        
        for (size_t j = 0; j < prefixLen && macAddress[j] != '\0'; j++) {
          char macChar = macAddress[j];
          char prefixChar = prefix[j];
          // Convert both to lowercase for comparison
          if (macChar >= 'A' && macChar <= 'F') macChar = macChar - 'A' + 'a';
          if (prefixChar >= 'A' && prefixChar <= 'F') prefixChar = prefixChar - 'A' + 'a';
          
          if (macChar != prefixChar) {
            matches = false;
            break;
          }
        }
        
        if (matches) {
          LOG_INF("DEV", "✓ Matched device profile by MAC: %s -> %s", macAddress, KNOWN_DEVICES[i].name);
          return &KNOWN_DEVICES[i];
        }
      }
    }
    LOG_DBG("DEV", "No MAC match found for: %s", macAddress);
  }

  // Then try to find by device name (flexible matching)
  if (deviceName && strlen(deviceName) > 0) {
    for (int i = 0; i < KNOWN_DEVICES_COUNT; i++) {
      const char* profileName = KNOWN_DEVICES[i].name;
      
      // Try exact match first
      if (strcmp(deviceName, profileName) == 0) {
        LOG_INF("DEV", "✓ Matched device profile by exact name: %s", profileName);
        return &KNOWN_DEVICES[i];
      }
      
      // Try case-insensitive substring match for common patterns
      // This allows "Game Brick", "GameBrick", "IINE Game Brick", etc.
      if (strstr(deviceName, "Game") || strstr(deviceName, "game") || 
          strstr(deviceName, "GAME")) {
        if (strstr(deviceName, "Brick") || strstr(deviceName, "brick") || 
            strstr(deviceName, "BRICK")) {
          LOG_INF("DEV", "✓ Matched GameBrick by name pattern: %s -> IINE Game Brick", deviceName);
          return &KNOWN_DEVICES[0];  // Return GameBrick profile
        }
      }

      // Match IINE_control naming used by some GameBrick remotes
      if (strstr(deviceName, "IINE") || strstr(deviceName, "iine") ||
          strstr(deviceName, "IINE_control") || strstr(deviceName, "iine_control")) {
        LOG_INF("DEV", "✓ Matched GameBrick by IINE naming: %s", deviceName);
        return &KNOWN_DEVICES[0];
      }
      
      // Match MINI_KEYBOARD variants
      if (strstr(deviceName, "MINI") || strstr(deviceName, "mini") || 
          strstr(deviceName, "Mini")) {
        if (strstr(deviceName, "KEYBOARD") || strstr(deviceName, "keyboard") || 
            strstr(deviceName, "Keyboard")) {
          LOG_INF("DEV", "✓ Matched MINI_KEYBOARD by name pattern: %s", deviceName);
          return &KNOWN_DEVICES[1];  // Return MINI_KEYBOARD profile
        }
      }
    }
    
    LOG_DBG("DEV", "No profile match for device name: %s", deviceName);
  }

  return nullptr;
}

bool isStandardConsumerPageCode(uint8_t code) {
  // Standard HID Consumer Page codes for page navigation
  return code == STANDARD_PAGE_UP || code == STANDARD_PAGE_DOWN;
}

bool isCommonPageTurnCode(uint8_t code) {
  switch (code) {
    // Consumer page navigation
    case STANDARD_PAGE_UP:
    case STANDARD_PAGE_DOWN:

    // Keyboard page/navigation keys
    case KEYBOARD_PAGE_UP:
    case KEYBOARD_PAGE_DOWN:
    case KEYBOARD_LEFT_ARROW:
    case KEYBOARD_RIGHT_ARROW:

    // Media keys used by some remotes
    case KEYBOARD_VOLUME_UP:
    case KEYBOARD_VOLUME_DOWN:

    // Existing GameBrick fallback codes
    case 0x07:
    case 0x09:
      return true;
    default:
      return false;
  }
}

bool mapCommonCodeToDirection(uint8_t code, bool& pageForward) {
  switch (code) {
    // Next page — only unambiguous page-turn codes
    case STANDARD_PAGE_UP:
    case KEYBOARD_PAGE_DOWN:
    case KEYBOARD_RIGHT_ARROW:
    case KEYBOARD_VOLUME_UP:
    case 0x07:
      pageForward = true;
      return true;

    // Previous page
    case STANDARD_PAGE_DOWN:
    case KEYBOARD_PAGE_UP:
    case KEYBOARD_LEFT_ARROW:
    case KEYBOARD_VOLUME_DOWN:
    case 0x09:
      pageForward = false;
      return true;

    // NOT mapped: ENTER (0x28), SPACE (0x2C), UP/DOWN arrows
    // These have navigation meaning and should not be page-turn
    default:
      return false;
  }
}

const DeviceProfile* getCustomProfile() {
  loadCustomProfileFromStorage();
  if (customProfile.pageUpCode == 0x00 || customProfile.pageDownCode == 0x00) {
    return nullptr;
  }
  return &customProfile;
}

void setCustomProfile(uint8_t pageUpCode, uint8_t pageDownCode, uint8_t reportByteIndex) {
  (void)reportByteIndex;
  customProfile.pageUpCode = pageUpCode;
  customProfile.pageDownCode = pageDownCode;
  customProfileLoaded = true;

  char buf[16];
  snprintf(buf, sizeof(buf), "%u,%u", static_cast<unsigned>(pageUpCode), static_cast<unsigned>(pageDownCode));
  Storage.writeFile(CUSTOM_PROFILE_FILE, buf);
  LOG_INF("DEV", "Custom profile set: up=0x%02X down=0x%02X", pageUpCode, pageDownCode);
}

void clearCustomProfile() {
  customProfile.pageUpCode = 0x00;
  customProfile.pageDownCode = 0x00;
  customProfileLoaded = true;
  Storage.remove(CUSTOM_PROFILE_FILE);
  LOG_INF("DEV", "Custom profile cleared");
}

}  // namespace DeviceProfiles
#endif // ENABLE_BLE
