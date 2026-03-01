#include "BleRemoteManager.h"

#include <HalGPIO.h>
#include <Logging.h>

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
  NimBLEDevice::setSecurityAuth(true, false, true);  // bonding, no MITM, secure connections
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);

  enabled = true;
  LOG_DBG("BLE", "NimBLE initialized");
  return true;
}

void BleRemoteManager::deinit() {
  if (!enabled) return;

  stopScan();
  disconnect();
  NimBLEDevice::deinit(true);

  enabled = false;
  connected = false;
  scanning = false;
  client = nullptr;
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
  if (!enabled) return false;

  stopScan();

  // Reuse existing client or create new one
  client = NimBLEDevice::getClientByPeerAddress(addr);
  if (!client) {
    client = NimBLEDevice::createClient(addr);
    if (!client) {
      LOG_ERR("BLE", "Failed to create client");
      return false;
    }
  }

  client->setClientCallbacks(this, false);
  client->setConnectionParams(12, 12, 0, 150);
  client->setConnectTimeout(5);

  if (!client->connect(addr)) {
    LOG_ERR("BLE", "Connection failed to %s", addr.toString().c_str());
    return false;
  }

  // Discover HID service (UUID 0x1812)
  NimBLERemoteService* hidService = client->getService(NimBLEUUID((uint16_t)0x1812));
  if (!hidService) {
    LOG_ERR("BLE", "HID service not found");
    client->disconnect();
    return false;
  }

  // Subscribe to all HID Report Input characteristics (UUID 0x2A4D)
  auto& chars = hidService->getCharacteristics(true);
  bool subscribed = false;
  for (auto* ch : chars) {
    if (ch->getUUID() == NimBLEUUID((uint16_t)0x2A4D) && ch->canNotify()) {
      if (ch->subscribe(true, [this](NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
            onHidReport(data, len);
          })) {
        subscribed = true;
        LOG_DBG("BLE", "Subscribed to HID Report characteristic");
      }
    }
  }

  if (!subscribed) {
    LOG_ERR("BLE", "No HID Report characteristics found");
    client->disconnect();
    return false;
  }

  // connected flag is set by onConnect() callback
  LOG_DBG("BLE", "Connected to %s", addr.toString().c_str());
  return true;
}

void BleRemoteManager::disconnect() {
  if (client && client->isConnected()) {
    client->disconnect();
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
  // Filter: only HID devices (service UUID 0x1812)
  if (device->isAdvertisingService(NimBLEUUID((uint16_t)0x1812))) {
    std::lock_guard<std::mutex> lock(devicesMutex);

    // Avoid duplicates
    for (const auto& d : discoveredDevices) {
      if (d.address == device->getAddress()) return;
    }

    BleDiscoveredDevice d;
    d.name = device->getName();
    if (d.name.empty()) d.name = device->getAddress().toString();
    d.address = device->getAddress();
    d.rssi = device->getRSSI();
    discoveredDevices.push_back(d);

    LOG_DBG("BLE", "Found HID device: %s (%d dBm)", d.name.c_str(), d.rssi);
  }
}

void BleRemoteManager::onScanEnd(const NimBLEScanResults& results, int reason) {
  scanning = false;
  std::lock_guard<std::mutex> lock(devicesMutex);
  LOG_DBG("BLE", "Scan ended, found %zu HID devices", discoveredDevices.size());
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
