// BluetoothHIDConnect.cpp
// BLE connection management: connectToDevice, disconnectFromDevice, isConnected,
// getConnectedDevices, findConnectedDevice, processInputEvents, setters, setBondedDevice.
// Part of BluetoothHIDManager — split for modularity.

#include "BluetoothHIDManager.h"
#include <Logging.h>
#include <NimBLEDevice.h>
#include <HalPowerManager.h>

// Shared UUIDs defined in BluetoothHIDManager.cpp
extern const char* HID_SERVICE_UUID;
extern const char* HID_REPORT_UUID;

// onHIDNotify declared in BluetoothHIDInput.cpp (static method on class)

// ---- Client Callbacks ----

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    LOG_INF("BT", "Client connected: %s", pClient->getPeerAddress().toString().c_str());
  }

  void onDisconnect(NimBLEClient* pClient, int reason) override {
    LOG_ERR("BT", "Client disconnected: %s (reason: %d)",
            pClient->getPeerAddress().toString().c_str(), reason);
  }
};

// ---- Connection Methods ----

bool BluetoothHIDManager::connectToDevice(const std::string& address) {
  if (!_enabled) {
    LOG_ERR("BT", "Cannot connect: Bluetooth not enabled");
    lastError = "Bluetooth not enabled";
    return false;
  }

  if (isConnected(address)) {
    LOG_INF("BT", "Already connected to %s", address.c_str());
    return true;
  }

  LOG_INF("BT", "Connecting to device %s", address.c_str());

  try {
    NimBLEClient* pClient = NimBLEDevice::createClient();
    if (!pClient) {
      lastError = "Failed to create BLE client";
      LOG_ERR("BT", "Failed to create BLE client");
      return false;
    }

    static ClientCallbacks clientCallbacks;
    pClient->setClientCallbacks(&clientCallbacks);

    // Determine address type: check scan results first, fall back to public
    uint8_t addrType = BLE_ADDR_PUBLIC;
    for (const auto& dev : _discoveredDevices) {
      if (dev.address == address) {
        addrType = dev.addressType;
        break;
      }
    }
    NimBLEAddress bleAddress(address, addrType);
    if (!pClient->connect(bleAddress)) {
      lastError = "Connection failed";
      LOG_ERR("BT", "Failed to connect to %s", address.c_str());
      NimBLEDevice::deleteClient(pClient);
      return false;
    }

    LOG_INF("BT", "Connected, discovering services...");

    NimBLERemoteService* pService = pClient->getService(HID_SERVICE_UUID);
    if (!pService) {
      lastError = "HID service not found";
      LOG_ERR("BT", "Device %s doesn't have HID service", address.c_str());
      pClient->disconnect();
      return false;
    }

    LOG_INF("BT", "Found HID service, enumerating report characteristics...");

    // Find all Report characteristics that support notify/indicate (input reports)
    auto pCharacteristics = pService->getCharacteristics(true);
    int reportCount = 0;
    std::vector<NimBLERemoteCharacteristic*> reportChars;

    for (auto it = pCharacteristics.begin(); it != pCharacteristics.end(); ++it) {
      auto* pChar = *it;
      LOG_DBG("BT", "Characteristic UUID: %s, canRead:%d canWrite:%d canNotify:%d canIndicate:%d",
              pChar->getUUID().toString().c_str(),
              pChar->canRead(), pChar->canWrite(), pChar->canNotify(), pChar->canIndicate());

      if (pChar->getUUID().equals(NimBLEUUID(HID_REPORT_UUID))) {
        reportCount++;
        LOG_INF("BT", "Found Report char #%d, notify:%d indicate:%d UUID:%s",
                reportCount, pChar->canNotify(), pChar->canIndicate(),
                pChar->getUUID().toString().c_str());

        if (pChar->canNotify() || pChar->canIndicate()) {
          reportChars.push_back(pChar);
          LOG_INF("BT", "Added Report char #%d for subscription", reportCount);
        }
      }
    }

    if (reportChars.empty()) {
      lastError = "No input report characteristic found";
      LOG_ERR("BT", "No Report characteristic with notify/indicate found");
      pClient->disconnect();
      return false;
    }

    LOG_INF("BT", "Subscribing to %d Report characteristics...", reportChars.size());

    for (size_t i = 0; i < reportChars.size(); i++) {
      auto* pChar = reportChars[i];
      LOG_INF("BT", "Subscribing to Report char #%d...", i + 1);
      bool subResult = pChar->subscribe(true, onHIDNotify);
      LOG_INF("BT", "Report char #%d subscribe result: %d", i + 1, subResult);
      if (!subResult) {
        LOG_INF("BT", "Failed to subscribe to Report char #%d (continuing)", i + 1);
      }
    }

    LOG_INF("BT", "Subscribed to %d HID Report characteristics", reportChars.size());

    // Build connected device record
    ConnectedDevice connDev;
    connDev.address = address;
    connDev.client = pClient;
    connDev.reportChars = reportChars;
    connDev.connectedTime = millis();
    connDev.subscribed = true;
    connDev.lastActivityTime = millis();
    connDev.wasConnected = true;

    // Resolve device name from scan results
    bool foundInScan = false;
    for (const auto& dev : _discoveredDevices) {
      if (dev.address == address) {
        connDev.name = dev.name;
        foundInScan = true;
        LOG_INF("BT", "Device found in scan results: %s (%s)", dev.name.c_str(), address.c_str());
        break;
      }
    }

    if (!foundInScan) {
      LOG_INF("BT", "Device not in scan results (may be previously paired): %s", address.c_str());
    }

    // Match device profile
    connDev.profile = DeviceProfiles::findDeviceProfile(address.c_str(), connDev.name.c_str());

    if (connDev.profile) {
      LOG_INF("BT", "Using device profile: %s (byte[%d] for keycode)",
              connDev.profile->name, connDev.profile->reportByteIndex);
    } else {
      LOG_INF("BT", "No known profile matched for %s, will auto-detect from HID codes", address.c_str());
    }

    _connectedDevices.push_back(connDev);

    LOG_INF("BT", "Successfully connected to %s", address.c_str());
    lastError = "Connected";
    return true;

  } catch (const std::exception& e) {
    lastError = std::string("Connection error: ") + e.what();
    LOG_ERR("BT", "%s", lastError.c_str());
    return false;
  } catch (...) {
    lastError = "Unknown connection error";
    LOG_ERR("BT", "%s", lastError.c_str());
    return false;
  }
}

bool BluetoothHIDManager::disconnectFromDevice(const std::string& address) {
  LOG_INF("BT", "Disconnecting from device %s", address.c_str());

  auto it = std::find_if(_connectedDevices.begin(), _connectedDevices.end(),
    [&address](const ConnectedDevice& dev) { return dev.address == address; });

  if (it != _connectedDevices.end()) {
    NimBLEClient* client = it->client;

    if (client && client->isConnected()) {
      try {
        HalPowerManager::Lock lock;
        LOG_DBG("BT", "Calling disconnect on client...");
        client->disconnect();
      } catch (const std::exception& e) {
        LOG_ERR("BT", "Error during disconnect: %s", e.what());
      } catch (...) {
        LOG_ERR("BT", "Unknown error during disconnect");
      }
    }

    // Erase from vector first, then delete client to avoid use-after-free
    _connectedDevices.erase(it);
    if (client) {
      NimBLEDevice::deleteClient(client);
    }
    LOG_INF("BT", "Disconnected from %s", address.c_str());
    return true;
  }

  LOG_INF("BT", "Device %s not in connected list", address.c_str());
  return false;
}

bool BluetoothHIDManager::isConnected(const std::string& address) const {
  return std::find_if(_connectedDevices.begin(), _connectedDevices.end(),
    [&address](const ConnectedDevice& dev) { return dev.address == address; }) != _connectedDevices.end();
}

std::vector<std::string> BluetoothHIDManager::getConnectedDevices() const {
  std::vector<std::string> addresses;
  for (const auto& dev : _connectedDevices) {
    addresses.push_back(dev.address);
  }
  return addresses;
}

ConnectedDevice* BluetoothHIDManager::findConnectedDevice(const std::string& address) {
  auto it = std::find_if(_connectedDevices.begin(), _connectedDevices.end(),
    [&address](const ConnectedDevice& dev) { return dev.address == address; });

  return (it != _connectedDevices.end()) ? &(*it) : nullptr;
}

// ---- Input / Callback Setters ----

void BluetoothHIDManager::processInputEvents() {
  // Input events processed via NimBLE notification callbacks — nothing to poll.
}

void BluetoothHIDManager::setInputCallback(std::function<void(uint16_t)> callback) {
  _inputCallback = callback;
  LOG_DBG("BT", "Input callback registered");
}

void BluetoothHIDManager::setButtonInjector(std::function<void(uint8_t)> injector) {
  _buttonInjector = injector;
  LOG_DBG("BT", "Button injector registered");
}

void BluetoothHIDManager::setBondedDevice(const std::string& address, const std::string& name) {
  _bondedDeviceAddress = address;
  _bondedDeviceName = name;
  LOG_INF("BT", "Bonded device set: %s (%s)", _bondedDeviceAddress.c_str(), _bondedDeviceName.c_str());
}
