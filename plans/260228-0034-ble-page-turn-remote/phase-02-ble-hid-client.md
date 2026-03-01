# Phase 2: BLE HID Client & Report Parsing

## Context

- [Phase 1](./phase-01-nimble-halgpio-integration.md) — NimBLE integration, HalGPIO injection
- [BLE Research](../reports/researcher-260225-esp32c3-ble-capabilities.md) — HID support details

## Overview

- **Priority:** P1
- **Status:** Pending
- **Description:** Implement NimBLE Central client that scans for BLE HID devices, connects, subscribes to HID Report characteristic, parses keyboard reports, and maps HID keycodes to HalGPIO button bits.

## Key Insights

- BLE HID Service UUID: `0x1812`
- HID Report characteristic UUID: `0x2A4D` (Input Report)
- Standard keyboard HID report = 8 bytes: `[modifier, reserved, key1, key2, key3, key4, key5, key6]`
- Page-turn remotes typically send arrow keys, Page Up/Down, Enter, Volume keys
- NimBLE central mode: `NimBLEClient` connects, `NimBLERemoteService` discovers services, `NimBLERemoteCharacteristic` subscribes to notifications
- Consumer Control reports (volume keys) use different report ID — need to handle both keyboard and consumer HID reports

## Requirements

### Functional
- Scan for nearby BLE devices advertising HID service
- Connect to selected device and discover HID service
- Subscribe to HID Report Input notifications
- Parse keyboard HID reports → extract pressed keycodes
- Map keycodes to HalGPIO button indices (BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_BACK, BTN_CONFIRM)
- Release button state when key is released (report with zero keys)

### Non-Functional
- Connection timeout: 10 seconds
- Scan duration: configurable (default 10s)
- Handle disconnection gracefully (clear bleState)
- Thread-safe: NimBLE callbacks run on NimBLE task, bleState read on main loop

## Architecture

```
NimBLE Stack (FreeRTOS task)
  │
  ├── NimBLEScan → onResult() callback
  │     └── Filter by HID service UUID (0x1812)
  │
  ├── NimBLEClient → onConnect() / onDisconnect()
  │     └── Discover HID service → subscribe to Report characteristic
  │
  └── Characteristic notification callback
        └── parseHidReport(data, len)
              └── mapKeycodeToButton(keycode)
                    └── gpio.setBleButtonState(bitmask)
```

## Related Code Files

### Modify
- `src/ble/BleRemoteManager.h` — flesh out full interface
- `src/ble/BleRemoteManager.cpp` — implement scan, connect, HID parsing

### Reference (read-only)
- `lib/hal/HalGPIO.h` — button index constants (BTN_LEFT=2, BTN_RIGHT=3, etc.)
- `open-x4-sdk/libs/hardware/InputManager/include/InputManager.h` — button definitions

## Implementation Steps

1. **Define HID keycode → button mapping table**

   ```cpp
   // Common page-turn remote keycodes (USB HID Usage Tables)
   namespace BleHidKeys {
     // Keyboard page (0x07)
     constexpr uint8_t KEY_ENTER      = 0x28;
     constexpr uint8_t KEY_BACKSPACE   = 0x2A;
     constexpr uint8_t KEY_SPACE       = 0x2C;
     constexpr uint8_t KEY_RIGHT_ARROW = 0x4F;
     constexpr uint8_t KEY_LEFT_ARROW  = 0x50;
     constexpr uint8_t KEY_DOWN_ARROW  = 0x51;
     constexpr uint8_t KEY_UP_ARROW    = 0x52;
     constexpr uint8_t KEY_PAGE_DOWN   = 0x4E;
     constexpr uint8_t KEY_PAGE_UP     = 0x4B;

     // Consumer Control page (0x0C) — used by camera shutter remotes
     constexpr uint16_t CC_VOLUME_UP   = 0x00E9;
     constexpr uint16_t CC_VOLUME_DOWN = 0x00EA;
   }
   ```

2. **Default key mapping**

   ```cpp
   struct BleKeyMapping {
     uint8_t hidKeycode;
     uint8_t gpioButton;  // HalGPIO::BTN_* index
   };

   // Default: arrow keys + page keys → navigation
   static const BleKeyMapping defaultKeyMap[] = {
     {BleHidKeys::KEY_RIGHT_ARROW, HalGPIO::BTN_RIGHT},
     {BleHidKeys::KEY_LEFT_ARROW,  HalGPIO::BTN_LEFT},
     {BleHidKeys::KEY_DOWN_ARROW,  HalGPIO::BTN_DOWN},
     {BleHidKeys::KEY_UP_ARROW,    HalGPIO::BTN_UP},
     {BleHidKeys::KEY_PAGE_DOWN,   HalGPIO::BTN_DOWN},
     {BleHidKeys::KEY_PAGE_UP,     HalGPIO::BTN_UP},
     {BleHidKeys::KEY_ENTER,       HalGPIO::BTN_CONFIRM},
     {BleHidKeys::KEY_BACKSPACE,   HalGPIO::BTN_BACK},
     {BleHidKeys::KEY_SPACE,       HalGPIO::BTN_RIGHT},
   };
   ```

3. **Implement NimBLE scan with HID filter**

   ```cpp
   void BleRemoteManager::startScan(uint32_t durationSec) {
     NimBLEScan* scan = NimBLEDevice::getScan();
     scan->setScanCallbacks(this);  // BleRemoteManager implements NimBLEScanCallbacks
     scan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);
     scan->setActiveScan(true);
     scan->start(durationSec, false);  // non-blocking
   }
   ```

4. **Implement scan result callback — filter HID devices**

   ```cpp
   void BleRemoteManager::onResult(const NimBLEAdvertisedDevice* device) {
     if (device->isAdvertisingService(NimBLEUUID((uint16_t)0x1812))) {
       // Store discovered device info for UI
       DiscoveredDevice d;
       d.name = device->getName();
       d.address = device->getAddress();
       d.rssi = device->getRSSI();
       discoveredDevices.push_back(d);
     }
   }
   ```

5. **Implement connect + HID service discovery**

   ```cpp
   bool BleRemoteManager::connectToDevice(const NimBLEAddress& addr) {
     NimBLEClient* client = NimBLEDevice::createClient();
     client->setClientCallbacks(this);  // onConnect/onDisconnect

     if (!client->connect(addr)) return false;

     NimBLERemoteService* hidService = client->getService(NimBLEUUID((uint16_t)0x1812));
     if (!hidService) { client->disconnect(); return false; }

     // Subscribe to all Input Report characteristics
     auto chars = hidService->getCharacteristics(true);
     for (auto* ch : *chars) {
       if (ch->getUUID() == NimBLEUUID((uint16_t)0x2A4D) && ch->canNotify()) {
         ch->subscribe(true, [this](NimBLERemoteCharacteristic* c,
                                     uint8_t* data, size_t len, bool isNotify) {
           onHidReport(data, len);
         });
       }
     }
     connected = true;
     return true;
   }
   ```

6. **Implement HID report parser**

   ```cpp
   void BleRemoteManager::onHidReport(const uint8_t* data, size_t len) {
     uint8_t buttonState = 0;

     if (len >= 3) {
       // Standard keyboard report: [modifier, reserved, key1..key6]
       // Check keys starting at byte 2
       for (size_t i = 2; i < len && i < 8; i++) {
         uint8_t keycode = data[i];
         if (keycode == 0) continue;  // no key
         for (const auto& mapping : defaultKeyMap) {
           if (mapping.hidKeycode == keycode) {
             buttonState |= (1 << mapping.gpioButton);
           }
         }
       }
     }

     gpio.setBleButtonState(buttonState);
   }
   ```

7. **Handle disconnection — clear BLE state**

   ```cpp
   void BleRemoteManager::onDisconnect(NimBLEClient* client, int reason) {
     connected = false;
     gpio.setBleButtonState(0);  // Clear all BLE buttons
     LOG_DBG("BLE", "Disconnected, reason: %d", reason);
   }
   ```

8. **Verify with test** — manually call `gpio.setBleButtonState(1 << HalGPIO::BTN_RIGHT)` and confirm page turn works in EPUB reader.

## Todo List

- [ ] Define HID keycode constants (keyboard + consumer control)
- [ ] Define default keycode → button mapping table
- [ ] Implement `BleRemoteManager` as `NimBLEScanCallbacks` + `NimBLEClientCallbacks`
- [ ] Implement scan with HID service UUID filter
- [ ] Store discovered devices list for UI (Phase 4)
- [ ] Implement `connectToDevice()` — connect, discover HID service, subscribe to reports
- [ ] Implement `onHidReport()` — parse keyboard reports, map to button bitmask
- [ ] Implement `onDisconnect()` — clear bleState
- [ ] Handle Consumer Control reports (volume keys from camera shutter remotes)
- [ ] Add thread safety for `discoveredDevices` vector (NimBLE callback thread vs main)
- [ ] Compile and verify no errors

## Success Criteria

- Can scan and discover BLE HID devices
- Can connect to a BLE HID keyboard/remote
- HID key presses appear as button events in HalGPIO
- Page turning works in EPUB reader via BLE remote
- Disconnection clears all BLE button state cleanly

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| HID report format varies across remotes | Support standard 8-byte keyboard report. Test with 2-3 remotes. |
| Consumer Control reports have different format | Detect by report length or report ID. Handle as separate case. |
| NimBLE callback thread race conditions | Use `volatile` for bleState. Discovered devices use mutex. |
| Some remotes need pairing/bonding before reports work | NimBLE handles Just Works pairing. Enable bonding via `NimBLEDevice::setSecurityAuth()`. |

## Next Steps

→ Phase 3: Settings persistence (bleEnabled, bonded address, key mapping)
→ Phase 4: Pairing UI activity (scan results list, connect button)
