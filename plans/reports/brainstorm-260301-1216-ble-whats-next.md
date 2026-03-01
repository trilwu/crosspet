# Brainstorm: BLE Page-Turn Remote — What's Next

**Date:** 2026-03-01
**Status:** Concluded → Moving to implementation

## Problem Statement
BLE page-turn remote feature complete (5 phases), but not yet hardware-tested or committed. Need to determine shipping path.

## Decision: Ship BLE Feature (Personal Fork)
- **Target:** Personal fork only, not upstream PR
- **BLE remote type:** Generic laptop presentation clicker (BLE HID)

## Code Review Findings
- **Overall:** Ship-ready with 2 critical fixes
- **Full report:** `plans/reports/code-review-260301-1216-ble-remote-implementation.md`

### Critical (fix before flash)
1. **NimBLEAddress type hardcoded to 0** → auto-reconnect fails for random-address BLE devices. Store address type in settings.
2. **Only keyboard HID reports parsed** → Consumer Control reports (common in presentation clickers) ignored. Add parsing.

### Warnings (fix later)
3. `connectToDevice()` blocks main loop ~5s (acceptable)
4. BleRemotePairingActivity.cpp 359 lines (split later)
5. `bleEnabled` not bounds-checked in JSON load (minor)

## Agreed Shipping Path
1. Fix critical #1 and #2
2. Compile for ESP32-C3
3. Flash and test with real BLE remote
4. Polish based on findings
5. Clean commit

## Architecture Positives
- GPIO injection at HalGPIO level — elegant, zero downstream changes
- WiFi/BLE mutual exclusion thorough
- NimBLE flags optimized for RAM
- Settings persistence follows existing patterns
