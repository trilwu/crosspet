# Code Review: BLE Page-Turn Remote Support

## Scope
- **New files (4):** `BleRemoteManager.h/cpp`, `BleRemotePairingActivity.h/cpp`
- **Modified files (11):** `platformio.ini`, `HalGPIO.h/cpp`, `CrossPointSettings.h`, `JsonSettingsIO.cpp`, `SettingsList.h`, `SettingsActivity.h/cpp`, `main.cpp`, `WifiSelectionActivity.cpp`, `english.yaml`
- **LOC:** ~867 new (BLE core + pairing UI), ~80 modified
- **Focus:** Thread safety, edge detection, memory safety, BLE lifecycle, WiFi mutual exclusion, UI state machine

## Overall Assessment

Well-structured feature. Clean separation between BLE manager, GPIO integration, and pairing UI. The architecture follows existing patterns (Activity lifecycle, settings serialization, extern globals). Key concerns are around thread safety of BLE state flags and a few edge cases in lifecycle management.

---

## Critical Issues

### 1. Thread Safety: `connected`, `scanning`, `enabled` flags lack synchronization
**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.h` lines 55-58
**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp`

The `bleState` in HalGPIO is correctly marked `volatile`, but `connected`, `scanning`, and `enabled` in BleRemoteManager are plain `bool`. These are written from NimBLE callbacks running on the NimBLE FreeRTOS task (`onConnect`, `onDisconnect`, `onScanEnd`) and read from the main loop (`tick()`, `isConnected()`, `isScanning()`).

On ESP32-C3 (single-core RISC-V), this is **less dangerous** than on dual-core ESP32 because there is no true parallelism -- but NimBLE callbacks still run from a different FreeRTOS task context. Without `volatile` or atomics, the compiler may cache these values in registers across loop iterations, causing stale reads.

**Impact:** Possible missed connection/disconnection events; main loop could keep calling `tick()` after connection established, or miss that scanning ended.

**Recommendation:** Mark `connected` and `scanning` as `volatile bool`, or use `std::atomic<bool>`. `enabled` and `suspended` are only set from main loop context so they are safe.

```cpp
volatile bool connected = false;
volatile bool scanning = false;
```

### 2. Race condition in `connectToDevice`: `connected` set in two places
**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` lines 136, 221

`connected = true` is set at line 136 (end of `connectToDevice`, main task) AND at line 221 (`onConnect` callback, NimBLE task). If `onConnect` fires before line 136 and a disconnect happens between the callback and line 136, the main loop would incorrectly think we are still connected.

**Impact:** Low probability on single-core, but architecturally unsound.

**Recommendation:** Remove `connected = true` from `connectToDevice()` (line 136). The `onConnect` callback already handles it. The return value of `connectToDevice` should rely on whether subscription succeeded rather than the connection flag.

---

## High Priority

### 3. `suspend()` sets `suspended` then calls `deinit()` which clears `enabled` -- `resume()` checks `suspended` but not `enabled`
**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` lines 167-182

```cpp
void BleRemoteManager::suspend() {
  if (!enabled) return;   // guard checks enabled
  suspended = true;
  deinit();               // deinit() sets enabled = false
}

void BleRemoteManager::resume() {
  if (!suspended) return; // only checks suspended
  suspended = false;
  init();                 // re-initializes
}
```

This works correctly as-is because `resume()` checks `suspended` not `enabled`. However, after `resume()`, auto-reconnect is NOT re-queued. The caller (`WifiSelectionActivity::onExit()`) handles this:

```cpp
bleManager.resume();
if (SETTINGS.bleEnabled && strlen(SETTINGS.bleBondedDeviceAddr) > 0) {
  bleManager.autoReconnect(SETTINGS.bleBondedDeviceAddr);
}
```

**Issue:** If BLE was disabled (`SETTINGS.bleEnabled == 0`) when WiFi started but user changed the setting during WiFi activity, `resume()` + `autoReconnect()` would still fire because the check is against the current setting value. This is actually correct behavior for the forward case. No actual bug here, but worth noting as a non-obvious design decision.

### 4. `reconnectAddress` uninitialized default
**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.h` line 60

`NimBLEAddress reconnectAddress;` has no explicit initialization. NimBLE's default constructor initializes to 0, so this is safe. However, `tick()` will attempt `connectToDevice(reconnectAddress)` when `reconnectPending` is true. If `autoReconnect()` is called with an invalid address string, `NimBLEAddress(std::string(bondedAddr), 0)` may produce an unusable address.

**Impact:** `connectToDevice` would simply fail and return false. No crash, but log noise.

**Recommendation:** Add validation in `autoReconnect()`:
```cpp
if (!enabled || strlen(bondedAddr) < 17) return;  // MAC addr is "XX:XX:XX:XX:XX:XX" = 17 chars
```

### 5. `BleRemotePairingActivity.cpp` exceeds 200-line guideline (358 lines)
**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp`

Per project rules, code files should stay under 200 lines. The render methods (lines 200-358) could be extracted to a separate file.

**Recommendation:** Extract all `render*()` methods into `BleRemotePairingActivityRender.cpp`, keeping loop/lifecycle in the main file.

---

## Medium Priority

### 6. Edge detection: `isPressed()` reads `bleState` directly without snapshot
**File:** `/Users/trilm/DEV/crosspoint-reader/lib/hal/HalGPIO.cpp` line 21

```cpp
bool HalGPIO::isPressed(uint8_t buttonIndex) const {
  return inputMgr.isPressed(buttonIndex) || (bleState & (1 << buttonIndex));
}
```

`isPressed()` reads `bleState` (volatile) directly, while `wasPressed()` uses the snapshotted `bleWasPressed`. If `isPressed()` is called multiple times in the same loop iteration, `bleState` could change between calls (BLE callback fires between reads).

**Impact:** Inconsistent reads within a single frame. Unlikely to cause visible bugs since `isPressed()` is used less frequently than `wasPressed()` for page turns, but power-button hold detection in `main.cpp` line 377 uses `isPressed(BTN_POWER)` which only checks physical buttons. BLE cannot inject power button presses (BTN_POWER = 6, bitmask bit 6), so no real issue.

**Recommendation:** Consider snapshotting `bleState` once in `update()` and using that snapshot for `isPressed()` too. Low urgency.

### 7. `connectToDevice` is synchronous/blocking -- called from UI loop
**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp` line 67

`connectToDevice()` blocks the main loop during connection (up to 10-second timeout per `setConnectTimeout(10)`). During this time, no screen updates, no button handling, no auto-sleep timer reset.

**Impact:** E-ink display won't update during connection attempt (already showing "Connecting..." so this is acceptable). But auto-sleep timer is not reset, and if sleep timeout is 1 minute, the device could enter deep sleep mid-connection.

**Recommendation:** Reset auto-sleep timer before calling `connectToDevice()`, or mark the pairing activity as `preventAutoSleep`. This matches how `WifiSelectionActivity` handles blocking operations. Also, `tick()` in main loop calls `connectToDevice()` for auto-reconnect which blocks too.

### 8. No bounds check on `buttonIndex` in BLE bitmask operations
**File:** `/Users/trilm/DEV/crosspoint-reader/lib/hal/HalGPIO.cpp` lines 21-31

```cpp
(bleState & (1 << buttonIndex))
```

If `buttonIndex >= 8`, this shifts beyond `uint8_t` range. Button constants go up to `BTN_POWER = 6`, so bit 6 is the max. `1 << 6 = 64` fits in uint8_t. But `BTN_POWER` is index 6 and BLE remotes cannot inject it (no HID mapping for it), so a BLE-injected power press is impossible.

**Impact:** No bug with current button definitions. If buttons are ever added beyond index 7, this would overflow.

### 9. `onHidReport` ignores Consumer Control reports
**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` lines 233-249

The HID report parser only handles standard keyboard reports (Usage Page 0x07). Many BLE page-turn remotes (e.g., Bluetooth presentation clickers) send Consumer Control reports (Usage Page 0x0C) with different keycodes for Page Up/Down. These would be silently ignored.

**Impact:** Some popular BLE remotes won't work. This is a feature gap, not a bug.

**Recommendation:** Document in user-facing docs which remotes are supported, or add Consumer Control report parsing in a follow-up.

---

## Low Priority

### 10. `getSignalStrengthIndicator` duplicated between WiFi and BLE activities
**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp` line 201
**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/network/WifiSelectionActivity.cpp` line 463

Identical logic. Could be extracted to a shared utility.

### 11. `discoveredDevices.size()` accessed without lock in `onScanEnd`
**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 217

```cpp
LOG_DBG("BLE", "Scan ended, found %zu HID devices", discoveredDevices.size());
```

This reads the vector size without holding `devicesMutex`. Since NimBLE callbacks run on the NimBLE task and `onScanEnd` fires after all `onResult` callbacks, this is practically safe. But it is technically a data race.

### 12. No `#if CROSSPOINT_EMULATED == 0` guard on BLE code
**File:** `/Users/trilm/DEV/crosspoint-reader/lib/hal/HalGPIO.h` line 22

The `inputMgr` field is guarded by `#if CROSSPOINT_EMULATED == 0`, but the BLE state fields (`bleState`, `prevBleState`, etc.) and `setBleButtonState()` are NOT guarded. This is correct -- BLE state fields are plain data and don't depend on hardware. The compile guard does NOT affect BLE state handling. `HalGPIO.cpp` implementations reference `inputMgr` without a guard, which means the entire `.cpp` file is hardware-only. Since BLE is also hardware-only (ESP32), this is consistent.

---

## Positive Observations

1. **Clean mutex usage** for `discoveredDevices` -- proper `lock_guard` in all access points (`onResult`, `getDiscoveredDevices`, `clearDiscoveredDevices`).

2. **Correct edge detection logic** in `HalGPIO::update()` -- snapshot-then-diff pattern (`currentBle & ~prevBleState`) ensures single-frame press events.

3. **strncpy with null-terminator** consistently applied in settings serialization (`JsonSettingsIO.cpp` lines 194-200, `BleRemotePairingActivity.cpp` lines 69-73).

4. **NimBLE build flags** well-configured -- central-only mode, single connection, observer role. Saves RAM on the constrained ESP32-C3.

5. **WiFi mutual exclusion** correctly implemented -- suspend before WiFi, resume + auto-reconnect after.

6. **UI state machine** covers all states with proper exit paths. Every state handles Back button. No stuck states possible.

7. **Settings persistence** follows existing patterns exactly -- same save/load/clamp approach as other settings.

8. **BLE deinit before deep sleep** in `enterDeepSleep()` (main.cpp line 190) prevents resource leaks.

---

## Recommended Actions (Priority Order)

1. **Mark `connected` and `scanning` as volatile** in `BleRemoteManager.h` -- simple 1-line fix, prevents potential stale reads
2. **Remove duplicate `connected = true`** from `connectToDevice()` line 136 -- let `onConnect` callback be the sole writer
3. **Add MAC address length validation** in `autoReconnect()` -- prevents attempting connection to garbage address
4. **Add auto-sleep prevention** during BLE connection attempts, especially the `tick()` auto-reconnect path
5. **Extract render methods** from `BleRemotePairingActivity.cpp` to comply with 200-line guideline
6. **Document supported remote types** (standard keyboard HID only, not Consumer Control)

## Metrics
- Type Coverage: N/A (C++, no TypeScript)
- Test Coverage: Manual testing only (no unit test framework for hardware code)
- Linting Issues: 0 (clean compile per reports)
- Flash Usage: 93.8% (tight, monitor with future additions)
- RAM Usage: 31.2% (acceptable)

## Unresolved Questions

1. Should `tick()` (auto-reconnect) be rate-limited? Currently it attempts reconnect every loop iteration until it succeeds or `reconnectPending` is cleared. On failure, it clears the flag and never retries. Is one-shot retry the intended behavior?
2. Does the NimBLE stack handle bonding persistence across deep sleep cycles, or does the bonding table need explicit save/restore via `NimBLEDevice::setSecurityAuth` bond storage?
3. Is there a path where `bleManager.suspend()` is called when BLE is already disabled (bleEnabled=0)? The `if (!enabled) return` guard would skip it, but `resume()` would also be skipped since `suspended` was never set to true. This is correct behavior but should be documented.
