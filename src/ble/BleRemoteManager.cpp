#include "BleRemoteManager.h"

#include <HalGPIO.h>
#include <Logging.h>

#include <algorithm>

// Prevent Arduino from releasing BLE controller memory at boot.
// NimBLE-Arduino 2.x doesn't include esp32-hal-bt-mem.h, so btInUse() returns
// false and initArduino() frees BLE memory via esp_bt_controller_mem_release().
// Subsequent NimBLEDevice::init() → esp_bt_controller_init() then crashes.
extern "C" bool btInUse(void) { return true; }

// USB HID keyboard keycodes (Usage Page 0x07)
namespace BleHidKeys {
constexpr uint8_t KEY_ENTER = 0x28;
constexpr uint8_t KEY_BACKSPACE = 0x2A;
constexpr uint8_t KEY_SPACE = 0x2C;
constexpr uint8_t KEY_PAGE_UP = 0x4B;
constexpr uint8_t KEY_PAGE_DOWN = 0x4E;
constexpr uint8_t KEY_RIGHT_ARROW = 0x4F;
constexpr uint8_t KEY_LEFT_ARROW = 0x50;
constexpr uint8_t KEY_DOWN_ARROW = 0x51;
constexpr uint8_t KEY_UP_ARROW = 0x52;
}  // namespace BleHidKeys

// USB HID Consumer Control usage IDs (Usage Page 0x0C)
namespace BleConsumerKeys {
constexpr uint16_t SCAN_NEXT_TRACK = 0x00B5;
constexpr uint16_t SCAN_PREV_TRACK = 0x00B6;
constexpr uint16_t PLAY_PAUSE = 0x00CD;
constexpr uint16_t VOLUME_UP = 0x00E9;
constexpr uint16_t VOLUME_DOWN = 0x00EA;
constexpr uint16_t AC_BACK = 0x0224;
constexpr uint16_t AC_FORWARD = 0x0225;
}  // namespace BleConsumerKeys

// Default keycode -> HalGPIO button mapping for page-turn remotes
struct BleKeyMapping {
  uint8_t hidKeycode;
  uint8_t gpioButton;
};

// Consumer Control usage -> HalGPIO button mapping
struct BleConsumerMapping {
  uint16_t usageId;
  uint8_t gpioButton;
};

static const BleKeyMapping defaultKeyMap[] = {
    {BleHidKeys::KEY_RIGHT_ARROW, HalGPIO::BTN_RIGHT}, {BleHidKeys::KEY_LEFT_ARROW, HalGPIO::BTN_LEFT},
    {BleHidKeys::KEY_DOWN_ARROW, HalGPIO::BTN_DOWN},   {BleHidKeys::KEY_UP_ARROW, HalGPIO::BTN_UP},
    {BleHidKeys::KEY_PAGE_DOWN, HalGPIO::BTN_DOWN},    {BleHidKeys::KEY_PAGE_UP, HalGPIO::BTN_UP},
    {BleHidKeys::KEY_ENTER, HalGPIO::BTN_CONFIRM},     {BleHidKeys::KEY_BACKSPACE, HalGPIO::BTN_BACK},
    {BleHidKeys::KEY_SPACE, HalGPIO::BTN_RIGHT},
};

static const BleConsumerMapping consumerKeyMap[] = {
    {BleConsumerKeys::SCAN_NEXT_TRACK, HalGPIO::BTN_RIGHT},
    {BleConsumerKeys::SCAN_PREV_TRACK, HalGPIO::BTN_LEFT},
    {BleConsumerKeys::AC_FORWARD, HalGPIO::BTN_RIGHT},
    {BleConsumerKeys::AC_BACK, HalGPIO::BTN_BACK},
    {BleConsumerKeys::VOLUME_UP, HalGPIO::BTN_UP},
    {BleConsumerKeys::VOLUME_DOWN, HalGPIO::BTN_DOWN},
    {BleConsumerKeys::PLAY_PAUSE, HalGPIO::BTN_CONFIRM},
};

BleRemoteManager::BleRemoteManager(HalGPIO& gpio) : gpio(gpio) {}

bool BleRemoteManager::init() {
  if (enabled) return true;

  NimBLEDevice::init("CrossPoint");
  // No security requirements during init — NimBLE auto-negotiates encryption
  // when the remote HID service requires it during characteristic access
  NimBLEDevice::setSecurityAuth(false, false, false);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // max power for better range

  enabled = true;
  LOG_DBG("BLE", "NimBLE initialized");
  return true;
}

void BleRemoteManager::deinit() {
  if (!enabled) return;

  stopScan();
  // disconnect() already deletes and nulls client
  if (client && client->isConnected()) {
    client->disconnect();
  }
  client = nullptr;  // NimBLEDevice::deinit(true) will free all client memory
  NimBLEDevice::deinit(true);

  enabled = false;
  connected = false;
  scanning = false;
  LOG_DBG("BLE", "NimBLE deinitialized");
}

void BleRemoteManager::startScan(uint32_t durationSec) {
  if (!enabled || scanning) return;

  clearDiscoveredDevices();

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(this);
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  scanning = true;  // Set before start to avoid race with onScanEnd callback
  scan->start(durationSec, false);  // non-blocking

  LOG_DBG("BLE", "Scan started for %lu seconds", durationSec);
}

void BleRemoteManager::stopScan() {
  if (!scanning) return;

  NimBLEDevice::getScan()->stop();
  scanning = false;
  LOG_DBG("BLE", "Scan stopped");
}

bool BleRemoteManager::connectToDevice(const NimBLEAddress& addr) {
  lastError.clear();

  if (!enabled) {
    // Re-init BLE if it was disabled (e.g. main loop deinit'd while settings toggle is off)
    if (!init()) {
      lastError = "E0: BLE init failed";
      return false;
    }
  }

  // Force-stop scan through NimBLE API directly (not just our wrapper)
  NimBLEDevice::getScan()->stop();
  scanning = false;
  vTaskDelay(pdMS_TO_TICKS(500));

  // Delete ALL existing clients to free the single connection slot
  if (client) {
    if (client->isConnected()) client->disconnect();
    NimBLEDevice::deleteClient(client);
    client = nullptr;
  }
  // Also clean up any client NimBLE may have for this address
  NimBLEClient* existing = NimBLEDevice::getClientByPeerAddress(addr);
  if (existing) {
    NimBLEDevice::deleteClient(existing);
  }

  client = NimBLEDevice::createClient(addr);
  if (!client) {
    lastError = "E1: No client slot";
    LOG_ERR("BLE", "%s", lastError.c_str());
    return false;
  }

  client->setClientCallbacks(this, false);
  client->setConnectTimeout(12);

  LOG_DBG("BLE", "Connecting to %s (type %d)...", addr.toString().c_str(), addr.getType());

  // Use connect() without address — uses the address passed to createClient
  if (!client->connect()) {
    lastError = "E2: Connect timeout";
    LOG_ERR("BLE", "%s to %s", lastError.c_str(), addr.toString().c_str());
    NimBLEDevice::deleteClient(client);
    client = nullptr;
    return false;
  }

  if (!client->isConnected()) {
    lastError = "E3: Dropped after connect";
    LOG_ERR("BLE", "%s", lastError.c_str());
    NimBLEDevice::deleteClient(client);
    client = nullptr;
    return false;
  }

  LOG_DBG("BLE", "Connected OK, discovering services...");
  vTaskDelay(pdMS_TO_TICKS(500));

  const auto& services = client->getServices(true);
  if (!client->isConnected()) {
    lastError = "E4: Dropped in svc discovery";
    LOG_ERR("BLE", "%s", lastError.c_str());
    NimBLEDevice::deleteClient(client);
    client = nullptr;
    return false;
  }

  LOG_DBG("BLE", "Found %d services", (int)services.size());

  NimBLERemoteService* hidService = client->getService(NimBLEUUID((uint16_t)0x1812));
  if (!hidService) {
    char buf[48];
    snprintf(buf, sizeof(buf), "E5: No HID svc (%d svcs)", (int)services.size());
    lastError = buf;
    LOG_ERR("BLE", "%s", lastError.c_str());
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    client = nullptr;
    return false;
  }

  const auto& chars = hidService->getCharacteristics(true);
  if (!client->isConnected()) {
    lastError = "E6: Dropped in char discovery";
    LOG_ERR("BLE", "%s", lastError.c_str());
    NimBLEDevice::deleteClient(client);
    client = nullptr;
    return false;
  }

  bool subscribed = false;
  for (auto* ch : chars) {
    if (ch->getUUID() == NimBLEUUID((uint16_t)0x2A4D) && ch->canNotify()) {
      if (ch->subscribe(true, [this](NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
            onHidReport(data, len);
          })) {
        subscribed = true;
        LOG_DBG("BLE", "Subscribed to HID Report");
      }
    }
  }

  if (!subscribed) {
    char buf[48];
    snprintf(buf, sizeof(buf), "E7: No HID reports (%d chars)", (int)chars.size());
    lastError = buf;
    LOG_ERR("BLE", "%s", lastError.c_str());
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    client = nullptr;
    return false;
  }

  // connected flag is set by onConnect() callback
  LOG_DBG("BLE", "Connected to %s", addr.toString().c_str());
  return true;
}

void BleRemoteManager::disconnect() {
  if (client) {
    if (client->isConnected()) {
      client->disconnect();
    }
    NimBLEDevice::deleteClient(client);
    client = nullptr;
  }
  connected = false;
  shouldAutoReconnect = false;
  gpio.setBleButtonState(0);
}

std::vector<BleDiscoveredDevice> BleRemoteManager::getDiscoveredDevices() {
  std::lock_guard<std::mutex> lock(devicesMutex);
  return discoveredDevices;
}

void BleRemoteManager::clearDiscoveredDevices() {
  std::lock_guard<std::mutex> lock(devicesMutex);
  discoveredDevices.clear();
}

void BleRemoteManager::autoReconnect(const char* bondedAddr, uint8_t addrType) {
  if (!enabled || strlen(bondedAddr) == 0) return;

  reconnectAddress = NimBLEAddress(std::string(bondedAddr), addrType);
  reconnectPending = true;
  shouldAutoReconnect = true;
  LOG_DBG("BLE", "Auto-reconnect queued for %s (type %d)", bondedAddr, addrType);
}

void BleRemoteManager::suspend() {
  if (!enabled) return;
  suspended = true;
  deinit();
  LOG_DBG("BLE", "Suspended for WiFi");
}

void BleRemoteManager::resume() {
  if (!suspended) return;
  suspended = false;
  init();

  // Re-queue auto-reconnect to bonded device after WiFi suspension
  if (shouldAutoReconnect) {
    reconnectPending = true;
  }
  LOG_DBG("BLE", "Resumed after WiFi");
}

void BleRemoteManager::tick() {
  if (!enabled || connected || !reconnectPending) return;

  reconnectPending = false;
  LOG_DBG("BLE", "Attempting auto-reconnect...");
  connectToDevice(reconnectAddress);
}

// --- NimBLE Callbacks ---

void BleRemoteManager::onResult(const NimBLEAdvertisedDevice* device) {
  // Show all discovered BLE devices — many keyboards/remotes don't advertise
  // HID service UUID or appearance. Connection will verify HID support.
  std::lock_guard<std::mutex> lock(devicesMutex);

  std::string name = device->getName();

  // Update existing entry if we now have a name (scan response arrived later)
  for (auto& d : discoveredDevices) {
    if (d.address == device->getAddress()) {
      if (!name.empty() && d.name.find(':') != std::string::npos) {
        d.name = name;  // replace address placeholder with real name
      }
      d.rssi = device->getRSSI();  // update signal strength
      if (!d.advertisesHid) {
        d.advertisesHid = device->isAdvertisingService(NimBLEUUID((uint16_t)0x1812));
      }
      return;
    }
  }

  BleDiscoveredDevice d;
  d.name = name.empty() ? device->getAddress().toString() : name;
  d.address = device->getAddress();
  d.rssi = device->getRSSI();
  d.advertisesHid = device->isAdvertisingService(NimBLEUUID((uint16_t)0x1812));
  discoveredDevices.push_back(d);

  LOG_DBG("BLE", "Found: %s (%d dBm)", d.name.c_str(), d.rssi);
}

void BleRemoteManager::onScanEnd(const NimBLEScanResults& results, int reason) {
  scanning = false;
  std::lock_guard<std::mutex> lock(devicesMutex);
  // Sort: HID-advertising devices first, then by signal strength (strongest first)
  std::sort(discoveredDevices.begin(), discoveredDevices.end(),
            [](const BleDiscoveredDevice& a, const BleDiscoveredDevice& b) {
              if (a.advertisesHid != b.advertisesHid) return a.advertisesHid;
              return a.rssi > b.rssi;
            });
  LOG_DBG("BLE", "Scan ended, found %zu devices", discoveredDevices.size());
}

void BleRemoteManager::onConnect(NimBLEClient* client) {
  connected = true;
  LOG_DBG("BLE", "Connected callback");
}

void BleRemoteManager::onDisconnect(NimBLEClient* client, int reason) {
  connected = false;
  gpio.setBleButtonState(0);  // Clear all BLE button state
  LOG_DBG("BLE", "Disconnected, reason: %d", reason);
}

// --- HID Report Parsing ---

void BleRemoteManager::onHidReport(const uint8_t* data, size_t len) {
  uint8_t buttonState = 0;

  if (len >= 3) {
    // Standard keyboard HID report: [modifier, reserved, key1, key2, ..., key6]
    for (size_t i = 2; i < len && i < 8; i++) {
      uint8_t keycode = data[i];
      if (keycode == 0) continue;

      uint8_t btn = mapKeycodeToButton(keycode);
      if (btn != 0xFF) {
        buttonState |= (1 << btn);
      }
    }
  } else if (len == 2) {
    // Consumer Control HID report: [usage_lo, usage_hi] (16-bit little-endian usage ID)
    uint16_t usage = data[0] | (data[1] << 8);
    if (usage != 0) {
      uint8_t btn = mapConsumerUsageToButton(usage);
      if (btn != 0xFF) {
        buttonState |= (1 << btn);
      }
    }
  }

  gpio.setBleButtonState(buttonState);
}

uint8_t BleRemoteManager::mapKeycodeToButton(uint8_t keycode) {
  for (const auto& mapping : defaultKeyMap) {
    if (mapping.hidKeycode == keycode) {
      return mapping.gpioButton;
    }
  }
  return 0xFF;  // unmapped
}

uint8_t BleRemoteManager::mapConsumerUsageToButton(uint16_t usageId) {
  for (const auto& mapping : consumerKeyMap) {
    if (mapping.usageId == usageId) {
      return mapping.gpioButton;
    }
  }
  return 0xFF;  // unmapped
}
