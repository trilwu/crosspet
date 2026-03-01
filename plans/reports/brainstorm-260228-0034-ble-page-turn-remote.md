# BLE Page-Turn Remote — Brainstorm Summary

**Date:** 2026-02-28
**Target:** Personal fork (not upstream — SCOPE.md conflict)
**Hardware:** ESP32-C3 (single-core RISC-V, ~384KB SRAM)

---

## Problem Statement

Physical buttons on the Xteink X4 require holding the device. A BLE page-turn remote enables hands-free reading — useful for cooking, music stands, desk reading, etc.

## Agreed Approach

### Integration Point: HalGPIO Level

BLE events injected directly into `HalGPIO.update()` state bitmask. All downstream code (MappedInputManager, activities, button remapping, side layout swap, auto-sleep detection) works unchanged.

```
BLE HID Remote → NimBLE Central → HID report parser → HalGPIO state bits
                                                        ↓
                              MappedInputManager reads as normal
                                                        ↓
                              Activities see button events transparently
```

### BLE Mode: Central (Client)

Reader acts as BLE Central connecting to external HID keyboard peripherals (standard page-turn remotes from Amazon/AliExpress).

### Library: NimBLE-Arduino

- 50% less RAM than Bluedroid (~40-60KB vs 60-100KB)
- Full central mode support
- PlatformIO: `h2zero/NimBLE-Arduino@latest`

### Pairing UX: Auto + Manual

- Auto-reconnect to last bonded device on wake
- Settings menu option to scan/pair new device
- Store bonded device address in settings (SD card)

---

## Key Technical Details

### HID Report Key Mapping

Standard page-turn remotes typically send:

| Remote Key | HID Key Code | Map To |
|-----------|-------------|--------|
| Next page | Right Arrow (0x4F) / Page Down (0x4E) / Space (0x2C) / Enter (0x28) | `BTN_RIGHT` or `BTN_DOWN` |
| Prev page | Left Arrow (0x50) / Page Up (0x4B) / Backspace (0x2A) | `BTN_LEFT` or `BTN_UP` |
| Volume Up | Vol+ (0xE9) — camera shutter remotes | configurable |
| Volume Down | Vol- (0xEA) — camera shutter remotes | configurable |

Need configurable key-to-button mapping stored in settings.

### RAM Budget

```
Total SRAM:           ~384 KB
OS/FreeRTOS:          ~50 KB
NimBLE Central:       ~50-60 KB
Reader Activity:      ~150 KB (EPUB parsing, section cache)
Remaining heap:       ~120-130 KB
```

**Verdict:** Tight but viable. WiFi must be OFF during BLE reading sessions. Mutual exclusion: WiFi OR BLE, not both.

### Connection Lifecycle

1. **Boot/Wake** → Check if BLE enabled in settings
2. **If enabled** → Init NimBLE, attempt directed connection to bonded device (fast, ~0.5-1s)
3. **If no bond** → Skip until user pairs via settings
4. **During reading** → BLE connected, receiving HID reports, mapped to button events
5. **Deep sleep** → BLE radio powers off (unavoidable)
6. **Wake** → Re-init BLE, auto-reconnect (~1-3s delay)

### Deep Sleep Reconnection Latency

- Directed reconnection (to known device): **0.5-2 sec**
- General scan + connect: **2-5 sec**
- Mitigation: Store bonded device address, use directed connection on wake
- User perceives: slight delay before remote works after waking device

### Single-Core Contention

- ESP32-C3 = 1 RISC-V core
- NimBLE runs as FreeRTOS task alongside main loop
- Main loop has 10-50ms delay between iterations → gives BLE stack CPU time
- E-ink refresh is blocking (~200-1000ms) → BLE events buffered during refresh
- **No expected issues** — NimBLE designed for single-core ESP32-C3

---

## Implementation Components

### 1. NimBLE BLE Manager (`src/ble/BleRemoteManager.h/.cpp`)
- Init/deinit NimBLE stack
- Scan for HID devices
- Connect/disconnect
- Handle bonding (store in settings)
- Parse HID reports → translate to button indices

### 2. HalGPIO Integration (`lib/hal/HalGPIO.cpp`)
- Add method: `void injectBleButtonState(uint8_t buttons)`
- In `update()`: merge physical ADC state with BLE injected state via OR
- BLE events appear identical to physical button presses

### 3. Settings Extension (`src/CrossPointSettings.h/.cpp`)
- `bool bleEnabled` — master toggle
- `String bleBondedAddress` — MAC of paired remote
- `uint8_t bleKeyMap[4]` — configurable HID key → button mapping

### 4. Pairing Activity (`src/activities/settings/BleRemotePairingActivity.h/.cpp`)
- Scan for BLE HID devices
- Show discovered devices list
- Select to pair/bond
- Show connection status

### 5. Settings Menu Entry
- Add "Bluetooth Remote" to settings list
- Submenu: Enable/Disable, Pair New, Key Mapping

---

## Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| RAM overflow with BLE + reader | HIGH | Empirical testing with `heap_caps_get_free_size()`. Disable WiFi during BLE. |
| HID report format varies by remote | MEDIUM | Support common key codes. Configurable mapping. Test with 2-3 popular remotes. |
| Reconnection delay after sleep | LOW | Directed connection to bonded device. Acceptable 1-2s delay. |
| BLE + WiFi conflict | MEDIUM | Mutual exclusion: auto-disable BLE when WiFi starts (book upload/OTA). Re-enable after. |
| Single-core latency | LOW | NimBLE designed for ESP32-C3. Main loop delay provides CPU time. |
| Flash size increase | LOW | NimBLE adds ~280KB flash. 16MB flash = plenty of room. |

---

## What NOT to Build (YAGNI)

- Multi-remote support (one remote at a time is enough)
- BLE peripheral mode (not needed for this use case)
- BLE MIDI, beacon, serial bridge (irrelevant)
- Complex HID descriptor parser (target known key codes, not arbitrary HID devices)
- WiFi+BLE simultaneous operation (mutual exclusion is simpler and safer)

---

## Success Criteria

- [ ] Pair with standard BLE page-turn remote from Settings
- [ ] Auto-reconnect to paired remote after wake from sleep
- [ ] Page forward/back via remote in EPUB reader
- [ ] Remote works in all reader types (EPUB, TXT, XTC)
- [ ] No RAM crash during normal reading session
- [ ] Battery life impact <15% compared to BLE-off reading
- [ ] WiFi features (upload, OTA) still work when BLE is disabled/paused

---

## Unresolved Questions

1. **Exact heap usage during EPUB reading** — need empirical measurement before confirming RAM viability. Run `ESP.getFreeHeap()` during chapter load.
2. **Which specific remotes to test with** — need to identify 2-3 popular models and their exact HID key codes.
3. **Bond storage format** — NimBLE may handle this internally via its bonding database, or may need manual storage on SD card.
4. **Auto-sleep interaction** — should BLE activity (receiving HID reports) reset the auto-sleep timer? Probably yes, but if remote has a stuck key, it could prevent sleep.

---

## Next Step

Create detailed implementation plan with phased approach:
1. Phase 1: NimBLE integration + basic scan/connect (proof of concept)
2. Phase 2: HID report parsing + HalGPIO injection
3. Phase 3: Settings UI + pairing flow
4. Phase 4: Auto-reconnect + polish
