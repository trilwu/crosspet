#include "BlePresenterManager.h"

#include <Logging.h>

// NimBLE-Arduino 2.x doesn't include esp32-hal-bt-mem.h, so btInUse() returns
// false by default, causing Arduino to release BLE controller memory at boot.
// This override keeps the memory reserved so NimBLE can initialize.
extern "C" bool btInUse(void) { return true; }

// Standard boot-compatible keyboard HID report descriptor (Usage Page 0x01, Usage 0x06)
static const uint8_t KEYBOARD_REPORT_MAP[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  //   Report ID (1)
    // Modifier byte
    0x05, 0x07,  //   Usage Page (Key Codes)
    0x19, 0xE0,  //   Usage Minimum (224)
    0x29, 0xE7,  //   Usage Maximum (231)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x01,  //   Logical Maximum (1)
    0x75, 0x01,  //   Report Size (1)
    0x95, 0x08,  //   Report Count (8)
    0x81, 0x02,  //   Input (Data, Variable, Absolute)
    // Reserved byte
    0x95, 0x01,  //   Report Count (1)
    0x75, 0x08,  //   Report Size (8)
    0x81, 0x03,  //   Input (Constant)
    // Key array (6 keys)
    0x95, 0x06,  //   Report Count (6)
    0x75, 0x08,  //   Report Size (8)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x65,  //   Logical Maximum (101)
    0x05, 0x07,  //   Usage Page (Key Codes)
    0x19, 0x00,  //   Usage Minimum (0)
    0x29, 0x65,  //   Usage Maximum (101)
    0x81, 0x00,  //   Input (Data, Array)
    0xC0,        // End Collection
};

// NimBLE server callbacks — update connected/advertising flags
struct BlePresenterManager::ServerCb : public NimBLEServerCallbacks {
  BlePresenterManager& mgr;
  explicit ServerCb(BlePresenterManager& m) : mgr(m) {}

  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    mgr.connected   = true;
    mgr.advertising = false;
    LOG_DBG("PRESENTER", "Host connected");
  }

  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    mgr.connected = false;
    // Re-advertise only if not in deinit (server != nullptr signals active state)
    if (mgr.server != nullptr) {
      NimBLEDevice::startAdvertising();
      mgr.advertising = true;
      LOG_DBG("PRESENTER", "Host disconnected, re-advertising");
    }
  }
};

bool BlePresenterManager::init() {
  // Caller must ensure NimBLE is fully shut down before calling init()
  if (NimBLEDevice::isInitialized()) {
    LOG_ERR("PRESENTER", "NimBLE still initialized — aborting");
    return false;
  }

  NimBLEDevice::init("CrossPet_Presenter");
  NimBLEDevice::setSecurityAuth(false, false, false);
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);

  server = NimBLEDevice::createServer();
  if (!server) {
    LOG_ERR("PRESENTER", "Failed to create BLE server");
    NimBLEDevice::deinit(true);
    return false;
  }
  server->setCallbacks(new ServerCb(*this), true);

  hid = new (std::nothrow) NimBLEHIDDevice(server);
  if (!hid) {
    LOG_ERR("PRESENTER", "Failed to allocate HID device");
    NimBLEDevice::deinit(true);
    server = nullptr;
    return false;
  }
  hid->setManufacturer("CrossPoint");
  hid->setPnp(0x02, 0x05AC, 0x0256, 0x0110);
  hid->setHidInfo(0x00, 0x01);

  hid->setReportMap((uint8_t*)KEYBOARD_REPORT_MAP, sizeof(KEYBOARD_REPORT_MAP));
  inputReport = hid->getInputReport(1);

  hid->setBatteryLevel(100);
  hid->startServices();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(hid->getHidService()->getUUID());
  adv->setAppearance(HID_KEYBOARD);
  adv->enableScanResponse(true);

  NimBLEDevice::startAdvertising();
  advertising = true;

  LOG_DBG("PRESENTER", "BLE HID started OK");
  return true;
}

void BlePresenterManager::deinit() {
  if (advertising) {
    NimBLEDevice::stopAdvertising();
    advertising = false;
  }
  connected   = false;
  // Clear pointers before deinit — NimBLEDevice::deinit(true) frees all objects
  server      = nullptr;
  hid         = nullptr;
  inputReport = nullptr;
  if (NimBLEDevice::isInitialized()) {
    NimBLEDevice::deinit(true);
    vTaskDelay(pdMS_TO_TICKS(100));  // let BLE host task fully shut down
  }
  LOG_DBG("PRESENTER", "BLE HID peripheral stopped");
}

void BlePresenterManager::sendKey(uint8_t keycode) {
  if (!connected || !inputReport) return;

  // Key-down: [modifier=0, reserved=0, key, 0, 0, 0, 0, 0]
  uint8_t report[8] = {0, 0, keycode, 0, 0, 0, 0, 0};
  inputReport->notify(report, sizeof(report));

  // Key-up: all zeros
  memset(report, 0, sizeof(report));
  inputReport->notify(report, sizeof(report));
}
