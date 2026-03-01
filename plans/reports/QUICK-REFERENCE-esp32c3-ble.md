# ESP32-C3 BLE: Quick Reference Checklist

## Your 12 Questions - Rapid Answers

| # | Question | Answer | Confidence | Notes |
|---|----------|--------|-----------|-------|
| 1 | BLE profiles (HID, GATT)? | ✅ HID, GATT, Battery, DIS, Mesh | HIGH | Classic BT = NO |
| 2 | RAM for BLE? | 60-100KB (Bluedroid) or 40-60KB (NimBLE) | HIGH | Use NimBLE (~50% less) |
| 3 | WiFi + BLE simultaneous? | ⚠️ Time-shared (not true parallel) | HIGH | WiFi dominates; works but degraded |
| 4 | BLE range? | 30-40m real-world; 200m theoretical | HIGH | Depends on TX power, PHY mode, antenna |
| 5 | BLE keyboard/HID? | ✅ YES - production-ready | HIGH | Full GATT-based HID support |
| 6 | BLE MIDI? | ✅ YES - via libraries | HIGH | ESP32-BLE-MIDI or Arduino-BLE-MIDI |
| 7 | ANCS (iPhone notifications)? | ✅ YES - custom implementation | MEDIUM | Requires GATT client + pairing |
| 8 | BLE beacon/iBeacon? | ✅ YES - simple, low overhead | HIGH | Best for RAM-constrained projects |
| 9 | BLE serial bridge? | ✅ YES - standard implementation | HIGH | Many libraries available |
| 10 | Arduino libraries? | **NimBLE-Arduino** (recommended) | HIGH | 50% less RAM than Bluedroid |
| 11 | Power consumption? | BLE: 8-15mA (advertise) / 80mA (connected) | HIGH | WiFi: 95-240mA; Deep sleep: 40µA |
| 12 | Deep sleep + BLE? | ❌ NO - incompatible (radio off) | HIGH | Use Light Sleep (~1-2mA) as alternative |

---

## RAM Budget Reality

**Available:** ~384 KB total SRAM
```
- OS/FreeRTOS: ~50 KB
- BLE Stack (NimBLE): ~50 KB
- WiFi Stack (if enabled): ~120 KB
- Application: ~150-180 KB (tight!)
```

**Implication:** Single-purpose BLE projects work fine. Multi-feature + WiFi = risky without careful memory management.

---

## Best Library Choice: NimBLE-Arduino

**Why?**
- 50% less RAM than Bluedroid
- 50% less flash
- Faster performance on ESP32-C3
- Full feature parity
- Active development

**Install:** Arduino IDE → Sketch → Include Library → Manage Libraries → Search "NimBLE-Arduino"

**GitHub:** [h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)

---

## Critical Constraints

1. **~65KB BLE overhead minimum** (non-negotiable)
2. **WiFi + BLE = time-shared** (not truly parallel)
3. **30-40m practical range** (not 200m)
4. **No deep sleep + BLE** (radio power off)
5. **Single RF module** (2.4 GHz shared)

---

## Projects by Viability

### ✅ DEFINITELY VIABLE
- Beacon broadcaster (iBeacon)
- BLE keyboard/mouse (HID)
- MIDI controller
- Serial bridge (UART)
- Sensor peripheral

### ⚠️ POSSIBLE (Tight)
- ANCS notifications receiver
- WiFi provisioning + BLE
- Multi-service GATT (2-3 services)
- WiFi + BLE low-bandwidth combo

### ❌ NOT VIABLE
- Deep sleep + always-on BLE
- High-throughput BLE (multi-Mbps)
- Complex BLE Mesh
- Everything-on-one-chip (WiFi + BLE + Bluetooth Classic)

---

## Power Budgets (Approximate)

| Scenario | Power | Battery Life (500mAh) |
|----------|-------|----------------------|
| Beacon only (100ms interval) | 1 mA | 20+ days |
| BLE connected (low traffic) | 2-3 mA | 7-10 days |
| BLE connected (continuous) | 80 mA | 6 hours |
| WiFi RX active | 100 mA | 5 hours |
| WiFi TX 9dBm | 200 mA | 2.5 hours |
| WiFi + BLE together | 20-30 mA | 18-25 hours |
| Deep sleep only | 40 µA | 1+ year |

---

## Common Gotchas

1. **"Why is my BLE range only 10m?"** → Check TX power setting (default is low). Set to +9dBm, check antenna.

2. **"BLE works until I enable WiFi"** → Normal. They share RF. WiFi takes priority.

3. **"Out of memory error with BLE + WiFi"** → Use NimBLE, disable Bluedroid, careful heap management.

4. **"BLE doesn't wake from deep sleep"** → Deep sleep powers off radio. Use RTC timer wake + advertise window.

5. **"HID keyboard not working"** → Verify HID descriptor, check MTU, test with phone first (usually works).

---

## Recommended Starting Libraries

| Use Case | Library | Rationale |
|----------|---------|-----------|
| Beacon | NimBLE-Arduino + native ads | Minimal overhead |
| Keyboard/HID | NimBLE-Arduino | Less RAM than Bluedroid |
| MIDI | ESP32-BLE-MIDI (v0.3.0+) | Purpose-built, uses NimBLE |
| Serial Bridge | BleSerial | Simple, works |
| ANCS | S-March/esp32_ANCS | Proven implementation |
| General GATT | NimBLE-Arduino | Best efficiency |

---

## Testing Checklist

Before production:
- [ ] Test actual BLE range in your environment
- [ ] Measure heap usage with `heap_caps_get_free_size()`
- [ ] Test WiFi + BLE together (if needed)
- [ ] Profile power consumption with multimeter
- [ ] Verify against real devices (phones, computers)
- [ ] Test at min/max temperatures

---

## Resources

**Official Documentation:**
- [Espressif ESP32-C3 BLE Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/ble/overview.html)
- [NimBLE-Arduino Docs](https://h2zero.github.io/NimBLE-Arduino/)

**Key GitHub Projects:**
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
- [ESP32-BLE-MIDI](https://github.com/max22-/ESP32-BLE-MIDI)
- [esp32_ANCS](https://github.com/S-March/esp32_ANCS)
- [ESP32_BleSerial](https://github.com/avinabmalla/ESP32_BleSerial)

---

**Created:** 2026-02-25
**Full Report:** See `researcher-260225-esp32c3-ble-capabilities.md`
