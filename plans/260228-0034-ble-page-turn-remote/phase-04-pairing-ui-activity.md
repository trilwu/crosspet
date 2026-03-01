# Phase 4: Pairing UI Activity

## Context

- [Phase 2](./phase-02-ble-hid-client.md) — BLE scan/connect logic in BleRemoteManager
- [Phase 3](./phase-03-settings-persistence.md) — Settings fields, SettingAction::BleRemote
- [WifiSelectionActivity](../../src/activities/network/WifiSelectionActivity.cpp) — pattern for scan → list → connect flow
- [ButtonRemapActivity](../../src/activities/settings/ButtonRemapActivity.cpp) — pattern for settings sub-activity

## Overview

- **Priority:** P2
- **Status:** Pending
- **Description:** Create BleRemotePairingActivity that shows scan results, lets user select a BLE HID remote, pairs/connects, and saves bonded device info to settings.

## Key Insights

- Follow WifiSelectionActivity as closest pattern: state machine (SCANNING → LIST → CONNECTING → CONNECTED/ERROR)
- Use existing GUI helpers: `drawHeader`, `drawList`, `drawHelpText`, `drawButtonHints`
- Activity launched via `startActivityForResult()` from SettingsActivity
- On successful pair: save address+name to settings, finish activity
- If already paired: show current device with option to forget/re-pair
- BleRemoteManager owns scan/connect logic — activity only drives UI

## Requirements

### Functional
- Show "Scanning..." with spinner while scanning for BLE HID devices
- Display list of discovered devices (name + RSSI indicator)
- User selects device → "Connecting..." → attempt connection
- On success: save bonded address/name to settings, show "Connected", auto-finish
- On failure: show error, allow retry
- If already paired: show current device name, "Forget" option
- Back button cancels and returns to settings

### Non-Functional
- Scan timeout: 10 seconds
- Connection timeout: 10 seconds
- List shows max 8 devices (screen space limit)

## Architecture

```
SettingsActivity
  └── startActivityForResult(BleRemotePairingActivity)
        │
        ├── State: IDLE
        │     └── Check if already paired → show status
        │
        ├── State: SCANNING
        │     └── bleManager.startScan(10)
        │     └── Poll discoveredDevices for UI updates
        │
        ├── State: SCAN_COMPLETE
        │     └── Show device list
        │     └── User navigates + selects
        │
        ├── State: CONNECTING
        │     └── bleManager.connectToDevice(addr)
        │     └── Show "Connecting..."
        │
        ├── State: CONNECTED
        │     └── Save to settings, show success, auto-finish
        │
        └── State: ERROR
              └── Show error message, allow retry
```

## Related Code Files

### Create
- `src/activities/settings/BleRemotePairingActivity.h`
- `src/activities/settings/BleRemotePairingActivity.cpp`

### Modify
- `src/activities/settings/SettingsActivity.cpp` — add `#include` and case handler (from Phase 3)

### Reference (read-only)
- `src/activities/network/WifiSelectionActivity.h/.cpp` — scan/list/connect pattern
- `src/activities/Activity.h` — base class interface
- `src/components/UITheme.h` — GUI drawing helpers

## Implementation Steps

1. **Define activity states**

   ```cpp
   enum class BleUiState {
     IDLE,           // Show paired device or prompt to scan
     SCANNING,       // Actively scanning
     SCAN_COMPLETE,  // Show results list
     CONNECTING,     // Connecting to selected device
     CONNECTED,      // Success — auto-finish after brief display
     ERROR           // Connection failed
   };
   ```

2. **Create BleRemotePairingActivity header**

   ```cpp
   class BleRemotePairingActivity final : public Activity {
     ButtonNavigator buttonNavigator;
     BleUiState state = BleUiState::IDLE;
     int selectedDeviceIndex = 0;
     std::string errorMessage;
     unsigned long stateEnteredAt = 0;

   public:
     explicit BleRemotePairingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
         : Activity("BLE Pairing", renderer, mappedInput) {}
     void onEnter() override;
     void onExit() override;
     void loop() override;
     void render(RenderLock&&) override;

   private:
     void startScanning();
     void connectToSelected();
     void forgetDevice();
   };
   ```

3. **Implement onEnter()**

   ```cpp
   void BleRemotePairingActivity::onEnter() {
     Activity::onEnter();
     selectedDeviceIndex = 0;
     errorMessage.clear();

     // If already paired, show IDLE with device info
     if (strlen(SETTINGS.bleBondedDeviceAddr) > 0) {
       state = BleUiState::IDLE;
     } else {
       startScanning();
     }
     requestUpdate();
   }
   ```

4. **Implement loop() state machine**

   - **IDLE**: Back exits. Confirm starts scan or forgets device (depending on selection).
   - **SCANNING**: Poll `bleManager.isScanning()`. When done → SCAN_COMPLETE. Back cancels scan.
   - **SCAN_COMPLETE**: Navigate list with Left/Right. Confirm connects. Back re-scans or exits.
   - **CONNECTING**: Poll `bleManager.isConnected()`. Timeout → ERROR. Success → CONNECTED.
   - **CONNECTED**: Brief display (1.5s), save settings, finish().
   - **ERROR**: Confirm retries. Back exits.

5. **Implement render()**

   Use existing GUI helpers matching other settings activities:
   - Header: "Bluetooth Remote"
   - IDLE: show paired device name or "No device paired". List: [Scan for devices] [Forget device]
   - SCANNING: centered "Scanning..." text
   - SCAN_COMPLETE: `drawList()` with device names + RSSI
   - CONNECTING: centered "Connecting..." text
   - CONNECTED: centered "Connected to [name]" text
   - ERROR: centered error message + "Press OK to retry"

6. **Implement connectToSelected()**

   ```cpp
   void BleRemotePairingActivity::connectToSelected() {
     auto& devices = bleManager.getDiscoveredDevices();
     if (selectedDeviceIndex >= devices.size()) return;

     state = BleUiState::CONNECTING;
     stateEnteredAt = millis();
     requestUpdate();

     const auto& device = devices[selectedDeviceIndex];
     if (bleManager.connectToDevice(device.address)) {
       // Save to settings
       strncpy(SETTINGS.bleBondedDeviceAddr, device.address.toString().c_str(),
               sizeof(SETTINGS.bleBondedDeviceAddr) - 1);
       strncpy(SETTINGS.bleBondedDeviceName, device.name.c_str(),
               sizeof(SETTINGS.bleBondedDeviceName) - 1);
       SETTINGS.saveToFile();

       state = BleUiState::CONNECTED;
       stateEnteredAt = millis();
     } else {
       state = BleUiState::ERROR;
       errorMessage = "Connection failed";
     }
     requestUpdate();
   }
   ```

7. **Include in SettingsActivity.cpp**

   ```cpp
   #include "BleRemotePairingActivity.h"
   ```

8. **Compile and verify**

## Todo List

- [ ] Create BleRemotePairingActivity.h with state enum and class declaration
- [ ] Create BleRemotePairingActivity.cpp with state machine loop
- [ ] Implement IDLE state (show paired device or prompt)
- [ ] Implement SCANNING state (drive BleRemoteManager scan, show progress)
- [ ] Implement SCAN_COMPLETE state (device list with navigation)
- [ ] Implement CONNECTING state (connect attempt, timeout handling)
- [ ] Implement CONNECTED state (save settings, auto-finish)
- [ ] Implement ERROR state (show message, retry option)
- [ ] Implement render() for all states using existing GUI helpers
- [ ] Implement forgetDevice() (clear settings fields, save)
- [ ] Wire up include + case in SettingsActivity.cpp
- [ ] Compile and verify

## Success Criteria

- Launching "Pair Remote" from Settings shows BLE pairing screen
- Scanning discovers nearby BLE HID devices
- Selecting a device initiates connection
- Successful pairing saves device to settings and returns to Settings
- "Forget Device" clears pairing and allows re-scan
- Back button exits cleanly at every state

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Blocking connect call freezes UI | Connect runs in NimBLE task. Use polling in loop() with timeout. |
| Too many scan results for screen | Limit to 8 entries. Sort by RSSI (strongest first). |
| Remote requires PIN pairing | Most page-turn remotes use Just Works. If PIN needed, show error. |
| E-ink refresh during scan/connect | Scan is background NimBLE task. UI updates only on state change. |

## Next Steps

→ Phase 5: Auto-reconnect on wake + WiFi mutual exclusion
