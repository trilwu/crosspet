// BluetoothHIDScan.cpp
// BLE scanning: ScanCallbacks, startScan, stopScan, onScanResult.
// Part of BluetoothHIDManager — split for modularity.

#include "BluetoothHIDManager.h"
#include <Logging.h>
#include <NimBLEDevice.h>

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

  LOG_INF("BT", "Starting BLE scan for %lu ms", durationMs);
  _scanning = true;
  _discoveredDevices.clear();

  try {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (!pScan) {
      LOG_ERR("BT", "Failed to get scan object");
      _scanning = false;
      return;
    }

    LOG_DBG("BT", "Setting up scan callbacks...");
    pScan->setScanCallbacks(&scanCallbacks, false);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    LOG_DBG("BT", "Starting continuous scan (duration: 0 = continuous)...");
    bool started = pScan->start(0, false);

    if (!started) {
      LOG_ERR("BT", "Failed to start scan!");
      _scanning = false;
      return;
    }

    LOG_DBG("BT", "Scan started, waiting %lu ms...", durationMs);
    delay(durationMs);

    LOG_DBG("BT", "Stopping scan after %lu ms...", durationMs);
    pScan->stop();

    _scanning = false;
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
  std::string name = advertisedDevice->getName();
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

  // Add new device
  BluetoothDevice device;
  device.address = address;
  device.name = name.empty() ? "Unknown" : name;
  device.rssi = rssi;
  device.isHID = isHID;
  device.addressType = advertisedDevice->getAddress().getType();

  _discoveredDevices.push_back(device);

  const std::string prefix = (address.size() >= 8) ? address.substr(0, 8) : address;
  LOG_INF("BT", "Scan device: %s (%s) prefix=%s RSSI:%d HID:%d",
    device.name.c_str(), device.address.c_str(), prefix.c_str(), rssi, isHID);

  LOG_DBG("BT", "Found device: %s (%s) RSSI:%d HID:%d",
          device.name.c_str(), device.address.c_str(), rssi, isHID);
}
