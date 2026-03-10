#include "BluetoothHIDManager.h"

#include <HalGPIO.h>
#include <Logging.h>

#include <algorithm>

#include "DeviceProfiles.h"


// Global pointer for static NimBLE callbacks to reach the instance
static BluetoothHIDManager* g_btHidManager = nullptr;

// Inactivity timeout — disconnect if no HID reports for 2 minutes
static constexpr unsigned long INACTIVITY_TIMEOUT_MS = 120000;
// Activity window for power management queries
static constexpr unsigned long ACTIVITY_WINDOW_MS = 240000;
// Cooldown between button injections to prevent flooding
static constexpr unsigned long INJECTION_COOLDOWN_MS = 150;

// USB HID keyboard keycodes (Usage Page 0x07)
namespace HidKeys {
constexpr uint8_t KEY_ENTER = 0x28;
constexpr uint8_t KEY_BACKSPACE = 0x2A;
constexpr uint8_t KEY_SPACE = 0x2C;
constexpr uint8_t KEY_PAGE_UP = 0x4B;
constexpr uint8_t KEY_PAGE_DOWN = 0x4E;
constexpr uint8_t KEY_RIGHT_ARROW = 0x4F;
constexpr uint8_t KEY_LEFT_ARROW = 0x50;
constexpr uint8_t KEY_DOWN_ARROW = 0x51;
constexpr uint8_t KEY_UP_ARROW = 0x52;
}  // namespace HidKeys

// NimBLE 2.x client callbacks
class BtHidClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    LOG_INF("BLE", "Connected to %s", pClient->getPeerAddress().toString().c_str());
  }
  void onDisconnect(NimBLEClient* pClient, int reason) override {
    LOG_INF("BLE", "Disconnected (reason: %d)", reason);
  }
};
static BtHidClientCallbacks clientCallbacks;

BluetoothHIDManager::BluetoothHIDManager(HalGPIO& gpio) : gpio(gpio) { g_btHidManager = this; }

// ── Lifecycle ─────────────────────────────────────────────────────────────────

bool BluetoothHIDManager::init() {
  if (enabled) return true;

  NimBLEDevice::init("CrossPoint");
  // Enable bonding + Secure Connections — required by most BLE HID keyboards
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  enabled = true;
  LOG_INF("BLE", "NimBLE initialized");
  return true;
}

void BluetoothHIDManager::deinit() {
  if (!enabled) return;

  stopScan();
  if (client) {
    if (client->isConnected()) {
      client->disconnect();
      vTaskDelay(pdMS_TO_TICKS(500));
    }
    NimBLEDevice::deleteClient(client);
    client = nullptr;
  }
  NimBLEDevice::deinit(true);

  enabled = false;
  connected = false;
  scanning = false;
  activeProfile = nullptr;
  LOG_INF("BLE", "NimBLE deinitialized");
}

// ── Scanning (async via NimBLE 2.3.6) ─────────────────────────────────────────

void BluetoothHIDManager::startScan(uint32_t durationSec) {
  if (!enabled || scanning) return;

  clearDiscoveredDevices();

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->clearResults();
  scan->setScanCallbacks(this);
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  scan->setMaxResults(0);
  scanning = true;
  scan->start(durationSec);

  LOG_INF("BLE", "Scan started (%lu s)", durationSec);
}

void BluetoothHIDManager::stopScan() {
  if (!scanning) return;
  NimBLEDevice::getScan()->stop();
  scanning = false;
}

// ── Connection ────────────────────────────────────────────────────────────────

bool BluetoothHIDManager::connectToDevice(const NimBLEAddress& addr) {
  lastError.clear();

  if (!enabled && !init()) {
    lastError = "E0: BLE init failed";
    return false;
  }

  // Stop scan and settle
  NimBLEDevice::getScan()->stop();
  scanning = false;
  vTaskDelay(pdMS_TO_TICKS(500));

  // Clean up existing client
  if (client) {
    if (client->isConnected()) client->disconnect();
    NimBLEDevice::deleteClient(client);
    client = nullptr;
  }

  client = NimBLEDevice::createClient();
  if (!client) {
    lastError = "E1: No client slot";
    return false;
  }

  client->setClientCallbacks(&clientCallbacks);
  client->setConnectTimeout(15);

  LOG_INF("BLE", "Connecting to %s...", addr.toString().c_str());

  // Cleanup helper — frees client slot on failure to prevent leak
  auto freeClient = [&]() {
    if (client) {
      if (client->isConnected()) client->disconnect();
      NimBLEDevice::deleteClient(client);
      client = nullptr;
    }
  };

  if (!client->connect(addr)) {
    lastError = "E2: Connect timeout";
    freeClient();
    return false;
  }

  if (!client->isConnected()) {
    lastError = "E3: Dropped after connect";
    freeClient();
    return false;
  }

  // Wait for encryption/bonding handshake
  vTaskDelay(pdMS_TO_TICKS(1500));

  if (!client->isConnected()) {
    lastError = "E4a: Dropped during encryption";
    freeClient();
    return false;
  }

  // Discover services
  auto& services = client->getServices(true);
  LOG_INF("BLE", "Found %d services", (int)services.size());

  if (!client->isConnected()) {
    lastError = "E4b: Dropped in svc discovery";
    freeClient();
    return false;
  }

  // Find HID service (UUID 0x1812)
  NimBLERemoteService* hidService = client->getService(NimBLEUUID((uint16_t)0x1812));
  if (!hidService) {
    char buf[48];
    snprintf(buf, sizeof(buf), "E5: No HID svc (%d svcs)", (int)services.size());
    lastError = buf;
    freeClient();
    return false;
  }

  // Subscribe to HID Report characteristics (UUID 0x2A4D)
  const auto& chars = hidService->getCharacteristics(true);
  if (!client->isConnected()) {
    lastError = "E6: Dropped in char discovery";
    freeClient();
    return false;
  }

  bool subscribed = false;
  for (auto* ch : chars) {
    if (ch->getUUID() == NimBLEUUID((uint16_t)0x2A4D) && (ch->canNotify() || ch->canIndicate())) {
      if (ch->subscribe(true, onHidNotifyStatic)) {
        subscribed = true;
        LOG_INF("BLE", "Subscribed to HID Report char");
      }
    }
  }

  if (!subscribed) {
    char buf[48];
    snprintf(buf, sizeof(buf), "E7: No HID reports (%d chars)", (int)chars.size());
    lastError = buf;
    freeClient();
    return false;
  }

  // Store connected device info and find matching profile
  connectedDeviceMac = addr.toString();
  // Try to find device name from discovered list
  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    for (const auto& d : discoveredDevices) {
      if (d.address == addr) {
        connectedDeviceName = d.name;
        break;
      }
    }
  }
  activeProfile = findDeviceProfile(connectedDeviceMac.c_str(), connectedDeviceName.c_str());
  LOG_INF("BLE", "Device profile: %s (byte[%d])", activeProfile->name, activeProfile->reportByteIndex);

  connected = true;
  lastHidKeycode = 0;
  lastButtonState = false;
  lastActivityTime = millis();
  LOG_INF("BLE", "Paired OK to %s", addr.toString().c_str());
  return true;
}

void BluetoothHIDManager::disconnect() {
  if (client) {
    if (client->isConnected()) client->disconnect();
    NimBLEDevice::deleteClient(client);
    client = nullptr;
  }
  connected = false;
  shouldAutoReconnect = false;
  activeProfile = nullptr;
}

// ── Discovered devices ────────────────────────────────────────────────────────

std::vector<BleDiscoveredDevice> BluetoothHIDManager::getDiscoveredDevices() {
  std::lock_guard<std::mutex> lock(devicesMutex);
  return discoveredDevices;
}

void BluetoothHIDManager::clearDiscoveredDevices() {
  std::lock_guard<std::mutex> lock(devicesMutex);
  discoveredDevices.clear();
}

// ── Auto-reconnect & WiFi mutual exclusion ────────────────────────────────────

void BluetoothHIDManager::autoReconnect(const char* bondedAddr, uint8_t addrType) {
  if (!enabled || strlen(bondedAddr) == 0) return;
  reconnectAddress = NimBLEAddress(std::string(bondedAddr), addrType);
  reconnectPending = true;
  shouldAutoReconnect = true;
  LOG_INF("BLE", "Auto-reconnect queued for %s", bondedAddr);
}

void BluetoothHIDManager::suspend() {
  if (!enabled) return;
  suspended = true;
  deinit();
  LOG_INF("BLE", "Suspended for WiFi");
}

void BluetoothHIDManager::resume() {
  if (!suspended) return;
  suspended = false;
  init();
  if (shouldAutoReconnect) reconnectPending = true;
  LOG_INF("BLE", "Resumed after WiFi");
}

void BluetoothHIDManager::tick() {
  // Detect connection loss
  if (connected && client && !client->isConnected()) {
    connected = false;
    activeProfile = nullptr;
    LOG_INF("BLE", "Connection lost, will auto-reconnect");
    if (shouldAutoReconnect) reconnectPending = true;
  }

  // Inactivity timeout — disconnect if no HID input for 2 minutes
  if (connected && lastActivityTime > 0 && (millis() - lastActivityTime > INACTIVITY_TIMEOUT_MS)) {
    LOG_INF("BLE", "Inactivity timeout, disconnecting");
    disconnect();
    if (shouldAutoReconnect) reconnectPending = true;
  }

  if (!enabled || connected || !reconnectPending) return;
  reconnectPending = false;
  LOG_INF("BLE", "Attempting auto-reconnect...");
  connectToDevice(reconnectAddress);
}

bool BluetoothHIDManager::hasRecentActivity() const {
  return lastActivityTime > 0 && (millis() - lastActivityTime < ACTIVITY_WINDOW_MS);
}

// ── NimBLE 2.3.6 Scan Callbacks ───────────────────────────────────────────────

void BluetoothHIDManager::onResult(const NimBLEAdvertisedDevice* device) {
  std::lock_guard<std::mutex> lock(devicesMutex);

  std::string name = device->getName();

  // Update existing entry if already discovered
  for (auto& d : discoveredDevices) {
    if (d.address == device->getAddress()) {
      if (!name.empty() && d.name.find(':') != std::string::npos) d.name = name;
      d.rssi = device->getRSSI();
      if (!d.advertisesHid) d.advertisesHid = device->isAdvertisingService(NimBLEUUID((uint16_t)0x1812));
      return;
    }
  }

  BleDiscoveredDevice d;
  d.name = name.empty() ? device->getAddress().toString() : name;
  d.address = device->getAddress();
  d.rssi = device->getRSSI();
  d.advertisesHid = device->isAdvertisingService(NimBLEUUID((uint16_t)0x1812));
  discoveredDevices.push_back(d);

  LOG_INF("BLE", "Found: %s (%d dBm) HID:%d", d.name.c_str(), d.rssi, d.advertisesHid);
}

void BluetoothHIDManager::onScanEnd(const NimBLEScanResults& results, int reason) {
  scanning = false;
  std::lock_guard<std::mutex> lock(devicesMutex);
  // Sort: HID devices first, then by signal strength
  std::sort(discoveredDevices.begin(), discoveredDevices.end(),
            [](const BleDiscoveredDevice& a, const BleDiscoveredDevice& b) {
              if (a.advertisesHid != b.advertisesHid) return a.advertisesHid;
              return a.rssi > b.rssi;
            });
  LOG_INF("BLE", "Scan ended, found %zu devices", discoveredDevices.size());
}

// ── HID Report Parsing (device-profile-aware) ────────────────────────────────

void BluetoothHIDManager::onHidNotifyStatic(NimBLERemoteCharacteristic* ch, uint8_t* data, size_t len, bool isNotify) {
  if (g_btHidManager && data && len > 0) {
    g_btHidManager->onHidReport(data, len);
  }
}

void BluetoothHIDManager::onHidReport(const uint8_t* data, size_t len) {
  if (len < 2) return;

  lastActivityTime = millis();

  uint8_t keycode = 0;
  bool isPressed = false;

  // Use device profile's reportByteIndex if we have a matched profile
  const uint8_t byteIdx = activeProfile ? activeProfile->reportByteIndex : 2;

  if (activeProfile && activeProfile->isConsumerPage && len == 2) {
    // Consumer Control: 2-byte report [usage_lo, usage_hi]
    keycode = data[0];
    isPressed = (keycode != 0x00);
  } else if (len > byteIdx) {
    // Standard keyboard: extract keycode from profile-specified byte
    keycode = data[byteIdx];
    isPressed = (keycode != 0x00);

    // Fallback: IINE Game Brick format (byte[4]) — only for IINE profile
    if (keycode == 0x00 && len >= 5 && activeProfile && activeProfile->reportByteIndex == 4 &&
        (data[4] == 0x07 || data[4] == 0x09)) {
      keycode = data[4];
      isPressed = (data[0] & 0x01) != 0;
    }
  }

  if (keycode == 0x00 || keycode == 0xFF) {
    lastButtonState = isPressed;
    lastHidKeycode = keycode;
    return;
  }

  // Detect PRESS transition (was released → now pressed) — one-shot injection
  if (isPressed && !lastButtonState) {
    uint8_t btn = mapKeycodeToButton(keycode);
    if (btn != 0xFF) {
      unsigned long now = millis();
      if (now - lastInjectionTime >= INJECTION_COOLDOWN_MS) {
        LOG_INF("BLE", "Key 0x%02X -> btn %d (profile: %s)", keycode, btn,
                activeProfile ? activeProfile->name : "none");
        gpio.injectButtonPress(btn);
        lastInjectionTime = now;
      }
    }
  }

  lastButtonState = isPressed;
  lastHidKeycode = keycode;
}

uint8_t BluetoothHIDManager::mapKeycodeToButton(uint8_t keycode) {
  // Check device-specific page codes first (from profile)
  if (activeProfile) {
    if (keycode == activeProfile->pageDownCode) return HalGPIO::BTN_DOWN;
    if (keycode == activeProfile->pageUpCode) return HalGPIO::BTN_UP;
  }

  // Standard keyboard keycodes
  switch (keycode) {
    case HidKeys::KEY_RIGHT_ARROW:
    case HidKeys::KEY_PAGE_DOWN:
    case HidKeys::KEY_DOWN_ARROW:
      return HalGPIO::BTN_DOWN;
    case HidKeys::KEY_LEFT_ARROW:
    case HidKeys::KEY_PAGE_UP:
    case HidKeys::KEY_UP_ARROW:
      return HalGPIO::BTN_UP;
    case HidKeys::KEY_ENTER:
    case HidKeys::KEY_SPACE:
      return HalGPIO::BTN_CONFIRM;
    case HidKeys::KEY_BACKSPACE:
      return HalGPIO::BTN_BACK;
    // Consumer Control page-turner codes
    case CONSUMER_PAGE_UP:
      return HalGPIO::BTN_DOWN;
    case CONSUMER_PAGE_DOWN:
      return HalGPIO::BTN_UP;
    // IINE Game Brick codes
    case 0x09:
      return HalGPIO::BTN_DOWN;
    case 0x07:
      return HalGPIO::BTN_UP;
    default:
      return 0xFF;
  }
}
