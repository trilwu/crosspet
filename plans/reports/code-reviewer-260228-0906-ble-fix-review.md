# Code Review: BLE Page-Turn Remote Fixes

**Date:** 2026-02-28
**Scope:** BLE suspend/resume lifecycle, WiFi mutual exclusion, main loop BLE toggle
**Files:** 12 files reviewed
**Verdict:** 1 critical bug, 1 high-priority bug, 2 medium issues

---

## Critical Issues

### 1. CRITICAL: Main loop re-inits BLE during WiFi suspension

**File:** `/Users/trilm/DEV/crosspoint-reader/src/main.cpp` lines 321-331

```cpp
if (SETTINGS.bleEnabled) {
    if (!bleManager.isEnabled()) {   // <-- enabled==false after suspend()->deinit()
      bleManager.init();              // <-- RE-INITS BLE WHILE WIFI IS ACTIVE
      ...
    }
    bleManager.tick();
} else if (bleManager.isEnabled()) {
    bleManager.deinit();
}
```

**Problem:** `suspend()` calls `deinit()` which sets `enabled = false`. On the very next `loop()` iteration, the main loop sees `SETTINGS.bleEnabled == true` and `isEnabled() == false`, so it calls `init()` -- re-initializing BLE while WiFi is actively using the 2.4GHz radio. This completely defeats the suspend/resume mechanism.

**Impact:** BLE and WiFi will fight over the shared radio. Could cause WiFi disconnects, BLE corruption, crashes, or silent data corruption during OTA updates/file transfers.

**Fix:** Check `suspended` state in the main loop:

```cpp
if (SETTINGS.bleEnabled) {
    if (!bleManager.isEnabled() && !bleManager.isSuspended()) {
      bleManager.init();
      ...
    }
    bleManager.tick();  // tick() already guards on enabled
} else if (bleManager.isEnabled()) {
    bleManager.deinit();
}
```

Add public accessor to `BleRemoteManager.h`:
```cpp
bool isSuspended() const { return suspended; }
```

---

## High Priority

### 2. HIGH: WifiSelectionActivity launched from SettingsActivity leaks BLE suspension

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/SettingsActivity.cpp` line 182

```cpp
case SettingAction::Network:
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, false), resultHandler);
```

`WifiSelectionActivity::onEnter()` calls `bleManager.suspend()`, but its `onExit()` does NOT call `bleManager.resume()` (by design -- the comment says parent activity handles it). However, when launched directly from SettingsActivity, there IS no parent WiFi-owning activity. The resultHandler only calls `SETTINGS.saveToFile()`.

**Impact:** BLE stays permanently suspended after using Settings > Network. Only recoverable by toggling `bleEnabled` off/on, or rebooting. (Though this is partially masked by Critical Issue #1 above -- the main loop would re-init BLE anyway, which itself is the wrong fix.)

**Fix options:**
- (a) Add `bleManager.resume()` to WifiSelectionActivity's onExit when it was the one that suspended (track via a `didSuspendBle` flag)
- (b) Move `bleManager.resume()` into the SettingsActivity resultHandler for the Network action
- (c) Give WifiSelectionActivity a constructor parameter `manageBle = false` and let calling activities control it

Option (b) is simplest:
```cpp
case SettingAction::Network:
    startActivityForResult(
        std::make_unique<WifiSelectionActivity>(renderer, mappedInput, false),
        [this](const ActivityResult&) {
            WiFi.mode(WIFI_OFF);
            bleManager.resume();
            SETTINGS.saveToFile();
        });
```

---

## Medium Priority

### 3. MEDIUM: Double suspend when CalibreConnectActivity is child of CrossPointWebServerActivity

**Files:** `CrossPointWebServerActivity.cpp` line 42, `CalibreConnectActivity.cpp` line 25

Both call `bleManager.suspend()` in `onEnter()`. When CrossPointWebServer launches Calibre as a sub-activity, suspend is called twice. On exit, CalibreConnectActivity calls `resume()` which sets `suspended = false` and calls `init()` -- **while CrossPointWebServerActivity still has WiFi active**.

The flow:
1. CrossPointWebServer.onEnter -> suspend() [suspended=true, enabled=false]
2. CalibreConnect.onEnter -> suspend() [early return: !enabled]
3. CalibreConnect.onExit -> resume() [suspended=false, init() called -- BLE INITS WHILE WIFI STILL ACTIVE]
4. CrossPointWebServer continues operating with WiFi

**Wait** -- step 2: `suspend()` checks `if (!enabled) return;` so it does nothing. Then `suspended` is still `true` from step 1. Step 3: `resume()` checks `if (!suspended) return;` -- `suspended` is `true`, so it proceeds to `init()`. This IS a bug.

**Fix:** Use a reference count instead of boolean, or ensure child activities don't call suspend/resume when they know a parent already manages it. Simplest: remove `suspend()`/`resume()` from CalibreConnectActivity since its parent (CrossPointWebServerActivity) already manages the lifecycle.

### 4. MEDIUM: `shouldAutoReconnect` never reset to false

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 165

```cpp
void BleRemoteManager::autoReconnect(const char* bondedAddr) {
    ...
    shouldAutoReconnect = true;
```

Once set, `shouldAutoReconnect` is never cleared. If the user un-pairs the device (clears bonded address), the flag remains true and `resume()` will keep re-queuing reconnect attempts to a stale `reconnectAddress`. The `deinit()` method does not reset it either.

**Fix:** Reset in `deinit()`:
```cpp
void BleRemoteManager::deinit() {
    ...
    shouldAutoReconnect = false;
}
```

And/or reset when the user explicitly disconnects:
```cpp
void BleRemoteManager::disconnect() {
    ...
    shouldAutoReconnect = false;
}
```

---

## Positive Observations

- `scanning = true` before `scan->start()` correctly prevents race with `onScanEnd` callback
- `client = nullptr` after `deinit()` prevents dangling pointer
- Idempotent guards on `suspend()`/`resume()` are good defensive coding
- `volatile` on `connected`, `scanning`, `reconnectPending` is appropriate for ISR/callback context
- HID report parsing is clean with proper bounds checking (`i < 8`)
- Thread-safe `devicesMutex` usage in scan callbacks is correct
- Key mapping table is well-structured and easily extensible

---

## Summary of Required Fixes

| # | Severity | Issue | File |
|---|----------|-------|------|
| 1 | CRITICAL | Main loop re-inits BLE during WiFi suspend | `main.cpp:321` |
| 2 | HIGH | Settings>Network leaves BLE permanently suspended | `SettingsActivity.cpp:182` |
| 3 | MEDIUM | CalibreConnect resume() re-inits BLE under CrossPointWebServer's WiFi | `CalibreConnectActivity.cpp:70` |
| 4 | MEDIUM | `shouldAutoReconnect` never reset | `BleRemoteManager.cpp:165` |

---

## Unresolved Questions

1. Is `volatile` sufficient for `connected`/`scanning`/`reconnectPending` on ESP32-C3 (single-core RISC-V), or should these use `std::atomic<bool>`? On single-core, `volatile` is likely sufficient since there is no cache coherency issue, but `std::atomic` would be more portable and express intent more clearly.
2. Should there be a timeout/retry limit on auto-reconnect in `tick()` to prevent infinite reconnect attempts to an unreachable device?
3. The `connectToDevice()` call in `tick()` is blocking (5s timeout) and runs in the main loop. Could this cause watchdog timeouts or UI freezes on the e-ink display?
