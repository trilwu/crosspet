#ifdef ENABLE_BLE
// BluetoothHIDScan.cpp
// BLE scanning: ScanCallbacks, startScan, stopScan, onScanResult.
// Part of BluetoothHIDManager — split for modularity.

#include "BluetoothHIDManager.h"
#include <Logging.h>
#include <NimBLEDevice.h>
#include <algorithm>

// Shared UUIDs defined in BluetoothHIDManager.cpp
extern const char* HID_SERVICE_UUID;
extern BluetoothHIDManager* g_instance;

// ---- Scan Callbacks ----

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    LOG_DBG("BT", "onResult callback triggered!");
    if (g_instance) {
      g_instance->onScanResult(const_cast<NimBLEAdvertisedDevice*>(advertisedDevice));
    } else {
      LOG_ERR("BT", "onResult called but g_instance is NULL!");
    }
  }

  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    LOG_INF("BT", "onScanEnd callback: %d devices, reason: %d", results.getCount(), reason);
  }
};

// Static instance to keep callbacks alive during scan
static ScanCallbacks scanCallbacks;

// ---- Scan Methods ----

void BluetoothHIDManager::startScan(uint32_t durationMs) {
  if (!_enabled || _scanning) {
    LOG_DBG("BT", "Cannot scan: enabled=%d scanning=%d", _enabled, _scanning);
    return;
  }

  // Cap scan duration to 5s — crowded environments can overwhelm heap in 10s
  if (durationMs > 5000) durationMs = 5000;

  LOG_INF("BT", "Starting BLE scan. Free heap: %d bytes", ESP.getFreeHeap());
  _scanning = true;
  _discoveredDevices.clear();

  try {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (!pScan) {
      LOG_ERR("BT", "Failed to get scan object");
      _scanning = false;
      return;
    }

    pScan->setScanCallbacks(&scanCallbacks, false);
    pScan->setActiveScan(false);  // Passive scan — saves ~4KB heap vs active
    pScan->setMaxResults(8);      // Limit discovered devices to conserve heap
    pScan->setInterval(100);
    pScan->setWindow(99);

    bool started = pScan->start(0, false);
    if (!started) {
      LOG_ERR("BT", "Failed to start scan!");
      _scanning = false;
      return;
    }

    // Blocking wait — yields to FreeRTOS to prevent watchdog timeout
    const unsigned long scanEnd = millis() + durationMs;
    while (millis() < scanEnd) {
      delay(100);
    }

    pScan->stop();
    _scanning = false;

    // Sort by: HID first, then by signal strength (strongest first)
    std::sort(_discoveredDevices.begin(), _discoveredDevices.end(),
      [](const BluetoothDevice& a, const BluetoothDevice& b) {
        if (a.isHID != b.isHID) return a.isHID > b.isHID;  // HID first
        // Named devices before "Unknown"
        bool aName = !a.name.empty() && a.name != "Unknown";
        bool bName = !b.name.empty() && b.name != "Unknown";
        if (aName != bName) return aName > bName;
        return a.rssi > b.rssi;  // Strongest signal first
    });

    LOG_INF("BT", "Scan complete, found %d devices", _discoveredDevices.size());
  } catch (const std::exception& e) {
    LOG_ERR("BT", "Scan failed: %s", e.what());
    _scanning = false;
    lastError = std::string("Scan failed: ") + e.what();
  }
}

void BluetoothHIDManager::stopScan() {
  if (!_scanning) return;

  LOG_INF("BT", "Stopping scan");

  try {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (pScan) {
      pScan->stop();
    }
  } catch (...) {
    LOG_ERR("BT", "Error stopping scan");
  }

  _scanning = false;
}

void BluetoothHIDManager::onScanResult(NimBLEAdvertisedDevice* advertisedDevice) {
  if (!advertisedDevice) return;

  std::string address = advertisedDevice->getAddress().toString();
  int rssi = advertisedDevice->getRSSI();
  bool isHID = advertisedDevice->isAdvertisingService(NimBLEUUID(HID_SERVICE_UUID));

  // Update existing entry if found
  for (auto& dev : _discoveredDevices) {
    if (dev.address == address) {
      dev.rssi = rssi;
      if (isHID) dev.isHID = true;
      return;
    }
  }

  // Skip very weak signals
  if (rssi < -85 && !isHID) return;

  std::string name = advertisedDevice->getName();
  bool hasName = !name.empty();

  // Only collect named devices or HID devices — skip anonymous non-HID
  if (!hasName && !isHID) return;

  // Cap devices to prevent heap exhaustion in crowded BLE environments.
  // Prioritize: HID > named > strong signal. Replace weakest non-HID when full.
  constexpr size_t MAX_DEVICES = 16;
  if (_discoveredDevices.size() >= MAX_DEVICES) {
    if (!isHID && !hasName) return;  // Skip unnamed non-HID when full
    // Find weakest non-HID unnamed device to replace
    int weakestIdx = -1;
    int weakestRssi = 0;
    for (size_t i = 0; i < _discoveredDevices.size(); i++) {
      const auto& d = _discoveredDevices[i];
      if (!d.isHID && (d.name.empty() || d.name == "Unknown")) {
        if (weakestIdx < 0 || d.rssi < weakestRssi) {
          weakestIdx = static_cast<int>(i);
          weakestRssi = d.rssi;
        }
      }
    }
    if (weakestIdx >= 0) {
      _discoveredDevices.erase(_discoveredDevices.begin() + weakestIdx);
    } else {
      return;  // All slots occupied by HID/named devices
    }
  }

  BluetoothDevice device;
  device.address = address;
  device.name = hasName ? name : "Unknown";
  device.rssi = rssi;
  device.isHID = isHID;
  device.addressType = advertisedDevice->getAddress().getType();

  _discoveredDevices.push_back(device);
  LOG_INF("BT", "[%d] %s (%s) RSSI:%d HID:%d",
          _discoveredDevices.size(), device.name.c_str(), address.c_str(), rssi, isHID);
}
#endif // ENABLE_BLE
