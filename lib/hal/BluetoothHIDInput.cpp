#ifdef ENABLE_BLE
// BluetoothHIDInput.cpp
// HID input processing: onHIDNotify, parseHIDReport, mapKeycodeToButton,
// hasRecentActivity, updateActivity, checkAutoReconnect.
// Part of BluetoothHIDManager — split for modularity.

#include "BluetoothHIDManager.h"
#include <Logging.h>
#include <NimBLEDevice.h>
#include <HalGPIO.h>

extern BluetoothHIDManager* g_instance;

// ---- GameBrick V2 button keycodes (byte[4] values) ----

static constexpr uint8_t GAMEBRICK_V2_BTN_CENTER = 0x0B;
static constexpr uint8_t GAMEBRICK_V2_BTN_A      = 0x09;
static constexpr uint8_t GAMEBRICK_V2_BTN_B      = 0x07;

// ---- Generic keycode extractor for unknown remotes ----

static uint8_t extractGenericPageTurnKeycode(const uint8_t* report, size_t length) {
  if (!report || length == 0) {
    return 0x00;
  }

  // First pass: prefer known page-turn keycodes anywhere in short reports.
  const size_t scanLen = length < 8 ? length : 8;
  for (size_t i = 0; i < scanLen; i++) {
    const uint8_t code = report[i];
    if (DeviceProfiles::isCommonPageTurnCode(code)) {
      return code;
    }
  }

  // Second pass: typical keyboard report key slots (bytes 2..7)
  for (size_t i = 2; i < scanLen; i++) {
    if (report[i] != 0x00) {
      return report[i];
    }
  }

  return 0x00;
}

// ---- Activity / Inactivity ----

bool BluetoothHIDManager::hasRecentActivity() const {
  unsigned long now = millis();
  for (const auto& device : _connectedDevices) {
    if (device.lastActivityTime > 0) {
      unsigned long timeSinceActivity = now - device.lastActivityTime;
      if (timeSinceActivity < 240000) {  // 4 minutes
        return true;
      }
    }
  }
  return false;
}

void BluetoothHIDManager::updateActivity() {
  // Check inactivity timeouts every 10 seconds
  unsigned long now = millis();

  if (now - lastMaintenanceCheck < 10000) {
    return;
  }
  lastMaintenanceCheck = now;

  // Find one inactive device and disconnect it
  std::string inactiveAddress;
  unsigned long inactiveTimeMs = 0;
  for (const auto& device : _connectedDevices) {
    if (device.lastActivityTime == 0) {
      continue;
    }

    unsigned long inactiveTime = now - device.lastActivityTime;
    if (inactiveTime > INACTIVITY_TIMEOUT_MS) {
      inactiveAddress = device.address;
      inactiveTimeMs = inactiveTime;
      break;
    }
  }

  if (!inactiveAddress.empty()) {
    LOG_INF("BT", "Device %s inactive for %lu ms, disconnecting",
            inactiveAddress.c_str(), inactiveTimeMs);
    disconnectFromDevice(inactiveAddress);
  }

  // Fire pending GameBrick synthetic releases
  unsigned long now2 = millis();
  for (auto& device : _connectedDevices) {
    if (device.pendingGameBrickRelease && now2 >= device.pendingGameBrickReleaseMs) {
      device.pendingGameBrickRelease = false;
      device.lastHIDKeycode = 0x00;
      device.lastButtonState = false;
      LOG_DBG("BT", "Synthetic GameBrick release for key=0x%02X", device.pendingGameBrickKeycode);
    }
  }
}

void BluetoothHIDManager::checkAutoReconnect(bool userInputDetected) {
  if (!_enabled) {
    return;
  }

  static unsigned long lastReconnectCheck = 0;
  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();

  // Only check every 5 seconds to avoid hammering
  if (now - lastReconnectCheck < 5000) {
    return;
  }
  lastReconnectCheck = now;

  // Remove stale disconnected clients from active list
  for (auto it = _connectedDevices.begin(); it != _connectedDevices.end();) {
    if (!it->client || !it->client->isConnected()) {
      // Don't deleteClient — stays in disconnected pool for reuse
      it = _connectedDevices.erase(it);
    } else {
      ++it;
    }
  }

  if (!_connectedDevices.empty()) {
    return;
  }

  // Avoid reconnect storms — wait 30s between attempts
  if (now - lastReconnectAttempt < 30000) {
    return;
  }
  lastReconnectAttempt = now;

  if (_bondedDeviceAddress.empty()) {
    return;
  }

  LOG_INF("BT", "Button activity detected while disconnected, reconnecting to bonded device %s",
          _bondedDeviceAddress.c_str());
  if (connectToDevice(_bondedDeviceAddress)) {
    LOG_INF("BT", "Reconnected to bonded device %s", _bondedDeviceAddress.c_str());
  } else {
    LOG_ERR("BT", "Reconnect to bonded device %s failed: %s",
            _bondedDeviceAddress.c_str(), lastError.c_str());
  }
}

// ---- Static helpers ----

// Returns true if device's profile name matches the given string.
static bool profileIs(const ConnectedDevice* dev, const char* name) {
  return dev->profile && strcmp(dev->profile->name, name) == 0;
}

// Inject a button event with 90ms same-key jitter suppression.
// Calls _buttonInjector and _buttonActivityNotifier if set.
void BluetoothHIDManager::injectWithCooldown(ConnectedDevice* device, uint8_t keycode, uint8_t btn) {
  unsigned long now = millis();
  // 90ms same-key jitter suppression (tightened from 150ms)
  if (keycode == device->lastInjectedKeycode && (now - device->lastInjectionTime) < 90) {
    LOG_DBG("BT", "Suppressed duplicate key=0x%02X within 90ms", keycode);
    return;
  }
  if (g_instance->_buttonInjector && btn != 0xFF) {
    static const char* btnNames[] = {"Back", "Confirm", "Left", "Right", "Up", "Down", "Power"};
    const char* btnName = (btn < 7) ? btnNames[btn] : "?";
    LOG_INF("BT", "Inject: key=0x%02X -> btn=%d (%s)", keycode, btn, btnName);
    g_instance->_buttonInjector(btn);
    device->lastInjectionTime = now;
    device->lastInjectedKeycode = keycode;
    device->lastNormalizedEventMs = now;

    if (g_instance->_buttonActivityNotifier) {
      g_instance->_buttonActivityNotifier(btn);
    }
  }
}

// ---- Profile-specific handlers ----

// GameBrick V1: byte[0] encodes press state (0x1x family), keycode in byte[4].
void BluetoothHIDManager::handleGameBrickV1(ConnectedDevice* device, uint8_t* pData, size_t length) {
  if (length < 5) return;

  // Accept only stable digital-button reports (0x1x family)
  const bool stableButtonReport = (pData[0] & 0xF0) == 0x10;
  if (!stableButtonReport) {
    LOG_DBG("BT", "Game Brick V1: ignoring transitional report byte[0]=0x%02X", pData[0]);
    device->lastButtonState = false;
    device->lastHIDKeycode = 0x00;
    return;
  }

  const bool isPressed = (pData[0] & 0x01) != 0;
  const uint8_t keycode = pData[device->profile->reportByteIndex];

  // Ignore startup pressed frame until a clean release is seen
  if (!device->hasSeenRelease) {
    if (!isPressed) {
      device->hasSeenRelease = true;
    } else {
      LOG_DBG("BT", "Game Brick V1: ignoring startup pressed frame keycode=0x%02X", keycode);
      device->lastButtonState = true;
      device->lastHIDKeycode = keycode;
      return;
    }
  }

  LOG_DBG("BT", "Game Brick V1: byte[0]=0x%02X, keycode=0x%02X, pressed=%d",
          pData[0], keycode, isPressed);

  const bool isNewPressEvent = isPressed &&
    (!device->lastButtonState || keycode != device->lastHIDKeycode);

  if (isNewPressEvent) {
    uint8_t btn = g_instance->mapKeycodeToButton(keycode, device);
    injectWithCooldown(device, keycode, btn);
  }

  device->lastButtonState = isPressed;
  device->lastHIDKeycode = keycode;
}

// GameBrick V2: counter-freeze detection, joystick X axis, center button debounce,
// context-sensitive A/B mapping. Schedules synthetic release after 80ms.
void BluetoothHIDManager::handleGameBrickV2(ConnectedDevice* device, uint8_t* pData, size_t length) {
  if (length < 5) return;

  const uint16_t counter = pData[1] | (static_cast<uint16_t>(pData[2]) << 8);
  const bool counterFrozen = (counter == device->lastGameBrickCounter) && counter != 0;
  device->lastGameBrickCounter = counter;

  const uint8_t joystickX = pData[3];  // 0x98 = center
  const bool joyLeft  = joystickX < 0x60;
  const bool joyRight = joystickX > 0xD0;

  const uint8_t keycode = pData[4];

  // Center button: accumulate frames to debounce
  const bool centerPressed = (keycode == GAMEBRICK_V2_BTN_CENTER);
  if (centerPressed) {
    device->gameBrickCenterPressFrames++;
    if (device->gameBrickCenterPressFrames < 3) return;  // require 3 consecutive frames
  } else {
    device->gameBrickCenterPressFrames = 0;
  }

  // A/B buttons: context-sensitive mapping
  uint8_t mappedBtn = 0xFF;
  const bool inReader = g_instance->_readerContextCallback && g_instance->_readerContextCallback();

  if (keycode == GAMEBRICK_V2_BTN_A) {
    mappedBtn = inReader ? HalGPIO::BTN_RIGHT : HalGPIO::BTN_CONFIRM;
  } else if (keycode == GAMEBRICK_V2_BTN_B) {
    mappedBtn = inReader ? HalGPIO::BTN_LEFT : HalGPIO::BTN_BACK;
  } else if (joyLeft) {
    mappedBtn = HalGPIO::BTN_LEFT;
  } else if (joyRight) {
    mappedBtn = HalGPIO::BTN_RIGHT;
  } else if (centerPressed) {
    mappedBtn = HalGPIO::BTN_CONFIRM;
  }

  const bool isPressed = (keycode != 0) || joyLeft || joyRight;

  LOG_DBG("BT", "Game Brick V2: counter=%u frozen=%d joy=0x%02X key=0x%02X btn=%d",
          counter, counterFrozen, joystickX, keycode, mappedBtn);

  // Counter-freeze = button held down. Only inject on first press (counter advancing).
  // The 90ms cooldown in injectWithCooldown provides additional protection.
  if (counterFrozen && keycode == device->lastGameBrickActiveKey) {
    return;  // held-button repeat — suppress
  }
  device->lastGameBrickActiveKey = keycode;

  if (isPressed && mappedBtn != 0xFF) {
    // Schedule synthetic release ~80ms after press
    device->pendingGameBrickRelease = true;
    device->pendingGameBrickReleaseMs = millis() + 80;
    device->pendingGameBrickKeycode = keycode;
    device->pendingGameBrickButton = mappedBtn;

    injectWithCooldown(device, keycode, mappedBtn);
  }
}

// Free2/Free3 rolling-keycode handler.
// Uses mapCommonCodeToDirection to classify direction regardless of which rolling code fires.
// On all-zero report, clears normalized direction latch.
void BluetoothHIDManager::handleFree2(ConnectedDevice* device, uint8_t* pData, size_t length) {
  if (length < 3) return;

  // Scan report bytes for a recognized page-turn code
  uint8_t keycode = 0x00;
  const size_t scanLen = length < 8 ? length : 8;
  for (size_t i = 0; i < scanLen; i++) {
    if (DeviceProfiles::isCommonPageTurnCode(pData[i])) {
      keycode = pData[i];
      break;
    }
  }

  const bool isPressed = (keycode != 0x00);

  if (isPressed) {
    bool pageForward = false;
    if (DeviceProfiles::mapCommonCodeToDirection(keycode, pageForward)) {
      // Update normalized direction latch
      device->lastNormalizedDirection = pageForward ? 1 : 2;
      device->lastNormalizedKeycode = keycode;

      const uint8_t btn = pageForward ? HalGPIO::BTN_RIGHT : HalGPIO::BTN_LEFT;
      LOG_DBG("BT", "Free2: key=0x%02X -> %s", keycode, pageForward ? "forward" : "back");
      injectWithCooldown(device, keycode, btn);
    }
  } else {
    // Release: clear latch
    device->lastNormalizedDirection = 0;
  }
}

// Generic handler for standard HID devices and auto-detect mode.
// Preserves press-transition logic from original onHIDNotify.
void BluetoothHIDManager::handleGeneric(ConnectedDevice* device, uint8_t* pData, size_t length) {
  uint8_t keycode = 0xFF;
  bool isPressed = false;

  if (device->profile) {
    // Profile-based extraction
    if (length >= device->profile->reportByteIndex + 1u) {
      keycode = pData[device->profile->reportByteIndex];
    }
    isPressed = (keycode != 0x00);
    LOG_DBG("BT", "Generic (profile %s): keycode=0x%02X, pressed=%d",
            device->profile->name, keycode, isPressed);
  } else {
    // Auto-detect mode
    keycode = extractGenericPageTurnKeycode(pData, length);
    if (length >= 5 && (keycode == 0x07 || keycode == 0x09)) {
      isPressed = ((pData[0] & 0x01) != 0) || (keycode != 0x00);
      LOG_DBG("BT", "Auto-detect (GameBrick-like): keycode=0x%02X, pressed=%d", keycode, isPressed);
    } else {
      isPressed = (keycode != 0x00);
      LOG_DBG("BT", "Auto-detect (generic HID): keycode=0x%02X, pressed=%d", keycode, isPressed);
    }
  }

  // Enable injection after first release (startup noise gate)
  if (!isPressed && !device->hasSeenRelease) {
    device->hasSeenRelease = true;
    LOG_DBG("BT", "First release detected, enabling button injection for device %s",
            device->address.c_str());
  }

  // Ignore invalid keycodes
  if (keycode == 0x00 || keycode == 0xFF) {
    device->lastButtonState = isPressed;
    device->lastHIDKeycode = keycode;
    return;
  }

  // Block all injections until first release seen
  if (!device->hasSeenRelease) {
    LOG_DBG("BT", "Startup gate active: ignoring keycode 0x%02X (waiting for first release)", keycode);
    device->lastButtonState = isPressed;
    device->lastHIDKeycode = keycode;
    return;
  }

  // Detect PRESS transition (new press or key change while still held)
  const bool isNewPressEvent = isPressed &&
    (!device->lastButtonState || keycode != device->lastHIDKeycode);

  if (isNewPressEvent) {
    LOG_INF("BT", ">>> BUTTON PRESSED: keycode=0x%02X <<<", keycode);
    uint8_t btn = g_instance->mapKeycodeToButton(keycode, device);
    injectWithCooldown(device, keycode, btn);

    if (g_instance->_inputCallback) {
      g_instance->_inputCallback(keycode);
    }
  }

  device->lastButtonState = isPressed;
  device->lastHIDKeycode = keycode;
}

// ---- HID Notification Callback ----

void BluetoothHIDManager::onHIDNotify(NimBLERemoteCharacteristic* pChar,
                                       uint8_t* pData, size_t length, bool isNotify) {
  if (!g_instance || !pData || length == 0) return;

  // Minimal logging — avoid blocking serial inside callback
  LOG_DBG("BT", "HID %d bytes", length);

  // Resolve sending device — use portENTER_CRITICAL for spinlock on single-core ESP32-C3.
  // Keep critical section minimal: device lookup only, not full processing.
  ConnectedDevice* device = nullptr;
  {
    portENTER_CRITICAL(&g_instance->_devicesMux);
    if (pChar && pChar->getRemoteService()) {
      auto client = pChar->getRemoteService()->getClient();
      if (client) {
        std::string deviceAddr = client->getPeerAddress().toString();
        device = g_instance->findConnectedDevice(deviceAddr);
      }
    }
    if (device) device->lastActivityTime = millis();
    portEXIT_CRITICAL(&g_instance->_devicesMux);
  }

  if (!device || length < 2) return;

  // Dispatch by profile type
  if (profileIs(device, "IINE Game Brick V2")) {
    handleGameBrickV2(device, pData, length);
  } else if (profileIs(device, "IINE Game Brick")) {
    handleGameBrickV1(device, pData, length);
  } else if (device->name.find("Free") != std::string::npos ||
             profileIs(device, "Free2 Style") ||
             profileIs(device, "Free2-M") ||
             profileIs(device, "Free3-M")) {
    handleFree2(device, pData, length);
  } else {
    handleGeneric(device, pData, length);
  }
}

// ---- HID Report Parsing ----

uint16_t BluetoothHIDManager::parseHIDReport(uint8_t* data, size_t length) {
  if (length < 3) {
    LOG_ERR("BT", "Invalid HID report length: %d", length);
    return 0;
  }

  uint8_t modifier = data[0];
  uint8_t keycode = data[2];

  if (keycode == 0 && modifier == 0) {
    return 0;
  }

  LOG_INF("BT", "HID Report: mod=0x%02X key=0x%02X", modifier, keycode);

  return (static_cast<uint16_t>(modifier) << 8) | keycode;
}

// ---- Keycode to Button Mapping ----

uint8_t BluetoothHIDManager::mapKeycodeToButton(uint8_t keycode, const ConnectedDevice* device) {
  if (keycode != 0x00) {
    LOG_DBG("BT", "mapKeycodeToButton() called with keycode: 0x%02X", keycode);
  }

  // Extract base profile pointer
  const DeviceProfiles::DeviceProfile* profile = device ? device->profile : nullptr;

  // Per-device custom profile (highest priority, overrides profile codes)
  if (device) {
    DeviceProfiles::DeviceProfile perDevProfile;
    if (DeviceProfiles::getCustomProfileForDevice(device->address, perDevProfile)) {
      if (keycode == perDevProfile.pageUpCode) {
        LOG_INF("BT", "Matched per-device pageUpCode 0x%02X -> PageBack", keycode);
        return HalGPIO::BTN_UP;
      }
      if (keycode == perDevProfile.pageDownCode) {
        LOG_INF("BT", "Matched per-device pageDownCode 0x%02X -> PageForward", keycode);
        return HalGPIO::BTN_DOWN;
      }
    }
  }

  // Profile-specific mapping (second priority)
  if (profile) {
    if (keycode == profile->pageUpCode) {
      LOG_INF("BT", "Matched profile pageUpCode 0x%02X (%s) -> PageBack", keycode, profile->name);
      return HalGPIO::BTN_UP;
    } else if (keycode == profile->pageDownCode) {
      LOG_INF("BT", "Matched profile pageDownCode 0x%02X (%s) -> PageForward", keycode, profile->name);
      return HalGPIO::BTN_DOWN;
    } else {
      LOG_DBG("BT", "Keycode 0x%02X not in profile %s (expecting 0x%02X/0x%02X), ignoring",
              keycode, profile->name, profile->pageUpCode, profile->pageDownCode);
      return 0xFF;
    }
  }

  // Global custom profile (user-configured, third priority)
  if (const auto* customProfile = DeviceProfiles::getCustomProfile()) {
    if (keycode == customProfile->pageUpCode) {
      LOG_INF("BT", "Mapped learned key 0x%02X -> PageBack", keycode);
      return HalGPIO::BTN_UP;
    }
    if (keycode == customProfile->pageDownCode) {
      LOG_INF("BT", "Mapped learned key 0x%02X -> PageForward", keycode);
      return HalGPIO::BTN_DOWN;
    }
  }

  // Navigation keys → map to actual device buttons
  switch (keycode) {
    case 0x28:  // Enter → Confirm
      LOG_INF("BT", "Mapped Enter -> Confirm");
      return HalGPIO::BTN_CONFIRM;
    case 0x29:  // Escape → Back
      LOG_INF("BT", "Mapped Escape -> Back");
      return HalGPIO::BTN_BACK;
    case 0x52:  // Up arrow
      LOG_INF("BT", "Mapped Up -> Up");
      return HalGPIO::BTN_UP;
    case 0x51:  // Down arrow
      LOG_INF("BT", "Mapped Down -> Down");
      return HalGPIO::BTN_DOWN;
    case 0x50:  // Left arrow → page back in reader, left in menus
      LOG_INF("BT", "Mapped Left -> Left");
      return HalGPIO::BTN_LEFT;
    case 0x4F:  // Right arrow → page forward in reader, right in menus
      LOG_INF("BT", "Mapped Right -> Right");
      return HalGPIO::BTN_RIGHT;
  }

  // Generic page-turn codes (consumer control, volume, Free2 rolling codes, etc.)
  bool pageForward = false;
  if (DeviceProfiles::mapCommonCodeToDirection(keycode, pageForward)) {
    if (pageForward) {
      LOG_INF("BT", "Mapped generic key 0x%02X -> Right (page forward)", keycode);
      return HalGPIO::BTN_RIGHT;
    }
    LOG_INF("BT", "Mapped generic key 0x%02X -> Left (page back)", keycode);
    return HalGPIO::BTN_LEFT;
  }

  if (keycode == 0x00) {
    return 0xFF;
  }

  LOG_DBG("BT", "Unmapped keycode: 0x%02X (no profile)", keycode);
  return 0xFF;
}
#endif // ENABLE_BLE
