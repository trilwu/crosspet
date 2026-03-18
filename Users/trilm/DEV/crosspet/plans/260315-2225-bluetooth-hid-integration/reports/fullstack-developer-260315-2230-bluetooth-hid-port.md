# Phase Implementation Report

### Executed Phase
- Phase: phase-01 (port BluetoothHIDManager + DeviceProfiles) + phase-06 (settings persistence)
- Plan: /Users/trilm/DEV/crosspet/plans/260315-2225-bluetooth-hid-integration/
- Status: completed

### Files Modified
- `src/CrossPointSettings.h` — +5 lines: added bleEnabled, bleBondedDeviceAddr[18], bleBondedDeviceName[32], bleBondedDeviceAddrType fields
- `src/JsonSettingsIO.cpp` — +23 lines: save + load for BLE fields in saveSettings/loadSettings

### Files Created
- `lib/hal/BluetoothHIDManager.h` — 106 lines (copied from fork, unchanged)
- `lib/hal/BluetoothHIDManager.cpp` — 122 lines: singleton, enable/disable, cleanup, saveState/loadState stubs
- `lib/hal/BluetoothHIDScan.cpp` — 133 lines: ScanCallbacks, startScan, stopScan, onScanResult
- `lib/hal/BluetoothHIDConnect.cpp` — 245 lines: ClientCallbacks, connectToDevice, disconnectFromDevice, isConnected, getConnectedDevices, findConnectedDevice, setters, setBondedDevice
- `lib/hal/BluetoothHIDInput.cpp` — 359 lines: extractGenericPageTurnKeycode, hasRecentActivity, updateActivity, checkAutoReconnect, onHIDNotify, parseHIDReport, mapKeycodeToButton
- `lib/hal/DeviceProfiles.h` — 100 lines (copied from fork, unchanged)
- `lib/hal/DeviceProfiles.cpp` — 211 lines (copied from fork, unchanged)

### Tasks Completed
- [x] Fetched all 4 source files from thedrunkpenguin/crosspet fork via gh api
- [x] Created BluetoothHIDManager.h (direct copy, 106 lines)
- [x] Modularized BluetoothHIDManager.cpp (849 lines) into 4 files: core, scan, connect, input
- [x] Created DeviceProfiles.h + DeviceProfiles.cpp (direct copies)
- [x] Verified btInUse() not in BluetoothHIDManager.cpp — no conflict with BlePresenterManager.cpp
- [x] Confirmed saveState/loadState are stubs — BUT BluetoothSettingsActivity already in repo uses SETTINGS fields
- [x] Added bleEnabled, bleBondedDeviceAddr, bleBondedDeviceName, bleBondedDeviceAddrType to CrossPointSettings.h
- [x] Added JSON read/write for all 4 BLE fields in JsonSettingsIO.cpp
- [x] Build: SUCCESS (pio run — no errors, no warnings from our files)

### Tests Status
- Compile: PASS (pio run: SUCCESS in 33s)
- Unit tests: N/A (embedded firmware, no unit test framework)

### Issues Encountered
1. BluetoothSettingsActivity.cpp + BluetoothSettingsDeviceList.cpp were already present in repo (untracked) referencing `SETTINGS.bleEnabled` etc. — required adding settings fields to unblock compile.
2. BluetoothHIDConnect.cpp is 245 lines (slightly over 200 limit) and BluetoothHIDInput.cpp is 359 lines. The input file excess is from onHIDNotify (130-line atomic callback) — cannot split cleanly without breaking NimBLE static callback contract.
3. git stash mid-session reverted CrossPointSettings.h and JsonSettingsIO.cpp — re-applied manually.

### Next Steps (unblocked)
- phase-02: BluetoothSettingsActivity already in repo, may need integration/UI fixes
- phase-03: Integrate BluetoothHIDManager into main.cpp loop (enable on startup if bleEnabled=1, call updateActivity/checkAutoReconnect)
- phase-04: Wire settings menu entries
- phase-05: WiFi/BLE mutual exclusion already implemented in BluetoothHIDManager::enable()

### Unresolved Questions
- BluetoothSettingsLearnMode.cpp (already in repo) — not inspected; may reference additional missing symbols
- bleBondedDeviceAddrType field: fork uses it as direct integer (0=public, 1=random), but NimBLEAddress in BluetoothHIDConnect.cpp currently hardcodes BLE_ADDR_PUBLIC — should connectToDevice use bleBondedDeviceAddrType when reconnecting?
