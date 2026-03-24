#ifdef ENABLE_BLE
// BluetoothHIDManager.cpp
// Core: singleton, lifecycle (enable/disable/cleanup), state persistence (stubs).
// Split from 849-line original for modularity.
// See BluetoothHIDScan.cpp, BluetoothHIDConnect.cpp, BluetoothHIDInput.cpp.

#include "BluetoothHIDManager.h"
#include <Logging.h>
#include <NimBLEDevice.h>
#include <WiFi.h>

// HID Service and characteristic UUIDs (shared across modules via extern)
const char* HID_SERVICE_UUID = "1812";
const char* HID_REPORT_UUID = "2A4D";
const char* HID_INFO_UUID = "2A4A";

// Global singleton pointer (accessed by callbacks in other modules)
BluetoothHIDManager* g_instance = nullptr;

// ---- Singleton ----

BluetoothHIDManager& BluetoothHIDManager::getInstance() {
  if (!g_instance) {
    g_instance = new BluetoothHIDManager();
    LOG_INF("BT", "BluetoothHIDManager instance created");
  }
  return *g_instance;
}

BluetoothHIDManager::BluetoothHIDManager() {
  LOG_DBG("BT", "BluetoothHIDManager constructor");
}

BluetoothHIDManager::~BluetoothHIDManager() {
  cleanup();
}

void BluetoothHIDManager::cleanup() {
  if (_enabled) {
    disable();
  }
}

// ---- Lifecycle ----

bool BluetoothHIDManager::enable() {
  if (_enabled) {
    LOG_DBG("BT", "Already enabled");
    return true;
  }

  // Guard: prevent conflict with BlePresenterManager or other NimBLE users
  // Allow re-enable if we previously initialized NimBLE (disable() keeps it alive)
  if (NimBLEDevice::isInitialized() && !_nimbleOwnedByUs) {
    LOG_ERR("BT", "NimBLE already initialized by another module — aborting");
    lastError = "BLE radio in use (Presenter?)";
    return false;
  }

  LOG_INF("BT", "Enabling Bluetooth...");

  // CRITICAL: Disable WiFi when enabling Bluetooth
  // ESP32-C3 cannot have both WiFi and BLE enabled simultaneously
  if (WiFi.getMode() != WIFI_OFF) {
    LOG_INF("BT", "Disabling WiFi to enable Bluetooth (mutual exclusion)");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
  }

  try {
    if (!_nimbleOwnedByUs) {
      // First-time init
      LOG_INF("BT", "Heap before NimBLE init: %d bytes free", ESP.getFreeHeap());
      NimBLEDevice::init("CrossPoint");
      LOG_INF("BT", "Heap after NimBLE init: %d bytes free (delta: %dKB)",
              ESP.getFreeHeap(), (int)(ESP.getFreeHeap()) / 1024);
      NimBLEDevice::setPower(ESP_PWR_LVL_P9);
      // No bonding/MITM/SC — most BLE HID remotes use "Just Works" pairing
      NimBLEDevice::setSecurityAuth(false, false, false);
      _nimbleOwnedByUs = true;
    } else {
      LOG_INF("BT", "Re-enabling with existing NimBLE stack. Free heap: %d bytes", ESP.getFreeHeap());
    }

    _enabled = true;
    lastError = "";

    LOG_INF("BT", "Bluetooth enabled. Free heap: %d bytes", ESP.getFreeHeap());
    loadState();
    return true;
  } catch (const std::exception& e) {
    LOG_ERR("BT", "Failed to enable Bluetooth: %s", e.what());
    lastError = std::string("Init failed: ") + e.what();
    _enabled = false;
    return false;
  } catch (...) {
    LOG_ERR("BT", "Failed to enable Bluetooth: unknown error");
    lastError = "Init failed: unknown error";
    _enabled = false;
    return false;
  }
}

bool BluetoothHIDManager::disable() {
  if (!_enabled) {
    LOG_DBG("BT", "Already disabled");
    return true;
  }

  LOG_INF("BT", "Disabling Bluetooth...");

  if (_scanning) {
    stopScan();
  }

  // Disconnect all devices but keep NimBLE initialized.
  // ESP32-C3 leaks ~8KB per deinit/reinit cycle — never call deinit().
  // BLE controller memory stays reserved until device restart.
  while (!_connectedDevices.empty()) {
    disconnectFromDevice(_connectedDevices[0].address);
  }

  // Stop advertising/scanning but keep NimBLE stack alive
  NimBLEDevice::getScan()->stop();
  NimBLEDevice::getScan()->clearResults();

  _enabled = false;
  lastError = "";

  LOG_INF("BT", "Bluetooth disabled (NimBLE kept alive to avoid heap leak)");
  return true;
}

// ---- State Persistence (stubs) ----

void BluetoothHIDManager::saveState() {
  LOG_DBG("BT", "Saving state (stub)");
  // Stub: would save paired devices to file
}

void BluetoothHIDManager::loadState() {
  LOG_DBG("BT", "Loading state (stub)");
  // Stub: would load paired devices from file
}
#endif // ENABLE_BLE
