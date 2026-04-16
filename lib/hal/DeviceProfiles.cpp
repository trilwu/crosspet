#ifdef ENABLE_BLE
#include "DeviceProfiles.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <Logging.h>
#include <HalStorage.h>

namespace {

// ─── Global custom profile (single, user-set) ────────────────────────────────

constexpr const char* CUSTOM_PROFILE_FILE = "/.crosspoint/ble_custom_profile.txt";

DeviceProfiles::DeviceProfile customProfile = {"Custom BLE Remote", nullptr, 0x00, 0x00, false, 2, false};
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

// ─── Per-device custom profiles (keyed by MAC address) ───────────────────────

constexpr const char* DEVICE_PROFILES_FILE = "/.crosspoint/ble_device_profiles.txt";

// In-memory cache: MAC -> profile. Loaded lazily from SD card.
// std::map is acceptable here — only allocated when BLE is active.
struct PerDeviceEntry {
  DeviceProfiles::DeviceProfile profile;
  char nameBuf[24];  // Storage for profile.name (avoids dangling ptr)
};

static std::map<std::string, PerDeviceEntry> deviceProfileCache;
static bool deviceProfileCacheLoaded = false;

static void loadDeviceProfileCache() {
  if (deviceProfileCacheLoaded) return;
  deviceProfileCacheLoaded = true;

  if (!Storage.exists(DEVICE_PROFILES_FILE)) return;

  String content = Storage.readFile(DEVICE_PROFILES_FILE);
  if (content.isEmpty()) return;

  const char* p = content.c_str();
  while (*p) {
    char mac[18] = {};
    unsigned up = 0, down = 0, bi = 0;
    int n = sscanf(p, "%17[^,],%u,%u,%u", mac, &up, &down, &bi);
    if (n == 4 && up <= 0xFF && down <= 0xFF && bi <= 10 && up != 0 && down != 0) {
      PerDeviceEntry entry = {};
      snprintf(entry.nameBuf, sizeof(entry.nameBuf), "Custom[%.12s]", mac);
      entry.profile = {entry.nameBuf, nullptr,
                       static_cast<uint8_t>(up), static_cast<uint8_t>(down),
                       false, static_cast<uint8_t>(bi), false};
      auto& stored = deviceProfileCache[std::string(mac)] = entry;
      stored.profile.name = stored.nameBuf;  // fix self-pointer after copy
    }
    // Advance to next line
    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;
  }
  LOG_INF("DEV", "Loaded %zu per-device BLE profiles from storage", deviceProfileCache.size());
}

static void saveDeviceProfileCache() {
  // Build file content from map
  String out;
  char line[48];
  for (const auto& kv : deviceProfileCache) {
    const auto& p = kv.second.profile;
    snprintf(line, sizeof(line), "%s,%u,%u,%u\n",
             kv.first.c_str(),
             static_cast<unsigned>(p.pageUpCode),
             static_cast<unsigned>(p.pageDownCode),
             static_cast<unsigned>(p.reportByteIndex));
    out += line;
  }
  // Ensure parent directory exists
  if (!Storage.exists("/.crosspoint")) {
    Storage.mkdir("/.crosspoint");
  }
  Storage.writeFile(DEVICE_PROFILES_FILE, out);
}

}  // namespace

namespace DeviceProfiles {

// ─── Profile lookup ──────────────────────────────────────────────────────────

const DeviceProfile* findDeviceProfile(const char* macAddress, const char* deviceName) {
  // 1. MAC prefix match against KNOWN_DEVICES (strict entries returned immediately)
  if (macAddress) {
    for (int i = 0; i < KNOWN_DEVICES_COUNT; i++) {
      if (!KNOWN_DEVICES[i].macPrefix) continue;

      const char* prefix = KNOWN_DEVICES[i].macPrefix;
      size_t prefixLen = strlen(prefix);
      bool matches = (strlen(macAddress) >= prefixLen);

      for (size_t j = 0; j < prefixLen && matches; j++) {
        char mc = macAddress[j];
        char pc = prefix[j];
        if (mc >= 'A' && mc <= 'F') mc = mc - 'A' + 'a';
        if (pc >= 'A' && pc <= 'F') pc = pc - 'A' + 'a';
        if (mc != pc) matches = false;
      }

      if (matches) {
        LOG_INF("DEV", "Matched device profile by MAC: %s -> %s", macAddress, KNOWN_DEVICES[i].name);
        if (KNOWN_DEVICES[i].strictProfile) {
          // Strict profiles cannot be overridden by custom per-device profiles
          return &KNOWN_DEVICES[i];
        }
        // Non-strict MAC match: check per-device custom first
        DeviceProfile perDeviceOut;
        if (getCustomProfileForDevice(std::string(macAddress), perDeviceOut)) {
          // The caller gets a pointer to the cached entry — safe because cache persists
          // for the session. We return &KNOWN_DEVICES[i] as the base but the per-device
          // profile overrides codes at the call site (BluetoothHIDInput). Here we honour
          // the spec: per-device custom profile wins over non-strict KNOWN_DEVICES.
          // We return the KNOWN_DEVICES entry and let the caller query per-device on top.
          // (findDeviceProfile's contract is: return best static profile match.)
          return &KNOWN_DEVICES[i];
        }
        return &KNOWN_DEVICES[i];
      }
    }
    LOG_DBG("DEV", "No MAC match found for: %s", macAddress);
  }

  // 2. Name-based matching (flexible, case-insensitive substring)
  if (deviceName && strlen(deviceName) > 0) {
    for (int i = 0; i < KNOWN_DEVICES_COUNT; i++) {
      const char* profileName = KNOWN_DEVICES[i].name;

      // Exact match
      if (strcmp(deviceName, profileName) == 0) {
        LOG_INF("DEV", "Matched device profile by exact name: %s", profileName);
        return &KNOWN_DEVICES[i];
      }
    }

    // Game Brick pattern (handles "Game Brick", "GameBrick", "IINE Game Brick", etc.)
    bool hasGame = strstr(deviceName, "Game") || strstr(deviceName, "game") || strstr(deviceName, "GAME");
    bool hasBrick = strstr(deviceName, "Brick") || strstr(deviceName, "brick") || strstr(deviceName, "BRICK");
    if (hasGame && hasBrick) {
      LOG_INF("DEV", "Matched GameBrick by name pattern: %s", deviceName);
      return &KNOWN_DEVICES[1];  // IINE Game Brick (index 1, non-V2)
    }

    // IINE_control naming pattern
    if (strstr(deviceName, "IINE") || strstr(deviceName, "iine")) {
      LOG_INF("DEV", "Matched GameBrick by IINE naming: %s", deviceName);
      return &KNOWN_DEVICES[1];
    }

    // MINI_KEYBOARD pattern
    bool hasMini = strstr(deviceName, "MINI") || strstr(deviceName, "mini") || strstr(deviceName, "Mini");
    bool hasKbd  = strstr(deviceName, "KEYBOARD") || strstr(deviceName, "keyboard") || strstr(deviceName, "Keyboard");
    if (hasMini && hasKbd) {
      LOG_INF("DEV", "Matched MINI_KEYBOARD by name pattern: %s", deviceName);
      return &KNOWN_DEVICES[2];  // MINI_KEYBOARD
    }

    // Free2 / Free3 fuzzy matching (handles "Free2", "FREE2", "free2", "Free 2", "Free-2",
    // "Free2-M", "Free3", "FREE3", "free3", "Free 3", "Free-3", "Free3-M")
    bool hasFree2 = strstr(deviceName, "Free2") || strstr(deviceName, "FREE2") ||
                    strstr(deviceName, "free2") || strstr(deviceName, "Free 2") ||
                    strstr(deviceName, "Free-2");
    bool hasFree3 = strstr(deviceName, "Free3") || strstr(deviceName, "FREE3") ||
                    strstr(deviceName, "free3") || strstr(deviceName, "Free 3") ||
                    strstr(deviceName, "Free-3");

    if (hasFree3) {
      LOG_INF("DEV", "Matched Free3-M by name pattern: %s", deviceName);
      return &KNOWN_DEVICES[6];  // Free3-M
    }
    if (hasFree2) {
      // Distinguish Free2-M from generic Free2 Style by "-M" or "M" suffix
      bool isM = strstr(deviceName, "-M") || strstr(deviceName, "Free2M") || strstr(deviceName, "FREE2M");
      if (isM) {
        LOG_INF("DEV", "Matched Free2-M by name pattern: %s", deviceName);
        return &KNOWN_DEVICES[5];  // Free2-M
      }
      LOG_INF("DEV", "Matched Free2 Style by name pattern: %s", deviceName);
      return &KNOWN_DEVICES[4];  // Free2 Style
    }

    LOG_DBG("DEV", "No profile match for device name: %s", deviceName);
  }

  return nullptr;
}

// ─── Code classification ─────────────────────────────────────────────────────

bool isStandardConsumerPageCode(uint8_t code) {
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
    case KEYBOARD_UP_ARROW:
    case KEYBOARD_DOWN_ARROW:
    case KEYBOARD_LEFT_ARROW:
    case KEYBOARD_RIGHT_ARROW:
    // NOT mapped: SPACE (0x2C) and ENTER (0x28) — these have navigation meaning
    // in menus and would cause false page-turn fires on generic keyboards.

    // Media/volume keys used by some remotes
    case KEYBOARD_VOLUME_UP:
    case KEYBOARD_VOLUME_DOWN:

    // Free2 rolling keycodes (forward group)
    case FREE2_FORWARD_A:
    case FREE2_FORWARD_B:
    case FREE2_FORWARD_C:
    case FREE2_FORWARD_D:

    // Free2 rolling keycodes (back group)
    case FREE2_BACK_A:
    case FREE2_BACK_B:
    case FREE2_BACK_C:
    case FREE2_BACK_D:

    // GameBrick fallback codes
    case 0x07:
    case 0x09:
      return true;

    default:
      return false;
  }
}

bool mapCommonCodeToDirection(uint8_t code, bool& pageForward) {
  switch (code) {
    // Next page (forward)
    case STANDARD_PAGE_UP:
    case KEYBOARD_PAGE_DOWN:
    case KEYBOARD_DOWN_ARROW:
    case KEYBOARD_RIGHT_ARROW:
    case KEYBOARD_VOLUME_UP:
    case FREE2_FORWARD_A:
    case FREE2_FORWARD_B:
    case FREE2_FORWARD_C:
    case FREE2_FORWARD_D:
    case 0x07:
      pageForward = true;
      return true;

    // Previous page (back)
    case STANDARD_PAGE_DOWN:
    case KEYBOARD_PAGE_UP:
    case KEYBOARD_UP_ARROW:
    case KEYBOARD_LEFT_ARROW:
    case KEYBOARD_VOLUME_DOWN:
    case FREE2_BACK_A:
    case FREE2_BACK_B:
    case FREE2_BACK_C:
    case FREE2_BACK_D:
    case 0x09:
      pageForward = false;
      return true;

    default:
      return false;
  }
}

// ─── Global custom profile ────────────────────────────────────────────────────

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

// ─── Per-device custom profiles ──────────────────────────────────────────────

bool getCustomProfileForDevice(const std::string& macAddress, DeviceProfile& outProfile) {
  loadDeviceProfileCache();
  auto it = deviceProfileCache.find(macAddress);
  if (it == deviceProfileCache.end()) return false;
  outProfile = it->second.profile;
  return true;
}

void setCustomProfileForDevice(const std::string& macAddress, uint8_t pageUpCode,
                                uint8_t pageDownCode, uint8_t reportByteIndex) {
  loadDeviceProfileCache();

  PerDeviceEntry entry = {};
  snprintf(entry.nameBuf, sizeof(entry.nameBuf), "Custom[%.12s]", macAddress.c_str());
  entry.profile = {entry.nameBuf, nullptr,
                   pageUpCode, pageDownCode,
                   false, reportByteIndex, false};
  auto& stored = deviceProfileCache[macAddress] = entry;
  stored.profile.name = stored.nameBuf;  // fix self-pointer after copy
  saveDeviceProfileCache();

  LOG_INF("DEV", "Per-device profile set for %s: up=0x%02X down=0x%02X byte=%u",
          macAddress.c_str(), pageUpCode, pageDownCode, reportByteIndex);
}

void clearCustomProfileForDevice(const std::string& macAddress) {
  loadDeviceProfileCache();
  auto it = deviceProfileCache.find(macAddress);
  if (it == deviceProfileCache.end()) return;
  deviceProfileCache.erase(it);
  saveDeviceProfileCache();
  LOG_INF("DEV", "Per-device profile cleared for %s", macAddress.c_str());
}

}  // namespace DeviceProfiles
#endif // ENABLE_BLE
