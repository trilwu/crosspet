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

// ---- HID Notification Callback ----

void BluetoothHIDManager::onHIDNotify(NimBLERemoteCharacteristic* pChar,
                                       uint8_t* pData, size_t length, bool isNotify) {
  if (!g_instance || !pData || length == 0) return;

  // Minimal logging — avoid blocking serial inside callback
  LOG_DBG("BT", "HID %d bytes", length);

  // Resolve sending device — use taskENTER_CRITICAL instead of portENTER_CRITICAL
  // to avoid disabling interrupts for too long on single-core ESP32-C3
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

  uint8_t keycode = 0xFF;
  bool isPressed = false;

  if (device->profile) {
    // Profile-based extraction
    if (length >= device->profile->reportByteIndex + 1u) {
      keycode = pData[device->profile->reportByteIndex];
    }

    if (strcmp(device->profile->name, "IINE Game Brick") == 0) {
      // Game Brick: accept only stable digital-button reports (0x1x family)
      const bool stableButtonReport = (pData[0] & 0xF0) == 0x10;
      if (!stableButtonReport) {
        LOG_DBG("BT", "Game Brick: ignoring transitional report byte[0]=0x%02X, keycode=0x%02X",
                pData[0], keycode);
        device->lastButtonState = false;
        device->lastHIDKeycode = 0x00;
        return;
      }

      isPressed = (pData[0] & 0x01) != 0;

      // Ignore startup pressed frame until a clean release is seen
      if (!device->hasSeenRelease) {
        if (!isPressed) {
          device->hasSeenRelease = true;
        } else {
          LOG_DBG("BT", "Game Brick: ignoring startup pressed frame keycode=0x%02X", keycode);
          device->lastButtonState = true;
          device->lastHIDKeycode = keycode;
          return;
        }
      }

      LOG_DBG("BT", "Game Brick: byte[0]=0x%02X, keycode=0x%02X, pressed=%d",
              pData[0], keycode, isPressed);
    } else {
      // Standard HID keyboard: non-zero keycode = pressed
      isPressed = (keycode != 0x00);
      LOG_DBG("BT", "Device %s: keycode=0x%02X, pressed=%d",
              device->profile->name, keycode, isPressed);
    }
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

  // CRITICAL GATE: block all injections until first release seen
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

    if (g_instance->_buttonInjector) {
      uint8_t btn = g_instance->mapKeycodeToButton(keycode, device->profile);
      if (btn != 0xFF) {
        unsigned long now = millis();
        if ((now - device->lastInjectionTime >= 150) || (keycode != device->lastInjectedKeycode)) {
          static const char* btnNames[] = {"Back", "Confirm", "Left", "Right", "Up", "Down", "Power"};
          const char* btnName = (btn < 7) ? btnNames[btn] : "?";
          LOG_INF("BT", "Inject: key=0x%02X -> btn=%d (%s)", keycode, btn, btnName);
          g_instance->_buttonInjector(btn);
          device->lastInjectionTime = now;
          device->lastInjectedKeycode = keycode;
        } else {
          LOG_DBG("BT", "Button injection throttled (cooldown: %u ms)",
                  now - device->lastInjectionTime);
        }
      }
    }

    if (g_instance->_inputCallback) {
      g_instance->_inputCallback(keycode);
    }
  }

  device->lastButtonState = isPressed;
  device->lastHIDKeycode = keycode;

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

uint8_t BluetoothHIDManager::mapKeycodeToButton(uint8_t keycode,
                                                  const DeviceProfiles::DeviceProfile* profile) {
  if (keycode != 0x00) {
    LOG_DBG("BT", "mapKeycodeToButton() called with keycode: 0x%02X", keycode);
  }

  // Profile-specific mapping takes priority
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

  // Check user-configured custom profile
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

  // Generic page-turn codes (consumer control, volume, etc.)
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
