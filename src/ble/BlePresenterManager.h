#pragma once
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

// BLE HID peripheral — presents device to a host as a keyboard for slide control.
class BlePresenterManager {
 public:
  bool init();
  void deinit();

  bool isConnected()   const { return connected; }
  bool isAdvertising() const { return advertising; }

  // Send a single key press+release (8-byte keyboard HID report, report ID 1).
  void sendKey(uint8_t keycode);

 private:
  bool connected   = false;
  bool advertising = false;

  NimBLEServer*         server      = nullptr;
  NimBLEHIDDevice*      hid         = nullptr;
  NimBLECharacteristic* inputReport = nullptr;

  struct ServerCb;  // defined in .cpp
};
