# Phase 1: NimBLE Integration & HalGPIO Injection

## Context

- [Brainstorm](../reports/brainstorm-260228-0034-ble-page-turn-remote.md)
- [BLE Research](../reports/researcher-260225-esp32c3-ble-capabilities.md)

## Overview

- **Priority:** P1 (foundation for all other phases)
- **Status:** Pending
- **Description:** Add NimBLE-Arduino dependency, create BleRemoteManager skeleton, modify HalGPIO to accept injected BLE button state.

## Key Insights

- NimBLE uses ~40-60KB RAM (central mode). Bluedroid uses 60-100KB. NimBLE is mandatory.
- HalGPIO.update() reads ADC pins → produces `currentState` bitmask. BLE injection = OR additional bits into this bitmask.
- `HalGPIO` lives in `lib/hal/` (part of firmware, not open-x4-sdk). Safe to modify in personal fork.
- ESP32-C3 config: must disable Bluedroid and enable NimBLE in sdkconfig/menuconfig.

## Requirements

### Functional
- NimBLE-Arduino compiles and links without errors
- HalGPIO accepts external button state injection
- Injected state merges with physical ADC state via bitwise OR
- No behavior change when BLE is not active (zero bits injected)

### Non-Functional
- Zero RAM overhead when BLE disabled (lazy init)
- No new dependencies besides NimBLE-Arduino

## Architecture

```
BleRemoteManager (new)
  │
  ├── init() / deinit()
  ├── isConnected()
  └── getButtonState() → uint8_t bitmask
        │
        ▼
HalGPIO::update()
  ├── Read ADC pins → physicalState
  ├── Read bleState from BleRemoteManager
  └── currentState = physicalState | bleState
```

## Related Code Files

### Modify
- `lib/hal/HalGPIO.h` — add `setBleButtonState(uint8_t)` method, add `bleState` member
- `lib/hal/HalGPIO.cpp` — merge bleState in `getState()`
- `platformio.ini` — add NimBLE-Arduino lib_dep

### Create
- `src/ble/BleRemoteManager.h` — BLE manager interface (skeleton)
- `src/ble/BleRemoteManager.cpp` — BLE manager implementation (skeleton)

## Implementation Steps

1. **Add NimBLE-Arduino to platformio.ini**
   ```ini
   lib_deps =
     ...existing...
     h2zero/NimBLE-Arduino@^2.1.0
   ```

2. **Add ESP32 BLE config flags to build_flags**
   ```ini
   build_flags =
     ...existing...
     -DCONFIG_BT_NIMBLE_ROLE_CENTRAL=1
     -DCONFIG_BT_NIMBLE_ROLE_PERIPHERAL=0
     -DCONFIG_BT_NIMBLE_ROLE_BROADCASTER=0
     -DCONFIG_BT_NIMBLE_ROLE_OBSERVER=1
     -DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
   ```
   Disabling peripheral/broadcaster saves RAM. Observer needed for scanning.

3. **Modify HalGPIO to accept BLE state**

   In `HalGPIO.h`:
   ```cpp
   private:
     volatile uint8_t bleState = 0;  // BLE-injected button bits
   public:
     void setBleButtonState(uint8_t state) { bleState = state; }
   ```

   In `HalGPIO.cpp` `getState()`:
   ```cpp
   uint8_t HalGPIO::getState() {
     uint8_t state = 0;
     // ...existing ADC reads...
     // Merge BLE-injected state
     state |= bleState;
     return state;
   }
   ```

4. **Create BleRemoteManager skeleton**

   `src/ble/BleRemoteManager.h`:
   ```cpp
   #pragma once
   #include <cstdint>

   class HalGPIO;

   class BleRemoteManager {
   public:
     explicit BleRemoteManager(HalGPIO& gpio);

     bool init();      // Initialize NimBLE stack
     void deinit();    // Shutdown NimBLE stack
     bool isEnabled() const;
     bool isConnected() const;

     // Scanning
     void startScan(uint32_t durationSec = 5);
     void stopScan();
     bool isScanning() const;

     // Connection (to be implemented in Phase 2)
     // void connectToDevice(const NimBLEAddress& addr);
     // void disconnect();

   private:
     HalGPIO& gpio;
     bool enabled = false;
     bool connected = false;
   };
   ```

5. **Verify compilation**
   ```sh
   pio run --environment default
   ```

## Todo List

- [ ] Add NimBLE-Arduino to platformio.ini lib_deps
- [ ] Add NimBLE config build flags (central-only, 1 connection)
- [ ] Add `bleState` member and `setBleButtonState()` to HalGPIO
- [ ] Merge `bleState` into `getState()` via bitwise OR
- [ ] Create `src/ble/BleRemoteManager.h` skeleton
- [ ] Create `src/ble/BleRemoteManager.cpp` skeleton with init/deinit
- [ ] Verify full project compiles with `pio run`

## Success Criteria

- `pio run` compiles without errors
- NimBLE-Arduino linked in build output
- HalGPIO merges physical + BLE state (verifiable by setting bleState manually)
- No regression in existing button behavior (bleState=0 by default)

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| NimBLE version incompatibility | Pin to known-good version (^2.1.0) |
| Flash size increase too large | NimBLE adds ~280KB. 16MB flash = fine. |
| Build flag conflicts with existing ESP-IDF config | Test compilation early. Adjust sdkconfig if needed. |

## Next Steps

→ Phase 2: Implement BLE HID client, report parsing, connection lifecycle
