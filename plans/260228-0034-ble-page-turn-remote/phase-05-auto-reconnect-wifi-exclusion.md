# Phase 5: Auto-Reconnect & WiFi Mutual Exclusion

## Context

- [Phase 2](./phase-02-ble-hid-client.md) — BLE connect/disconnect logic
- [Phase 3](./phase-03-settings-persistence.md) — `bleEnabled`, `bleBondedDeviceAddr` in settings
- [main.cpp](../../src/main.cpp) — boot flow, deep sleep, main loop
- [WifiSelectionActivity](../../src/activities/network/WifiSelectionActivity.cpp) — WiFi init pattern

## Overview

- **Priority:** P2
- **Status:** Pending
- **Description:** Auto-reconnect to bonded BLE remote on boot/wake. Implement WiFi/BLE mutual exclusion — auto-suspend BLE when WiFi starts, resume when WiFi stops. Integrate BLE lifecycle into main.cpp boot and sleep flow.

## Key Insights

- Deep sleep kills BLE radio. On wake, must re-init NimBLE + directed reconnect to bonded device.
- Directed connection (known address) is faster than general scan: ~0.5-2s vs 2-5s.
- WiFi and BLE share single 2.4GHz radio via time-division. WiFi dominates. Performance degrades.
- Practical solution: mutual exclusion. When WiFi activates (file transfer, OTA, KOReader sync), suspend BLE. Resume when WiFi deactivates.
- WiFi activities call `WiFi.begin()` / `WiFi.disconnect()`. Hook into these boundaries.
- BLE activity (receiving HID reports) should reset auto-sleep timer in main.cpp.
- `preventAutoSleep()` already exists on Activity — but BLE input is at HalGPIO level, so it already resets the timer via `gpio.wasAnyPressed()` check in main loop. No change needed.

## Requirements

### Functional
- On boot/wake: if `bleEnabled` and `bleBondedDeviceAddr` not empty → auto-reconnect
- Auto-reconnect uses directed connection (bonded address) for speed
- If reconnect fails after timeout: give up silently (user can manually re-pair)
- Before WiFi starts: call `bleManager.suspend()` → disconnect + deinit NimBLE
- After WiFi stops: call `bleManager.resume()` → re-init + auto-reconnect
- On deep sleep: call `bleManager.deinit()` → clean shutdown

### Non-Functional
- Auto-reconnect timeout: 5 seconds max
- Auto-reconnect runs in background (non-blocking to boot flow)
- No visible UI during auto-reconnect (silent background operation)
- BLE suspend/resume cycle < 3 seconds total

## Architecture

```
main.cpp setup()
  └── if (SETTINGS.bleEnabled && bleBondedAddr not empty)
        └── bleManager.init()
        └── bleManager.autoReconnect(bleBondedAddr)  // non-blocking

main.cpp loop()
  └── (no changes — HalGPIO already handles BLE state)

WiFi activities (file transfer, OTA, KOReader sync)
  └── onEnter():  bleManager.suspend()
  └── onExit():   bleManager.resume()

enterDeepSleep()
  └── bleManager.deinit()
```

## Related Code Files

### Modify
- `src/main.cpp` — add BLE init in setup(), deinit before sleep
- `src/ble/BleRemoteManager.h` — add `autoReconnect()`, `suspend()`, `resume()`
- `src/ble/BleRemoteManager.cpp` — implement lifecycle methods
- `src/activities/network/WifiSelectionActivity.cpp` — add suspend/resume calls
- `src/activities/settings/OtaUpdateActivity.cpp` — add suspend/resume calls

### Reference (read-only)
- `src/activities/network/CalibreConnectActivity.cpp` — another WiFi activity
- `src/activities/reader/KOReaderSyncActivity.cpp` — another WiFi activity

## Implementation Steps

1. **Add lifecycle methods to BleRemoteManager**

   ```cpp
   // Non-blocking: starts reconnect attempt in background
   void autoReconnect(const char* bondedAddr);

   // Suspend BLE (before WiFi). Disconnects + deinits NimBLE.
   void suspend();

   // Resume BLE (after WiFi). Re-inits NimBLE + auto-reconnects.
   void resume();
   ```

2. **Implement autoReconnect()**

   ```cpp
   void BleRemoteManager::autoReconnect(const char* bondedAddr) {
     if (!enabled || strlen(bondedAddr) == 0) return;

     NimBLEAddress addr(bondedAddr);
     // Use NimBLE's connect with known address — skips scan
     // Run on NimBLE task via xTimerCreate or direct call
     reconnectAddress = addr;
     reconnectPending = true;
   }
   ```

   In a periodic check (called from main loop or NimBLE task):
   ```cpp
   void BleRemoteManager::tick() {
     if (reconnectPending && !connected) {
       reconnectPending = false;
       connectToDevice(reconnectAddress);  // blocking but on NimBLE task
     }
   }
   ```

3. **Implement suspend() / resume()**

   ```cpp
   void BleRemoteManager::suspend() {
     if (!enabled) return;
     suspended = true;
     if (connected) {
       NimBLEDevice::getClientByPeerAddress(/* current addr */)->disconnect();
     }
     deinit();
     LOG_DBG("BLE", "Suspended for WiFi");
   }

   void BleRemoteManager::resume() {
     if (!suspended) return;
     suspended = false;
     init();
     autoReconnect(SETTINGS.bleBondedDeviceAddr);
     LOG_DBG("BLE", "Resumed after WiFi");
   }
   ```

4. **Integrate into main.cpp setup()**

   After `SETTINGS.loadFromFile()` and before `activityManager.goToBoot()`:
   ```cpp
   // Initialize BLE remote if enabled and bonded
   extern BleRemoteManager bleManager;
   if (SETTINGS.bleEnabled && strlen(SETTINGS.bleBondedDeviceAddr) > 0) {
     bleManager.init();
     bleManager.autoReconnect(SETTINGS.bleBondedDeviceAddr);
   }
   ```

5. **Integrate into main.cpp enterDeepSleep()**

   Before `display.deepSleep()`:
   ```cpp
   bleManager.deinit();
   ```

6. **Add global BleRemoteManager instance in main.cpp**

   ```cpp
   #include "ble/BleRemoteManager.h"
   BleRemoteManager bleManager(gpio);
   ```

7. **Add suspend/resume to WiFi activities**

   In `WifiSelectionActivity::onEnter()`:
   ```cpp
   extern BleRemoteManager bleManager;
   bleManager.suspend();
   ```

   In `WifiSelectionActivity::onExit()`:
   ```cpp
   extern BleRemoteManager bleManager;
   bleManager.resume();
   ```

   Same pattern for `OtaUpdateActivity`, `CalibreConnectActivity`, `KOReaderSyncActivity`.

8. **Add tick() call to main loop (optional)**

   If reconnect needs periodic polling:
   ```cpp
   void loop() {
     // ...existing...
     if (SETTINGS.bleEnabled) {
       bleManager.tick();
     }
     // ...existing...
   }
   ```

9. **Compile and test full lifecycle**
   - Boot with BLE enabled + bonded device → auto-connects
   - Enter WiFi activity → BLE suspends
   - Exit WiFi activity → BLE resumes + reconnects
   - Deep sleep → BLE deinits cleanly
   - Wake → BLE re-inits + auto-reconnects

## Todo List

- [ ] Add `autoReconnect()`, `suspend()`, `resume()`, `tick()` to BleRemoteManager
- [ ] Implement directed reconnect using bonded address (skip scan)
- [ ] Implement suspend: disconnect + deinit NimBLE
- [ ] Implement resume: init + auto-reconnect
- [ ] Add global `BleRemoteManager bleManager(gpio)` to main.cpp
- [ ] Add BLE init + auto-reconnect to setup() (after settings load)
- [ ] Add `bleManager.deinit()` to enterDeepSleep()
- [ ] Add `bleManager.suspend()` to WiFi activity onEnter() calls
- [ ] Add `bleManager.resume()` to WiFi activity onExit() calls
- [ ] Apply suspend/resume to: WifiSelectionActivity, OtaUpdateActivity, CalibreConnectActivity, KOReaderSyncActivity
- [ ] Add `bleManager.tick()` to main loop (if polling needed)
- [ ] Test full lifecycle: boot → read → WiFi → resume → sleep → wake
- [ ] Compile and verify

## Success Criteria

- BLE auto-reconnects to paired remote within 3s of boot/wake
- WiFi activities work normally (BLE suspended during WiFi)
- BLE resumes and reconnects after WiFi activity exits
- Deep sleep cleans up BLE without errors
- No RAM leaks across suspend/resume cycles
- Failed auto-reconnect doesn't block boot or cause errors

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Auto-reconnect blocks boot | Use non-blocking approach. Boot proceeds, reconnect in background. |
| Suspend during active HID report | Clear bleState in disconnect callback. Clean state guaranteed. |
| Resume fails after WiFi | Retry once. If fails, user can manually re-pair from settings. |
| Multiple WiFi activities nested | Track suspend count. Only resume on last exit (refcount). |
| NimBLE deinit/init cycle leaks memory | NimBLE handles cleanup. Verify with `ESP.getFreeHeap()` before/after. |

## Security Considerations

- NimBLE bonding uses encrypted keys stored in NVS (non-volatile storage)
- Just Works pairing (no PIN) — standard for page-turn remotes
- Bonded address stored in plain text in settings.json (acceptable for personal fork)
- No sensitive data transmitted over BLE (only button keycodes)

## Next Steps

After all phases complete:
- Test with 2-3 real BLE page-turn remotes
- Measure RAM usage: `ESP.getFreeHeap()` with BLE active vs inactive
- Measure battery impact: compare reading session duration with/without BLE
- Consider: expose key mapping in settings UI (future enhancement, not V1)
