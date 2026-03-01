# Code Review: BLE Page-Turn Remote Implementation

**Date:** 2026-03-01
**Reviewer:** code-reviewer agent
**Scope:** Full BLE HID remote feature -- new files + all modifications
**LOC reviewed:** ~900 (new) + ~150 (modified)

---

## Overall Assessment: SHIP-READY with minor fixes

The BLE remote implementation is well-architected, clean, and follows existing codebase patterns closely. The WiFi/BLE mutual exclusion is thorough and consistent across all 6 WiFi-using activities. The HalGPIO injection approach is elegant -- it makes BLE buttons indistinguishable from physical buttons to all downstream code. The code is production-quality with a handful of issues that should be addressed.

---

## Summary of Findings

| Severity | Count | Description |
|----------|-------|-------------|
| Critical | 1 | Double suspend in nested WiFi activities |
| Warning  | 5 | State management, address type, reconnect robustness, HID parsing, file size |
| Info     | 4 | Style, minor improvements |

---

## Critical Issues

### 1. Double suspend() when WifiSelectionActivity is launched from activities that already suspend BLE

**Files:**
- `/Users/trilm/DEV/crosspoint-reader/src/activities/network/WifiSelectionActivity.cpp` line 24
- `/Users/trilm/DEV/crosspoint-reader/src/activities/network/CrossPointWebServerActivity.cpp` line 42

**Problem:** `WifiSelectionActivity::onEnter()` calls `bleManager.suspend()` (line 24). But `CrossPointWebServerActivity::onEnter()` also calls `bleManager.suspend()` (line 42) before launching `WifiSelectionActivity` as a sub-activity. This means `suspend()` is called twice.

`suspend()` calls `deinit()`, which sets `enabled = false`. The second `suspend()` hits the `if (!enabled) return;` guard and does nothing harmful -- but critically, it sets `suspended = true` again (line 172), which is already true. When `CrossPointWebServerActivity::onExit()` calls `resume()` at line 106, the first `resume()` clears `suspended` and re-inits BLE. This works correctly because `WifiSelectionActivity` does NOT call `resume()` in its `onExit()` (there's even a comment about this at line 83-84).

However, the **standalone Network action** in `SettingsActivity` (line 183-193) launches `WifiSelectionActivity` directly without `CrossPointWebServerActivity` as parent. Here `WifiSelectionActivity.onEnter()` suspends BLE, but the result handler at line 190 calls `bleManager.resume()`. This is CORRECT for this path.

**Actual critical path:** `OtaUpdateActivity`, `KOReaderAuthActivity`, `OpdsBookBrowserActivity`, and `KOReaderSyncActivity` all call `bleManager.suspend()` in their `onEnter()` AND they launch `WifiSelectionActivity` as a sub-activity which calls `suspend()` again. The double-call is safe due to the guard in `suspend()`:
```cpp
void BleRemoteManager::suspend() {
  if (!enabled) return;  // Already deinited from first suspend
  ...
}
```

**Verdict:** After thorough trace, the double suspend is safe because:
1. Second `suspend()` returns early since `enabled` is already false
2. But `suspended` was already set to `true` on the first call, so the flag is correct
3. The resume in the parent's `onExit()` correctly re-enables BLE

**Wait -- BUG FOUND:** Looking more carefully at `suspend()`:
```cpp
void BleRemoteManager::suspend() {
  if (!enabled) return;  // <-- returns early on second call
  suspended = true;      // <-- never reached on second call (fine)
  deinit();
  ...
}
```

But in `WifiSelectionActivity::onEnter()` (line 24), when called from `CrossPointWebServerActivity`, `enabled` is already false (first suspend already called `deinit()`), so this second `suspend()` returns early. The `suspended` flag stays true from the first call. This is correct.

However, when `WifiSelectionActivity` is launched from `SettingsActivity` standalone (line 185), there is no prior `suspend()` call. `WifiSelectionActivity.onEnter()` properly suspends, and the result handler at line 190 properly resumes. This is correct.

**Reclassifying to Warning -- no actual data corruption, but confusing double-suspend pattern.**

**Recommendation:** Consider making `suspend()` idempotent by removing the `if (!enabled) return` guard and instead tracking the suspend state independently:
```cpp
void BleRemoteManager::suspend() {
  suspended = true;
  if (enabled) deinit();
}
```
This makes intent clearer.

---

## Warning Issues

### 2. NimBLEAddress constructed with hardcoded type 0 (public address)

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 164

```cpp
reconnectAddress = NimBLEAddress(std::string(bondedAddr), 0);
```

The second argument `0` means `BLE_ADDR_PUBLIC`. Many consumer BLE devices use random addresses (type 1). The address stored in settings (`bleBondedDeviceAddr`) comes from `device->getAddress().toString()`, which does NOT preserve the address type. When reconstructing the address for reconnect, type 0 is assumed, which may fail for devices using random addresses.

**Impact:** Auto-reconnect may fail silently for some HID remotes that use BLE random addresses. The user would need to re-pair each time.

**Recommendation:** Store the address type alongside the address string in settings:
```cpp
// In CrossPointSettings.h:
uint8_t bleBondedDeviceAddrType = 0;

// In BleRemotePairingActivity.cpp connectToSelected():
SETTINGS.bleBondedDeviceAddrType = device.address.getType();

// In BleRemoteManager.cpp autoReconnect():
reconnectAddress = NimBLEAddress(std::string(bondedAddr), addrType);
```

### 3. `shouldAutoReconnect` not preserved through suspend/resume cycle

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` lines 170-187

```cpp
void BleRemoteManager::suspend() {
  if (!enabled) return;
  suspended = true;
  deinit();  // <-- deinit() does NOT touch shouldAutoReconnect
}

void BleRemoteManager::resume() {
  if (!suspended) return;
  suspended = false;
  init();
  if (shouldAutoReconnect) {  // <-- checks shouldAutoReconnect
    reconnectPending = true;
  }
}
```

`deinit()` does NOT reset `shouldAutoReconnect` (it only clears `enabled`, `connected`, `scanning`, `client`). This is CORRECT for the suspend/resume case.

However, `disconnect()` (line 147) sets `shouldAutoReconnect = false`. If something causes a disconnect before suspend is called, the auto-reconnect won't happen after resume. This is actually correct behavior -- explicit disconnect should disable auto-reconnect.

**This is fine.** Reclassifying to Info.

### 4. `tick()` calls `connectToDevice()` which is blocking

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` lines 189-195

```cpp
void BleRemoteManager::tick() {
  if (!enabled || connected || !reconnectPending) return;
  reconnectPending = false;
  connectToDevice(reconnectAddress);  // blocking: up to 5s timeout
}
```

`tick()` is called from the main loop. `connectToDevice()` calls `client->connect(addr)` with a 5-second timeout (line 102). During this time, the main loop is blocked -- no button input, no screen updates, no sleep timer.

On an e-paper reader this is acceptable (no real-time display updates needed), and it happens only at boot or after WiFi suspension ends. But it could delay sleep entry by up to 5 seconds.

**Recommendation:** Consider adding a max retry count or exponential backoff. Currently if the remote is off, `reconnectPending` is set to false after one failed attempt and never retried (unless WiFi suspend/resume triggers it again). This is actually acceptable behavior for an e-reader.

### 5. HID report parsing assumes standard keyboard report format only

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` lines 239-256

```cpp
void BleRemoteManager::onHidReport(const uint8_t* data, size_t len) {
  uint8_t buttonState = 0;
  if (len >= 3) {
    // Standard keyboard HID report: [modifier, reserved, key1, ..., key6]
    for (size_t i = 2; i < len && i < 8; i++) {
      ...
    }
  }
  gpio.setBleButtonState(buttonState);
}
```

Many presentation clickers send Consumer Control reports (Usage Page 0x0C) rather than Keyboard reports (Usage Page 0x07). Consumer Control reports have a different format -- typically 2 bytes with usage codes like `0x00B5` (Scan Next Track) or `0x00CD` (Play/Pause). The current code only handles standard keyboard keycodes.

**Impact:** Some popular remotes (e.g., Logitech R500, Kensington presenters) may not work or only partially work because they send Consumer Control usage codes.

**Recommendation:** Check the HID Report descriptor or handle both report formats. For a future iteration, consider parsing the HID Report Map characteristic (UUID 0x2A4B) to determine the report format. For now, documenting which remotes are supported is the minimum.

### 6. BleRemotePairingActivity.cpp is 359 lines, exceeds 200-line guideline

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp`

The file is 359 lines. Per project code standards, files should be under 200 lines. The render methods (lines 200-358) could be extracted.

**Recommendation:** Extract render helpers to a separate file or reduce by inlining simpler renders.

---

## Info Issues

### 7. `std::mutex` usage on single-core ESP32-C3

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.h` line 66

```cpp
std::mutex devicesMutex;
```

On single-core ESP32-C3, `std::mutex` compiles to FreeRTOS mutex. NimBLE callbacks run on the NimBLE task (separate FreeRTOS task), so the mutex is actually necessary and correct here for protecting `discoveredDevices` between the NimBLE task and the main loop.

**Verdict:** Correct usage. No change needed.

### 8. BleRemotePairingActivity copies discovered devices vector multiple times

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp`

`getDiscoveredDevices()` returns a copy of the vector (by value, line 151 in BleRemoteManager.cpp). This is called at lines 57, 143, 296, and 326 in the pairing activity. Each call copies the vector and all `BleDiscoveredDevice` objects (which contain `std::string` and `NimBLEAddress`).

**Impact:** Minor. Scan results are typically <10 devices, and these calls happen during user-driven UI interactions, not tight loops.

### 9. `extern BleRemoteManager bleManager` declared in multiple files

**Files:** Multiple activity `.cpp` files and `main.cpp`

The `extern` declaration appears in 6+ files individually rather than in a shared header. This is consistent with the existing codebase pattern (other globals like `activityManager` follow the same approach), so no change needed. But if future maintenance becomes an issue, a forward-declaration header could help.

### 10. `pairedStr` buffer in renderIdle() is 64 bytes

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp` line 251

```cpp
char pairedStr[64];
snprintf(pairedStr, sizeof(pairedStr), tr(STR_BLE_PAIRED_WITH), SETTINGS.bleBondedDeviceName);
```

`SETTINGS.bleBondedDeviceName` is 32 bytes. The translated format string `"Paired: %s"` adds ~10 bytes. Total max ~42, well within 64 bytes. Safe.

---

## File-by-File Review

### `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.h` (80 lines)
- Clean, well-organized header
- Appropriate use of `volatile` for cross-task flags (`connected`, `scanning`, `reconnectPending`)
- Forward declaration of `HalGPIO` avoids circular includes
- `NimBLEScanCallbacks` and `NimBLEClientCallbacks` inheritance is correct NimBLE pattern
- **Good:** Thread-safe `getDiscoveredDevices()` returns by value

### `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` (266 lines)
- Exceeds 200-line guideline but is a cohesive single-responsibility class
- NimBLE initialization correct: `setSecurityAuth(true, false, true)` enables bonding + secure connections
- Scan parameters (interval 100, window 99) are near-continuous active scan -- good for pairing UX
- `connectToDevice()` properly discovers HID service 0x1812, subscribes to Report characteristics 0x2A4D
- Lambda capture in `subscribe()` callback (line 122) captures `this` -- safe because BleRemoteManager outlives the connection
- `deinit()` properly stops scan and disconnects before calling `NimBLEDevice::deinit(true)`
- **Key map is well-chosen** for presentation remotes: arrows, page up/down, enter, backspace, space

### `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.h` (51 lines)
- Clean activity header following project patterns
- State enum is clear and complete
- `ButtonNavigator` member for consistent navigation

### `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp` (359 lines)
- **Over 200-line guideline** (see Warning #6)
- State machine is well-structured: IDLE -> SCANNING -> SCAN_COMPLETE -> CONNECTING -> CONNECTED/ERROR
- `connectToSelected()` correctly persists bonded device info via `strncpy` with null-termination
- `onExit()` properly stops any in-progress scan
- Signal strength indicator is identical to WiFi's version -- minor DRY opportunity but acceptable
- Renders are clean, use UITheme metrics consistently
- `renderConnecting()` truncates long device names at 28 chars with "..." -- good UX

### `/Users/trilm/DEV/crosspoint-reader/lib/hal/HalGPIO.h` (65 lines)
- BLE state fields placed outside `#if CROSSPOINT_EMULATED == 0` guard -- correct
- `volatile uint8_t bleState` is appropriate for cross-task writes (NimBLE task -> main loop)
- `setBleButtonState()` is a simple atomic store -- safe for single-core
- Edge detection (`prevBleState`, `bleWasPressed`, `bleWasReleased`) follows existing `InputManager` pattern

### `/Users/trilm/DEV/crosspoint-reader/lib/hal/HalGPIO.cpp` (59 lines)
- Edge detection in `update()` is clean: snapshot `bleState`, compute edges via bitmask
- `isPressed()` combines physical and BLE with OR -- correct
- `wasPressed()`/`wasReleased()` OR physical edges with BLE edges -- correct
- `wasAnyPressed()`/`wasAnyReleased()` check BLE separately from physical -- correct
- **No BLE held-time tracking:** `getHeldTime()` only returns physical button held time. BLE buttons can trigger `wasPressed` but not `isPressed`+held-time combos. This means long-press chapter skip won't work with BLE remotes. This is acceptable as a known limitation for V1.

### `/Users/trilm/DEV/crosspoint-reader/src/CrossPointSettings.h` (232 lines)
- BLE settings added at end of member list (lines 198-201) -- clean
- `bleBondedDeviceAddr[18]` correctly sized for "XX:XX:XX:XX:XX:XX\0"
- `bleBondedDeviceName[32]` reasonable for device display names

### `/Users/trilm/DEV/crosspoint-reader/src/JsonSettingsIO.cpp` (362 lines)
- BLE settings save (lines 128-130) and load (lines 192-200) follow existing patterns exactly
- `strncpy` + null-termination pattern matches OPDS URL handling
- No clamp needed for `bleEnabled` since it's 0/1 toggle -- minor; could clamp to be safe
- **Exceeds 200-line guideline** but this is a pre-existing issue, not introduced by BLE changes

### `/Users/trilm/DEV/crosspoint-reader/src/SettingsList.h` (137 lines)
- BLE toggle added in Controls category (lines 73-74) -- appropriate placement
- `SettingInfo::Toggle` with key "bleEnabled" for web API compatibility -- correct

### `/Users/trilm/DEV/crosspoint-reader/src/main.cpp` (425 lines)
- BLE init at setup (lines 262-265): correctly gated on `bleEnabled && bleBondedDeviceAddr[0]`
- BLE deinit in `enterDeepSleep()` (line 190): correctly placed before display sleep
- Main loop BLE management (lines 322-332): handles enable/disable toggle + suspend awareness
- **Key pattern:** `if (!bleManager.isEnabled() && !bleManager.isSuspended())` prevents re-init during WiFi suspension -- this is the fix for the known recurring issue documented in agent memory
- `bleManager.tick()` placement after enable check -- correct ordering
- **Pre-existing issue:** `main.cpp` is 425 lines, well over 200-line guideline

### `/Users/trilm/DEV/crosspoint-reader/platformio.ini`
- NimBLE-Arduino ^2.1.0 added to `lib_deps` -- appropriate version
- NimBLE build flags are well-tuned:
  - `CONFIG_BT_NIMBLE_ROLE_CENTRAL=1` -- needed for HID client
  - `CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=0` -- saves RAM
  - `CONFIG_BT_NIMBLE_ROLE_BROADCASTER=0` -- saves RAM
  - `CONFIG_BT_NIMBLE_ROLE_OBSERVER=1` -- needed for scanning
  - `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1` -- saves RAM (~1KB per extra connection)
- These flags reduce NimBLE RAM footprint from ~80KB to ~50-60KB -- well-considered for 380KB total

### `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/SettingsActivity.cpp` (271 lines)
- BLE Pair Device action added to controls category (line 53) -- appropriate
- `BleRemote` case in `toggleCurrentSetting()` (lines 204-208) launches pairing activity correctly
- `extern BleRemoteManager bleManager` in Network case (line 184) for WiFi cleanup -- acceptable pattern

### WiFi-using activities (all modified)
All 6 WiFi-using activities correctly implement the suspend/resume pattern:
1. `CrossPointWebServerActivity` -- suspend in onEnter, resume in onExit
2. `WifiSelectionActivity` -- suspend in onEnter, NO resume in onExit (parent handles it)
3. `OtaUpdateActivity` -- suspend in onEnter, resume in onExit
4. `KOReaderAuthActivity` -- suspend in onEnter, resume in onExit
5. `OpdsBookBrowserActivity` -- suspend in onEnter, resume in onExit
6. `KOReaderSyncActivity` -- suspend in onEnter, resume in onExit

---

## Memory/Safety Concerns

1. **NimBLE RAM:** Central-only config with single connection uses ~50-60KB. With 380KB total and existing usage, this is tight but workable. The firmware already logs free heap every 10s in the main loop.

2. **Stack safety:** NimBLE callbacks run on the NimBLE task with default 4KB stack. `onHidReport()` is lightweight (bitmask operations). `onResult()` does `std::lock_guard` + `push_back` -- safe for typical scan results (<20 devices).

3. **Heap fragmentation:** `discoveredDevices` vector grows dynamically during scan. After scan completes, the vector is copied by value in `getDiscoveredDevices()`. Both vectors are short-lived (cleared on next scan or activity exit). Minimal fragmentation risk.

4. **No memory leaks:** `NimBLEDevice::deinit(true)` releases all NimBLE resources. The `client` pointer is managed by NimBLE's internal pool -- setting it to `nullptr` in `deinit()` is correct since `deinit(true)` deletes it.

5. **Thread safety:** `volatile` flags + `std::mutex` for discovered devices is appropriate for single-core ESP32-C3 with FreeRTOS tasks. The `setBleButtonState()` write from NimBLE task and `update()` read from main loop is safe because `uint8_t` read/write is atomic on RISC-V.

---

## Recommended Fixes (Prioritized)

1. **[Warning]** Store BLE address type in settings to handle random-address devices (Issue #2)
2. **[Warning]** Consider Consumer Control HID report parsing for broader remote compatibility (Issue #5)
3. **[Info]** Make `suspend()` idempotent by setting `suspended = true` before the `enabled` guard (Issue #1 recommendation)
4. **[Info]** Split `BleRemotePairingActivity.cpp` render methods to reduce file size (Issue #6)
5. **[Info]** Clamp `bleEnabled` during JSON load for consistency with other settings

---

## Positive Observations

1. **Elegant GPIO injection:** BLE button state merging at the HalGPIO level means zero changes to any activity's input handling code. All existing button logic (navigation, page turns, long press) works transparently.

2. **Consistent WiFi/BLE exclusion:** Every WiFi-using activity properly suspends/resumes BLE. The pattern is copy-paste consistent across all 6 activities.

3. **Clean NimBLE integration:** Scan callbacks, client callbacks, and HID report subscription follow NimBLE best practices. The `setScanCallbacks(this)` approach avoids raw function pointers.

4. **RAM-conscious config:** NimBLE build flags disable unused roles, saving significant RAM on the constrained ESP32-C3.

5. **Settings persistence follows existing patterns:** JSON serialization matches the exact style of existing OPDS, WiFi, and KOReader settings handling.

6. **Deep sleep handling:** BLE is properly deinitialized before deep sleep, and auto-reconnect is correctly re-queued after resume.

7. **User experience:** The pairing flow (scan -> select -> connect -> auto-finish) is intuitive and provides appropriate feedback at each stage. Signal strength indicators match the WiFi selection UI.

---

## Unresolved Questions

1. Should BLE address type be stored now, or can it be deferred to a later release? (Depends on which remotes users are likely to use)
2. Is Consumer Control HID report parsing needed for V1, or can it be added based on user feedback?
3. Is there a maximum bound for the number of discovered devices vector, or should it be capped to prevent unbounded heap allocation during scan?
