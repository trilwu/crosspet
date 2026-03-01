# BLE Page-Turn Remote -- Deep Code Review

**Reviewer:** code-reviewer
**Date:** 2026-02-28
**Scope:** All new + modified files for BLE HID page-turn remote feature
**Focus:** Correctness, missed integration points, concurrency, API compatibility

---

## Critical Issues

### 1. CRITICAL: `tick()` blocks main loop for up to 10 seconds

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 184-190
**File:** `/Users/trilm/DEV/crosspoint-reader/src/main.cpp` line 320-322

`tick()` calls `connectToDevice()` which blocks up to 10 seconds (`client->setConnectTimeout(10)`). This is invoked every main `loop()` iteration when `reconnectPending == true`.

**Impact:**
- UI completely freezes for 10 seconds. E-ink display unresponsive.
- Auto-sleep timer does NOT reset during block (line 350: `gpio.wasAnyPressed()` returns false because `gpio.update()` is not called during the block).
- If auto-sleep timeout is 1 minute and user has been idle 55s, the 10s block finishes, then the very next loop iteration triggers auto-sleep.
- `activityManager.loop()` not called -- any running webserver or Calibre transfer would stall.

**Suggested fix:**
Use `client->connect(addr, true, true)` (asyncConnect=true) in `tick()` and poll `client->isConnected()` on subsequent ticks. Alternatively, reduce `setConnectTimeout()` to 3 seconds and set `reconnectPending` with exponential backoff so failed reconnects don't block every loop.

```cpp
void BleRemoteManager::tick() {
  if (!enabled || connected || !reconnectPending || suspended) return;

  static unsigned long lastAttempt = 0;
  static unsigned long backoffMs = 5000;
  if (millis() - lastAttempt < backoffMs) return;

  lastAttempt = millis();
  reconnectPending = false;

  // Use short timeout for auto-reconnect
  client = NimBLEDevice::getClientByPeerAddress(reconnectAddress);
  if (!client) {
    client = NimBLEDevice::createClient(reconnectAddress);
    if (!client) return;
  }
  client->setClientCallbacks(this, false);
  client->setConnectTimeout(3);

  if (!client->connect(reconnectAddress, true, true)) { // async
    reconnectPending = true;
    backoffMs = std::min(backoffMs * 2, 30000UL);
  } else {
    backoffMs = 5000;
  }
}
```

---

### 2. CRITICAL: 5 WiFi activities MISSING BLE suspend/resume

**Files missing `bleManager.suspend()` / `bleManager.resume()`:**

| Activity | File | WiFi Usage |
|---|---|---|
| OtaUpdateActivity | `src/activities/settings/OtaUpdateActivity.cpp` | `WiFi.mode(WIFI_STA)` in `onEnter()`, `WiFi.mode(WIFI_OFF)` in `onExit()` |
| KOReaderAuthActivity | `src/activities/settings/KOReaderAuthActivity.cpp` | `WiFi.mode(WIFI_STA)` in `onEnter()`, `WiFi.mode(WIFI_OFF)` in `onExit()` |
| KOReaderSyncActivity | `src/activities/reader/KOReaderSyncActivity.cpp` | Uses WifiSelectionActivity OR direct `WiFi.status()` check |
| OpdsBookBrowserActivity | `src/activities/browser/OpdsBookBrowserActivity.cpp` | `WiFi.mode(WIFI_OFF)` in `onExit()` |
| CalibreConnectActivity | `src/activities/network/CalibreConnectActivity.cpp` | `WiFi.mode(WIFI_OFF)` in `onExit()` |

**Nuance:** OtaUpdateActivity, KOReaderAuthActivity, KOReaderSyncActivity, and CalibreConnectActivity all launch `WifiSelectionActivity` as a sub-activity, which already does `suspend()`/`resume()`. However:
- **OtaUpdateActivity** calls `WiFi.mode(WIFI_STA)` **BEFORE** launching WifiSelectionActivity (line 58), so WiFi is already active when WifiSelectionActivity suspends BLE. This is fine for suspend, but `WifiSelectionActivity::onExit()` calls `bleManager.resume()` -- then OtaUpdateActivity continues to use WiFi (for OTA download). BLE is now active **while WiFi is active** during the OTA download.
- **KOReaderAuthActivity** similarly calls `WiFi.mode(WIFI_STA)` before WifiSelection, and if already connected (line 58), **never launches WifiSelectionActivity at all** -- so BLE is never suspended.
- **KOReaderSyncActivity** -- same issue: if `WiFi.status() == WL_CONNECTED` (line 205), it skips WifiSelectionActivity entirely. BLE stays active during sync.
- **OpdsBookBrowserActivity** -- calls `checkAndConnectWifi()` which may launch WifiSelectionActivity, but if already connected, skips it.
- **CalibreConnectActivity** -- checks `WiFi.status() != WL_CONNECTED` (line 34), skips WifiSelectionActivity if already connected.

**Impact:** ESP32-C3 shares a single radio for BLE and WiFi 2.4GHz. Running both simultaneously causes interference, reduced throughput, connection instability, and increased power consumption. On some firmware versions, this causes hard crashes.

**Suggested fix:** Each WiFi-using activity should independently call `bleManager.suspend()` in `onEnter()` and `bleManager.resume()` in `onExit()`. The `suspend()`/`resume()` calls are already idempotent (`if (!enabled) return` / `if (!suspended) return`), so double-suspend from WifiSelectionActivity + parent is safe. Alternatively, add suspend/resume around the WiFi-active portion of each activity's lifecycle.

---

### 3. CRITICAL: `resume()` does not re-queue auto-reconnect

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 174-182

After `resume()`, the comment says "Caller should provide bonded address from settings." Only `WifiSelectionActivity::onExit()` (line 84-87) does this correctly. But if any other code path calls `resume()` without also calling `autoReconnect()`, the device will never reconnect to the remote after WiFi usage.

Currently only `WifiSelectionActivity::onExit()` calls `resume()`, so this is safe for now. But if the fix for issue #2 adds `bleManager.resume()` to other activities' `onExit()`, those callers must also call `autoReconnect()`.

**Suggested fix:** Move the auto-reconnect logic into `resume()` itself:
```cpp
void BleRemoteManager::resume() {
  if (!suspended) return;
  suspended = false;
  init();

  // Auto-reconnect if bonded address exists in settings
  if (strlen(SETTINGS.bleBondedDeviceAddr) > 0) {
    autoReconnect(SETTINGS.bleBondedDeviceAddr);
  }
  LOG_DBG("BLE", "Resumed after WiFi");
}
```

---

## High Priority Issues

### 4. HIGH: Settings toggle ON/OFF does not init/deinit BLE

**File:** `/Users/trilm/DEV/crosspoint-reader/src/SettingsList.h` line 73-74
**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/SettingsActivity.cpp` line 151-154

When user toggles `bleEnabled` OFF in Settings, the code simply flips the uint8_t value. BLE continues running. The setting only takes effect on next boot (checked in `main.cpp setup()` line 262 and `loop()` line 320).

**Impact:**
- Toggle OFF: BLE remains active, consuming power and radio bandwidth until next reboot.
- Toggle ON: BLE does not start until next reboot.
- Confusing UX: user expects immediate effect.

**Suggested fix:** Add a callback or side-effect in `toggleCurrentSetting()` specifically for the BLE toggle:

```cpp
// After toggling bleEnabled:
if (SETTINGS.bleEnabled) {
  bleManager.init();
  if (strlen(SETTINGS.bleBondedDeviceAddr) > 0) {
    bleManager.autoReconnect(SETTINGS.bleBondedDeviceAddr);
  }
} else {
  bleManager.deinit();
}
```

This requires either making `toggleCurrentSetting()` aware of BLE-specific side effects, or using a more general post-toggle callback pattern.

---

### 5. HIGH: `getHeldTime()` always returns 0 for BLE button presses

**File:** `/Users/trilm/DEV/crosspoint-reader/lib/hal/HalGPIO.cpp` line 36

```cpp
unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }
```

BLE button presses only set `bleState` / `bleWasPressed` bitmasks. They do not contribute to `getHeldTime()`. This means:

- **Long-press back to go to library** (`EpubReaderActivity.cpp:153`, `XtcReaderActivity.cpp:72`, `TxtReaderActivity.cpp:78`): BLE users can NEVER trigger long-press-back. The condition `getHeldTime() >= goHomeMs` will always be false.
- **Long-press chapter skip** (`EpubReaderActivity.cpp:194`, `XtcReaderActivity.cpp:108`): BLE users can NEVER do chapter skip via long-press side buttons.
- **Power button hold detection** (`main.cpp:377`): Not affected (BLE doesn't map to BTN_POWER -- verified in mapping table).
- **ButtonNavigator continuous scrolling** (`ButtonNavigator.cpp:70`): BLE users cannot hold a button to continuously scroll through lists.

**Impact:** Significant UX degradation for BLE-only users. They lose access to long-press actions entirely.

**Suggested fix:** Track BLE button hold duration in `HalGPIO`:

```cpp
// In HalGPIO.h
unsigned long bleHeldSince = 0;

// In HalGPIO::update()
if (currentBle != 0 && prevBleState == 0) {
  bleHeldSince = millis();
} else if (currentBle == 0) {
  bleHeldSince = 0;
}

// In HalGPIO::getHeldTime()
unsigned long physicalHeld = inputMgr.getHeldTime();
unsigned long bleHeld = (bleHeldSince > 0) ? (millis() - bleHeldSince) : 0;
return std::max(physicalHeld, bleHeld);
```

---

### 6. HIGH: `onDisconnect()` callback fires on NimBLE task thread -- concurrent access to `gpio`

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 226-229

```cpp
void BleRemoteManager::onDisconnect(NimBLEClient* client, int reason) {
  connected = false;
  gpio.setBleButtonState(0);  // Called from NimBLE FreeRTOS task
}
```

`gpio.setBleButtonState(0)` writes to `volatile uint8_t bleState`. Meanwhile, `gpio.update()` (main loop task, `main.cpp:318`) reads `bleState` and computes edge detection. Both run on different FreeRTOS tasks.

Similarly, `onHidReport()` (line 234-251) calls `gpio.setBleButtonState()` from the NimBLE notify callback, which also runs on the NimBLE task.

While `volatile` ensures visibility, it does NOT guarantee atomicity of the read-modify-write pattern in `update()`. On ESP32-C3 (single-core RISC-V), the NimBLE task runs on the same core, so preemption is the concern, not true parallel execution. The `update()` function reads `bleState` once into a local:

```cpp
uint8_t currentBle = bleState;  // single byte read -- atomic on RISC-V
```

This is safe for single-byte reads. However, the `onDisconnect` callback could fire mid-`update()` between reading `bleState` and writing `prevBleState`, causing a missed edge. The worst case is a phantom button press/release that lasts one frame -- low severity.

**Severity assessment:** LOW on ESP32-C3 single-core (preemptive but single-core makes byte operations practically atomic). Would be HIGH on dual-core ESP32.

---

### 7. HIGH: `std::mutex` may not be FreeRTOS-safe on ESP32-C3

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.h` line 64

`std::mutex devicesMutex` is used in `onResult()` (NimBLE task) and `getDiscoveredDevices()`/`clearDiscoveredDevices()` (main task). On ESP-IDF with C++ threading support enabled (which Arduino-ESP32 does), `std::mutex` maps to `pthread_mutex_t` which maps to FreeRTOS `xSemaphoreCreateMutex`. This is safe.

However, holding a mutex in a NimBLE scan callback could delay the NimBLE host task. If the main loop holds the lock for an extended period (e.g., during `renderScanComplete()` which copies the vector), NimBLE scan results might be dropped.

**Impact:** Unlikely to be a real problem in practice (lock hold times are microseconds), but worth noting for future debugging.

---

## Medium Priority Issues

### 8. MEDIUM: `WifiSelectionActivity::onExit()` always calls `bleManager.resume()` even if BLE was never enabled

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/network/WifiSelectionActivity.cpp` line 83-87

```cpp
bleManager.resume();
if (SETTINGS.bleEnabled && strlen(SETTINGS.bleBondedDeviceAddr) > 0) {
  bleManager.autoReconnect(SETTINGS.bleBondedDeviceAddr);
}
```

If `SETTINGS.bleEnabled == 0`, `resume()` still calls `init()` (line 177: `init()` sets `enabled = true`). Then `tick()` in the main loop (guarded by `SETTINGS.bleEnabled`, line 320) won't call `tick()`, but BLE is now initialized and consuming power/memory even though the user disabled it.

**Wait** -- checking `suspend()`:
```cpp
void BleRemoteManager::suspend() {
  if (!enabled) return;  // <- returns if BLE was never enabled
  suspended = true;
  deinit();
}
```

If BLE was never enabled, `suspend()` returns without setting `suspended = true`. Then `resume()` checks `if (!suspended) return;` -- returns early. So this is actually **safe**. No issue.

**Revised assessment:** The guard chain is correct. `suspend()` returns early if BLE not enabled, so `suspended` stays false, so `resume()` returns early. No bug.

---

### 9. MEDIUM: Pairing activity calls `bleManager.init()` even when `bleEnabled == 0`

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp` line 52

```cpp
void BleRemotePairingActivity::startScanning() {
  ...
  bleManager.init();
  bleManager.startScan(10);
}
```

User can navigate to Settings > Controls > Pair Remote even when `bleEnabled == 0`. The `startScanning()` method calls `bleManager.init()` which sets `enabled = true`. After pairing, BLE is now enabled at the manager level, but `SETTINGS.bleEnabled` is still 0. Result:
- `tick()` in main loop is guarded by `SETTINGS.bleEnabled` -- won't run.
- BLE is initialized but no reconnection logic runs.
- `enterDeepSleep()` calls `bleManager.deinit()` -- this is fine, will clean up.

**Impact:** Minor -- BLE radio stays on unnecessarily after leaving pairing activity until next sleep, wasting power. Also, if user pairs a device while bleEnabled=0 and doesn't toggle it ON, the pairing works but auto-reconnect won't happen on boot.

**Suggested fix:** Either (a) hide "Pair Remote" menu item when `bleEnabled == 0`, or (b) set `SETTINGS.bleEnabled = 1` when user successfully pairs a device:

```cpp
// In connectToSelected(), after successful connection:
if (!SETTINGS.bleEnabled) {
  SETTINGS.bleEnabled = 1;
}
```

---

### 10. MEDIUM: No BLE deinit on `onExit()` of BleRemotePairingActivity

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp` line 34-41

```cpp
void BleRemotePairingActivity::onExit() {
  Activity::onExit();
  if (bleManager.isScanning()) {
    bleManager.stopScan();
  }
}
```

If user entered pairing, started scan, found no devices, pressed back -- BLE was `init()`'d but never `deinit()`'d. If `SETTINGS.bleEnabled == 0`, BLE stays initialized until deep sleep.

If `SETTINGS.bleEnabled == 1`, this is actually correct behavior (BLE should stay running for the bonded remote).

**Impact:** Minor power waste when `bleEnabled == 0`.

---

### 11. MEDIUM: `connectToDevice()` uses `NimBLEDevice::createClient(addr)` which auto-sets peer address

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 92, 103

```cpp
client = NimBLEDevice::createClient(addr);
...
if (!client->connect(addr)) {
```

`createClient(addr)` already sets the peer address. Then `connect(addr)` is called with the same address. This is redundant but not harmful -- NimBLE v2.1 `connect(address)` overload works correctly even if peer address was already set. No bug.

---

### 12. MEDIUM: NimBLE client not explicitly deleted after `deinit(true)`

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 47-58

```cpp
void BleRemoteManager::deinit() {
  if (!enabled) return;
  stopScan();
  disconnect();
  NimBLEDevice::deinit(true);  // clearAll=true
  enabled = false;
  connected = false;
  scanning = false;
}
```

`NimBLEDevice::deinit(true)` with `clearAll=true` deletes all clients, services, etc. This means the `client` pointer (line 61 in header) becomes a dangling pointer. Next time `tick()` -> `connectToDevice()` runs after `resume()`, it calls `NimBLEDevice::getClientByPeerAddress(addr)` which returns nullptr (clients deleted), so it creates a new client. This is fine.

However, the `client` member still points to freed memory. If any code checks `client != nullptr` or calls `client->isConnected()` between `deinit()` and the next `connectToDevice()`, it's UB.

Looking at `disconnect()` (line 141):
```cpp
void BleRemoteManager::disconnect() {
  if (client && client->isConnected()) {  // <-- reads dangling pointer after deinit
```

But `deinit()` calls `disconnect()` BEFORE `NimBLEDevice::deinit(true)`, so the order is safe in `deinit()`. However, if `disconnect()` is called independently after `deinit()` (e.g., from `forgetDevice()` -> `bleManager.disconnect()`), `client` is dangling.

**Call flow check:** `forgetDevice()` (line 86) -> `bleManager.disconnect()` -> checks `client && client->isConnected()`. If BLE was never initialized or was deinitialized, `client` could be dangling.

**Suggested fix:** Set `client = nullptr` in `deinit()`:
```cpp
void BleRemoteManager::deinit() {
  if (!enabled) return;
  stopScan();
  disconnect();
  NimBLEDevice::deinit(true);
  client = nullptr;  // <-- add this
  enabled = false;
  ...
}
```

---

### 13. MEDIUM: `discoveredDevices` vector not cleared in `deinit()`

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 47-58

The `discoveredDevices` vector (which may contain `NimBLEAddress` objects) is not cleared during `deinit()`. `NimBLEAddress` is a value type (wraps `ble_addr_t`), so there's no dangling pointer issue. But it's wasteful to keep stale scan results in memory.

**Suggested fix:** Add `clearDiscoveredDevices()` to `deinit()`.

---

### 14. MEDIUM: Power button could theoretically be triggered via BLE

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 25-31
**File:** `/Users/trilm/DEV/crosspoint-reader/lib/hal/HalGPIO.h` line 64

The key mapping table maps HID keycodes to buttons 0-5 (BTN_BACK through BTN_DOWN). BTN_POWER = 6. No HID keycode is mapped to button 6.

However, `HalGPIO::isPressed()` checks `prevBleState & (1 << buttonIndex)`. If a BLE HID device sends keycode 0x2C (SPACE, mapped to BTN_RIGHT=3), bit 3 is set. No issue for power (bit 6).

But what if a malicious or buggy HID device sends raw report data that somehow causes `buttonState |= (1 << 6)`? Looking at `onHidReport()`: `mapKeycodeToButton()` returns 0xFF for unmapped keys, and the code skips when `btn == 0xFF`. The maximum mapped button index is BTN_DOWN=5. Bit 6 cannot be set through the mapping table.

**Verdict:** Safe. BTN_POWER cannot be triggered via BLE. The 8-bit bitmask does mean bits 6 and 7 could theoretically be set if a mapping were added, but current code prevents this.

---

## Low Priority Issues

### 15. LOW: `scanning` flag set AFTER `scan->start()` (race condition with `onScanEnd`)

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 60-73

```cpp
scan->start(durationSec, false);  // non-blocking
scanning = true;                  // set after start
```

If the scan completes extremely quickly (or fails immediately), `onScanEnd()` fires on the NimBLE task and sets `scanning = false` before line 71 executes. Then `scanning` is set to `true` even though scanning has ended.

**Impact:** `BleRemotePairingActivity` polls `bleManager.isScanning()` in its `loop()` and would never see the scan end. The user would be stuck on "Scanning..." forever.

**Suggested fix:** Set `scanning = true` before `scan->start()`.

---

### 16. LOW: `onScanEnd` acquires `devicesMutex` unnecessarily

**File:** `/Users/trilm/DEV/crosspoint-reader/src/ble/BleRemoteManager.cpp` line 215-218

```cpp
void BleRemoteManager::onScanEnd(const NimBLEScanResults& results, int reason) {
  scanning = false;
  std::lock_guard<std::mutex> lock(devicesMutex);
  LOG_DBG("BLE", "Scan ended, found %zu HID devices", discoveredDevices.size());
}
```

The lock is only used to read `discoveredDevices.size()` for logging. This is harmless but unnecessary.

---

### 17. LOW: `renderScanComplete()` copies the entire device vector each frame

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp` line 296

```cpp
auto devices = bleManager.getDiscoveredDevices();  // copies vector
```

Called on every render. Since this is e-ink (infrequent renders), performance is fine. But `loop()` also copies it (line 143). Consider caching the copy once on state transition to `SCAN_COMPLETE`.

---

### 18. LOW: Translation string format inconsistency

**File:** `/Users/trilm/DEV/crosspoint-reader/lib/I18n/translations/english.yaml` line 363

```yaml
STR_BLE_PAIRED_WITH: "Paired: %s"
```

Used with `snprintf` (line 252 of BleRemotePairingActivity.cpp). Buffer is only 64 bytes. If device name is very long (up to 31 chars from settings), total could be ~39 chars. Buffer is adequate.

---

### 19. LOW: `BleRemotePairingActivity` file is 359 lines -- exceeds 200-line guideline

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/settings/BleRemotePairingActivity.cpp`

Per project rules, files should be under 200 lines. The render helpers (lines 201-358) could be extracted to a separate file.

---

## Emulated Build Check (Item #2)

**Finding:** `CROSSPOINT_EMULATED` is only referenced in `lib/hal/HalGPIO.h` (line 22). There is no `HalGPIO_emulated.cpp` in the codebase. The build flag is not defined in `platformio.ini` for any environment.

The BLE state fields (`bleState`, `prevBleState`, `bleWasPressed`, `bleWasReleased`) and `setBleButtonState()` are declared OUTSIDE the `#if CROSSPOINT_EMULATED == 0` guard, which is correct. However, `HalGPIO.cpp` references `inputMgr` unconditionally (without a guard), which means the entire file only compiles for hardware builds.

BLE code (`BleRemoteManager.cpp`) includes `<NimBLEDevice.h>`, which is ESP32-specific. If a native/emulated build exists, it would fail to compile `BleRemoteManager.cpp` unless excluded.

**Verdict:** The emulated build (if it exists) is likely broken by the BLE addition, but this may be intentional since BLE is hardware-only. Check if the build system excludes `src/ble/` for emulated targets.

---

## NimBLE v2.x API Compatibility Check (Item #8)

All API calls verified against the installed NimBLE-Arduino v2.1.0 headers in `.pio/libdeps/`:

| Call | v2 Header Signature | Status |
|---|---|---|
| `NimBLEDevice::createClient(addr)` | `static NimBLEClient* createClient(const NimBLEAddress&)` | OK |
| `NimBLEDevice::getClientByPeerAddress(addr)` | `static NimBLEClient* getClientByPeerAddress(const NimBLEAddress&)` | OK |
| `NimBLEDevice::deinit(true)` | `static bool deinit(bool clearAll = false)` | OK (returns bool in v2, code ignores return) |
| `client->connect(addr)` | `bool connect(const NimBLEAddress&, bool, bool, bool)` | OK |
| `client->setClientCallbacks(this, false)` | `void setClientCallbacks(NimBLEClientCallbacks*, bool deleteCallbacks = true)` | OK |
| `ch->subscribe(true, lambda)` | `bool subscribe(bool, const notify_callback, bool)` | OK -- callback signature matches: `(NimBLERemoteCharacteristic*, uint8_t*, size_t, bool)` |
| `hidService->getCharacteristics(true)` | `const std::vector<NimBLERemoteCharacteristic*>& getCharacteristics(bool refresh = false) const` | OK -- returns const ref |
| `NimBLEDevice::init("CrossPoint")` | `static bool init(const std::string&)` | OK (returns bool in v2, code ignores return) |

**Verdict:** All NimBLE API calls are compatible with v2.1.0. No v1 patterns detected.

---

## Summary of Findings by Severity

| # | Severity | Issue | File |
|---|---|---|---|
| 1 | CRITICAL | `tick()` blocks main loop 10s during auto-reconnect | `BleRemoteManager.cpp:189` |
| 2 | CRITICAL | 5 WiFi activities miss BLE suspend/resume | Multiple |
| 3 | CRITICAL | `resume()` doesn't auto-reconnect | `BleRemoteManager.cpp:174` |
| 4 | HIGH | Settings toggle doesn't init/deinit BLE | `SettingsActivity.cpp:151` |
| 5 | HIGH | `getHeldTime()` always 0 for BLE -- no long-press | `HalGPIO.cpp:36` |
| 6 | HIGH | `onDisconnect` concurrent access (low risk on single-core) | `BleRemoteManager.cpp:226` |
| 7 | HIGH | `std::mutex` in scan callback may delay NimBLE host | `BleRemoteManager.cpp:197` |
| 9 | MEDIUM | Pairing activity inits BLE even when bleEnabled=0 | `BleRemotePairingActivity.cpp:52` |
| 10 | MEDIUM | No BLE deinit on pairing activity exit | `BleRemotePairingActivity.cpp:34` |
| 12 | MEDIUM | Dangling `client` pointer after `deinit()` | `BleRemoteManager.cpp:47` |
| 13 | MEDIUM | `discoveredDevices` not cleared in `deinit()` | `BleRemoteManager.cpp:47` |
| 15 | LOW | `scanning` flag race with `onScanEnd` | `BleRemoteManager.cpp:70` |
| 16 | LOW | Unnecessary mutex in `onScanEnd` | `BleRemoteManager.cpp:217` |
| 17 | LOW | Vector copy per render frame | `BleRemotePairingActivity.cpp:296` |
| 19 | LOW | File exceeds 200-line guideline | `BleRemotePairingActivity.cpp` |

---

## Positive Observations

- Clean separation of BLE manager from GPIO layer via `setBleButtonState()` bitmask injection
- Edge detection (wasPressed/wasReleased) correctly implemented for BLE buttons in `HalGPIO::update()`
- Key mapping table is safe -- no mapping to BTN_POWER (bit 6)
- NimBLE v2 API usage is correct and modern
- Settings persistence (JSON save/load) for BLE fields is complete and properly null-terminated
- `suspend()`/`resume()` idempotent guards are correct
- HID report parsing handles variable-length reports (up to 8 bytes)
- Thread-safe device list access via mutex

---

## Recommended Priority Actions

1. **[CRITICAL]** Make `tick()` non-blocking -- use async connect or short timeout with backoff
2. **[CRITICAL]** Add `bleManager.suspend()`/`resume()` to all WiFi-using activities, or move suspend into a shared "WiFi session" wrapper
3. **[CRITICAL]** Move auto-reconnect logic into `resume()` method
4. **[HIGH]** Add BLE init/deinit side effects to settings toggle
5. **[HIGH]** Implement BLE held-time tracking for long-press actions
6. **[MEDIUM]** Set `client = nullptr` in `deinit()`
7. **[MEDIUM]** Clear `discoveredDevices` in `deinit()`
8. **[LOW]** Set `scanning = true` before `scan->start()`

---

## Unresolved Questions

1. Does an emulated/simulator build target exist? If so, how does it exclude `src/ble/` and NimBLE dependencies?
2. Should Pair Remote menu item be hidden when `bleEnabled == 0`?
3. Is there a maximum number of NimBLE clients that can be created? (v2 defaults to `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1` per platformio.ini -- so only one client at a time, which matches the implementation.)
4. Should `BleRemotePairingActivity` auto-enable `bleEnabled` on successful pairing?
