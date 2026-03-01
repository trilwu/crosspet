# ESP32-C3 Bluetooth Low Energy (BLE) Capabilities Research Report

**Date:** 2026-02-25
**Focus:** Practical BLE 5.0 capabilities for creative device use cases
**RAM Constraint:** ~380KB total (minus existing firmware)

---

## EXECUTIVE SUMMARY

ESP32-C3 is **production-ready for most BLE use cases** but RAM-constrained. Single-purpose BLE applications work well. Simultaneous WiFi+BLE is possible but degraded. Deep-sleep BLE advertising is **NOT supported** (radio powers off). NimBLE library is strongly preferred over Bluedroid for RAM efficiency.

**CRITICAL CONSTRAINT:** ~64-100KB RAM consumed by BLE stack alone = limiting factor for complex projects.

---

## 1. BLE PROFILES & SERVICES SUPPORT

### Supported Profiles
- **GATT/SMP/GAP** - Full support (core BLE)
- **HID over GATT** - YES (keyboard, mouse, game controller)
- **Battery Service** - YES
- **Device Information Service (DIS)** - YES
- **Proprietary GATT Services** - YES (custom profiles)
- **Mesh** - YES (Bluetooth LE SIG Mesh)
- **Classic Bluetooth** - NO (LE only, no BR/EDR)

### Profiles NOT Natively Supported
- ANCS, MIDI, iBeacon = require custom implementation via GATT or advertisement data

---

## 2. RAM CONSUMPTION: THE HARD CONSTRAINT

### Datasheet Specifications
| Component | Memory |
|-----------|--------|
| Total on-chip SRAM | **400 KB** |
| Cache (pre-allocated) | 16 KB (fixed) |
| Usable SRAM | **~384 KB** |
| RTC FAST Memory | 8 KB (survives deep sleep) |

### BLE Stack RAM Usage
- **Bluedroid** (default ESP-IDF): 60-100KB RAM
- **NimBLE** (recommended): 40-60KB RAM (50% smaller)
- Typical allocation: **~64-80KB for BLE stack**
- Leaves: **~300-340KB for application code + data**

### Real-World Impact
```
Total: 384 KB
- BLE Stack: 65 KB
- OS/FreeRTOS: ~50 KB
- WiFi Stack (if enabled): ~120 KB
- Application heap: ~150 KB
```

**Verdict:** Single BLE peripheral (keyboard/beacon/sensor) = comfortable. BLE central + WiFi = tight. Complex multi-service BLE + WiFi = risky without careful memory management.

---

## 3. WiFi + BLE SIMULTANEOUS OPERATION

### Hardware Reality
- **Single shared 2.4 GHz RF module** (cannot truly run both simultaneously)
- Uses **time-division multiplexing** arbitration
- NOT true parallel operation

### Coexistence Behavior
**Mechanism:**
- WiFi and BLE take turns accessing RF based on priority
- WiFi gets priority (higher arbitration)
- BLE gets interrupted during WiFi TX/RX windows

**WiFi Status Impact:**
| WiFi State | BLE Impact |
|-----------|-----------|
| IDLE | BLE has full RF access |
| SCANNING | Significant interruptions |
| CONNECTING | Constant arbitration |
| CONNECTED | Periodic interruptions (~100ms duty cycles) |

### Practical Implications
- **WiFi active (connected)** = BLE latency increases 10-50x, throughput drops
- **WiFi idle** = BLE works normally
- **WiFi scanning** = BLE nearly non-functional
- **Both dormant** = minimal overhead

### Configuration
Must enable `CONFIG_ESP_COEX_SW_COEXIST_ENABLE` in menuconfig. Coexistence status switches **automatically** without API calls.

**Verdict:** Simultaneous operation works but WiFi dominates RF resources. Good for low-bandwidth BLE (e.g., periodic status) + WiFi primary. Bad for real-time BLE or heavy traffic.

---

## 4. BLE RANGE

### Practical Distances
- **Open air, optimal conditions:** 200m theoretical (BLE 5.0 coded PHY)
- **Real-world indoor:** 30-40m typical
- **With obstructions:** 10-20m
- **Range test results from users:** 40-80m reported for various ESP32-C3 boards

### Transmit Power Levels
8 configurable levels: `-12 dBm to +9 dBm` (3 dBm steps)
```
Level 0: -12 dBm → ~10m range
Level 4: 0 dBm   → ~20m range
Level 7: +9 dBm  → ~40-50m range
```

### PHY Modes (range vs speed tradeoff)
| PHY | Data Rate | Range | Notes |
|-----|-----------|-------|-------|
| 1M | 1 Mbps | Standard | Default, best compatibility |
| 2M | 2 Mbps | Reduced | Faster but shorter range |
| Coded | 125/500 Kbps | Extended | BLE 5.0 long-range |

**Verdict:** 30-40m practical range is realistic. Extend via antenna design or coded PHY. 200m requires perfect conditions and coded mode.

---

## 5. BLE HID DEVICE SUPPORT (Keyboard/Mouse)

### Status: FULLY SUPPORTED

**YES - ESP32-C3 can act as BLE HID keyboard, mouse, gamepad**

### Implementation Path
1. Configure HID profile (GATT-based, not Classic BT)
2. Implement HID descriptor + input/output characteristics
3. Send HID reports via GATT notifications

### Available Libraries
- **Arduino ESP32 BLE Library** (Bluedroid-based)
  - GitHub: `espressif/arduino-esp32`
  - Example: `BLE_HID_Keyboard`
- **NimBLE-Arduino** (lighter weight)
  - GitHub: `h2zero/NimBLE-Arduino`
  - Recommended for RAM-constrained projects
  - ~40KB RAM vs 65KB for Bluedroid
- **Community Projects:** `asterics/esp32_mouse_keyboard` (advanced HID example)

### Practical Example Use Cases
- Wireless keyboard input device
- Gaming controller (analog/digital inputs)
- Mouse replacement
- Custom input peripheral

**Verdict:** Production-ready. Use NimBLE for minimal RAM footprint.

---

## 6. BLE MIDI SUPPORT

### Status: FULLY SUPPORTED (via libraries)

**YES - ESP32-C3 can send/receive BLE MIDI**

### Implementation Path
1. Use **ESP32-BLE-MIDI library** (Arduino)
2. Or **Arduino-BLE-MIDI library** (cross-platform)
3. Implements BLE MIDI 1.0 spec
4. Uses custom GATT characteristics for MIDI data

### Available Libraries
- **ESP32-BLE-MIDI**
  - GitHub: `max22-/ESP32-BLE-MIDI`
  - Latest uses NimBLE backend (v0.3.0+)
  - Includes running status, timestamp support
  - Active development

- **Arduino-BLE-MIDI**
  - GitHub: `lathoub/Arduino-BLE-MIDI`
  - Cross-platform (works with ESP32)
  - Lighter weight option

- **Control Surface Framework**
  - GitHub: `tttapa/Control-Surface`
  - Full MIDI framework with BLE support
  - More overhead but feature-rich

### Practical Use Cases
- Wireless MIDI controller
- Bluetooth MIDI footswitch
- BLE synthesizer interface
- DAW controller via Bluetooth

**Verdict:** Production-ready. NimBLE backend recommended for space.

---

## 7. APPLE NOTIFICATION CENTER SERVICE (ANCS)

### Status: SUPPORTED (via custom implementation)

**YES - ESP32-C3 can receive iPhone notifications via ANCS**

### Technical Details
**ANCS Characteristics:**
- Notification Source (mandatory) - receives notification events
- Control Point (optional) - dismiss/action notifications
- Data Source (optional) - fetch notification details

### Implementation Requirements
1. Scan for ANCS service (UUID `7905F431-B5CE-4E99-A40F-4B1E122D00D0`)
2. Discover characteristics
3. Subscribe to Notification Source notifications
4. Parse notification events (UUID, event flags, attributes)

### Available Libraries/Projects
- **S-March/esp32_ANCS** - GitHub ANCS implementation
- **takeru/Arduino_ESP32_ANCS** - Arduino-compatible example
- **asfahanieyad/ESP32-iPhone-Notifications** - Complete working example

### Practical Constraints
- **Requires iPhone pairing** (must be in Bluetooth settings)
- **RAM overhead:** Additional ~10-20KB for GATT client + parser
- **Battery drain:** Continuous BLE connection required (moderate power draw)

**Verdict:** Achievable. Good fit for smartwatch/notification display use case. Not suitable for ultra-low-power applications.

---

## 8. BLE BEACON (iBeacon) CAPABILITIES

### Status: FULLY SUPPORTED

**YES - ESP32-C3 can broadcast as iBeacon or generic BLE beacon**

### Implementation Path
1. Configure BLE advertisement with iBeacon manufacturer data
2. Set UUID, major, minor identifiers
3. Set transmit power (RSSI @ 1m calibration)
4. Start broadcasting

### iBeacon Parameters
```
- UUID: 16-byte identifier (namespace)
- Major: 2 bytes (grouping within UUID)
- Minor: 2 bytes (specific beacon)
- TX Power: -12 to +9 dBm
  (must match 1m RSSI for ranging accuracy)
```

### Available Libraries
- **ESPHome esp32_ble_beacon component**
  - Simple configuration (YAML)
  - Minimal code
- **Native Arduino (via BLE library)**
  - Full control
  - ~20 lines of code

### Practical Use Cases
- Location detection (proximity marketing)
- Indoor navigation
- Inventory/asset tracking
- Zone-based automation

### RAM Impact
- Beacon advertising: **minimal** (~10KB)
- Compared to HID/MIDI services (50KB+)
- **Best choice for RAM-constrained projects**

### Power Consumption
- Advertising only (no connection): **8-15 mA**
- Interval configurable: 100-10240ms
- Longer intervals = lower average power

**Verdict:** Excellent use case. High compatibility, low overhead.

---

## 9. BLE SERIAL BRIDGE (UART Bridge)

### Status: FULLY SUPPORTED

**YES - ESP32-C3 can act as wireless UART bridge (replaces HC-05)**

### Implementation Path
1. Create GATT service with TX/RX characteristics
2. Map UART input → BLE TX characteristic
3. Map BLE RX characteristic → UART output
4. Handle fragmentation (MTU negotiation)

### Available Libraries/Projects
- **BleSerial** - Simple Arduino library
  - GitHub: `avinabmalla/ESP32_BleSerial`
  - Easy integration

- **esp32_serial_ble_bridge** - Standalone firmware
  - GitHub: `riozebratubo/esp32_serial_ble_bridge`
  - Direct UART-to-BLE passthrough

- **esp32-bluetooth-bridge** - HC-05 replacement
  - GitHub: `coddingtonbear/esp32-bluetooth-bridge`
  - Drop-in Classic BT alternative

- **esp32-ble-uart-mx** - Dual-role bridge
  - GitHub: `olegv142/esp32-ble-uart-mx`
  - Acts as client/server simultaneously
  - RFID reader integration example

### MTU Configuration
- Default: 20 bytes per packet
- Negotiable: up to 512+ bytes
- Larger MTU = faster but fragmentation overhead

### Practical Use Cases
- Wireless serial communication
- Sensor data streaming
- Debug console over BLE
- Equipment control interface

**Verdict:** Production-ready. Simpler than HID, standard pattern in ecosystem.

---

## 10. ARDUINO/PLATFORMIO LIBRARIES COMPARISON

### Library Landscape

| Library | Backend | Flash | RAM | Speed | Notes |
|---------|---------|-------|-----|-------|-------|
| **NimBLE-Arduino** | Apache NimBLE | 280KB | 40-60KB | Faster | **RECOMMENDED** - 50% less RAM |
| **Arduino ESP32 BLE** | Bluedroid | 380KB | 60-100KB | Standard | Default, more features |
| **ESP32_BLE_Arduino** | Bluedroid | 400KB | 65KB+ | Standard | Legacy, nkolban's library |
| **BleSerial** | Arduino BLE | Minimal | 30KB | Slower | Simple serial only |
| **ESP32-BLE-MIDI** | NimBLE (v0.3.0+) | 320KB | 50KB+ | Good | MIDI-specific |

### Detailed Comparison

#### **NimBLE-Arduino** (STRONGEST FOR ESP32-C3)
**Pros:**
- 50% less RAM (40-60KB vs 60-100KB)
- 50% less flash
- Faster BLE operations
- Full feature parity with Bluedroid
- Excellent documentation

**Cons:**
- Slightly different API
- Less beginner examples

**Use When:** RAM-constrained, BLE only, need efficiency

**GitHub:** [h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)

---

#### **Arduino ESP32 BLE Library** (Standard Default)
**Pros:**
- Included in arduino-esp32 core
- Extensive examples
- Large community
- Well-documented

**Cons:**
- Heavier RAM usage
- Slower on ESP32-C3
- Bluedroid more complex

**Use When:** Learning, need examples, have extra RAM

---

#### **ESP32-BLE-MIDI** (MIDI Projects)
**Pros:**
- MIDI-specific (abstracts away GATT)
- Now uses NimBLE backend
- Active maintenance

**Cons:**
- MIDI-only (can't do other GATT)
- Limited flexibility

**Use When:** MIDI-specific projects only

**GitHub:** [max22-/ESP32-BLE-MIDI](https://github.com/max22-/ESP32-BLE-MIDI)

---

### Installation

**Arduino IDE (all libraries):**
```
Sketch → Include Library → Manage Libraries
Search: "NimBLE-Arduino"
Click Install
```

**PlatformIO:**
```ini
lib_deps =
    h2zero/NimBLE-Arduino@latest
    ; or for Bluedroid:
    ; arduino-esp32
```

**Verdict:** Default to **NimBLE-Arduino** for ESP32-C3. ~40% more headroom than alternatives.

---

## 11. POWER CONSUMPTION COMPARISON

### Current Draw by Mode

| Mode | Current | Notes |
|------|---------|-------|
| **Deep Sleep** | 40 µA | Radio off, RTC timer only |
| **Light Sleep (idle)** | 1-2 mA | Radio off, can wake on GPIO/timer |
| **Idle (no BLE)** | 5-10 mA | CPU active, no wireless |
| **BLE Advertising** | 8-15 mA | Depends on interval (100-10240ms) |
| **BLE Connected** | 80 mA | Continuous connection + notifications |
| **WiFi RX** | 95-100 mA | Receiving |
| **WiFi TX (0 dBm)** | 130 mA | Transmitting low power |
| **WiFi TX (9 dBm)** | 180-240 mA | Transmitting high power |

### Power Budget Examples

**Use Case 1: Beacon (advertising only)**
```
Base: 10 mA @ 10% duty (100ms interval)
Average: 1 mA
Battery Life (500 mAh): 500 hours = 20+ days
```

**Use Case 2: BLE Connected Peripheral**
```
Connection idle: 2 mA @ 80% duty
+ Notification: 80 mA @ 1% duty
Average: 2.4 mA
Battery Life (500 mAh): 200 hours = 8-9 days
```

**Use Case 3: WiFi + BLE Together**
```
WiFi active (RX): 100 mA @ 20% duty
+ BLE idle: 10 mA @ 80% duty
Average: 28 mA
Battery Life (500 mAh): 18 hours
```

### Optimization Strategies
1. **Increase BLE advertising interval** (200-500ms) = 30-40% power reduction
2. **Use Light Sleep between events** = 5x improvement over idle
3. **Lower TX power** (-6 dBm vs 9 dBm) = 2x power reduction in range-acceptable cases
4. **Disable WiFi when not needed** = 100+ mA savings
5. **Use RTC fast memory** (8KB) for critical state = avoid SRAM drain

**Verdict:** BLE advertising = reasonable battery life (~20 days). Connected = acceptable (~8 days). WiFi+BLE = drain (~18 hours).

---

## 12. DEEP SLEEP + BLE ADVERTISING

### Critical Finding: NOT SUPPORTED

**Deep Sleep (power off radio) + BLE advertising = INCOMPATIBLE**

### Why
- Deep sleep powers off all wireless peripherals (radio, WiFi, BLE)
- BLE advertising requires active radio transmission
- Cannot wake from deep sleep on BLE events
- **BLE wakeup only works in Light Sleep mode** (higher power)

### Supported Deep Sleep Wakeup Sources
- Timer (RTC) - wake after N seconds
- GPIO external interrupt - button press, etc.
- WiFi MAC wakeup (edge case, rarely used)

### Light Sleep Alternative
If BLE wakeup needed:
- **Light Sleep** instead of Deep Sleep
- Radio remains powered, low-power idle state
- Can wake on BLE connection/advertisement
- Power: ~1-2 mA (vs 40 µA deep sleep)

### Practical Workaround
**For maximum battery + BLE connectivity:**
1. Use RTC timer to wake every 30-60 sec
2. Initialize BLE
3. Advertise for 10 seconds (window for client to connect)
4. Return to deep sleep
5. If client connects, wake immediately to serve

This gives **pseudo-continuous BLE availability** while deep sleeping 90% of time.

### Code Pattern
```cpp
// Example: advertise 10s every 60s
void setup() {
  esp_sleep_enable_timer_wakeup(60 * 1e6); // 60 sec
}

void loop() {
  // Initialize BLE
  BLEDevice::init("MyDevice");
  // ... setup

  // Advertise for 10 seconds
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();
  delay(10000);
  pAdvertising->stop();

  // Clean up and sleep
  BLEDevice::deinit(false);
  esp_deep_sleep_start();
}
```

**Verdict:** Deep sleep + continuous BLE = impossible. Use periodic wake + advertise window for pseudo-continuous availability.

---

## CREATIVE USE CASE ASSESSMENT

### HIGH CONFIDENCE (Production-Ready)
✅ **BLE Keyboard/Mouse** - Plenty of examples, HID fully supported
✅ **BLE Beacon** - Simple, proven, low overhead
✅ **MIDI Controller** - Active libraries, works well
✅ **Sensor Reader** (peripheral) - Standard GATT
✅ **UART Bridge** - Multiple solid implementations
✅ **HID Device** - Full support

### MEDIUM CONFIDENCE (Workable with caveats)
⚠️ **ANCS Notifications** - Requires custom GATT client, connection always-on
⚠️ **WiFi + BLE Bridge** - Works but RF time-sharing degrades both
⚠️ **Multi-Service GATT** - Tight on RAM, requires NimBLE
⚠️ **Simultaneous WiFi/BLE** - Possible but WiFi dominates

### LOW CONFIDENCE (Challenging)
❌ **Deep Sleep + BLE** - Fundamentally incompatible
❌ **High-throughput BLE** (>1 Mbps) - RAM/processing limits
❌ **Complex BLE Mesh** - RAM overhead significant
❌ **Bluedroid + WiFi + other comms** - Exceeds available RAM

---

## MEMORY-CONSTRAINED PROJECT RECOMMENDATIONS

### If You Have ~300KB Heap Available

**Scenario A: BLE Beacon Only**
```
NimBLE: 50KB
Custom code: 50KB
Heap free: 200KB (safe margin)
✅ VIABLE
```

**Scenario B: BLE Keyboard + WiFi Provisioning**
```
NimBLE: 50KB
HID implementation: 30KB
WiFi stack: 120KB
Heap needed: 100KB
✅ VIABLE (tight but works)
```

**Scenario C: ANCS + GATT Services**
```
NimBLE: 50KB
ANCS client: 15KB
Multiple GATT services: 40KB
Heap needed: 130KB
⚠️ POSSIBLE (risky, test thoroughly)
```

**Scenario D: Full WiFi + BLE + UART + MIDI**
```
Requirements: >300KB
Available: 300KB
❌ NOT VIABLE (use larger board)
```

---

## UNRESOLVED QUESTIONS

1. **Exact RAM peak usage during simultaneous WiFi+BLE operations** - Depends heavily on WiFi state and BLE activity timing. Recommend empirical testing with `heap_caps_get_free_size()`.

2. **BLE central mode (scanner/client) RAM overhead** - Research indicates similar to peripheral but not explicitly confirmed for C3.

3. **Can BLE mesh coexist with WiFi on ESP32-C3** - Theoretically yes via coexistence, but no real-world validation found.

4. **ANCS implementation latency/throughput** - Custom implementations not benchmarked.

5. **Maximum concurrent GATT connections** - Spec says 20+ devices, but RAM typically limits to 2-3 on C3.

---

## FINAL VERDICT

**ESP32-C3 is a SOLID choice for BLE projects IF:**
1. You use **NimBLE**, not Bluedroid
2. You focus on **single-purpose BLE** (not trying to do everything)
3. RAM budget is **carefully managed** (~40-50KB for BLE, ~150KB app max)
4. You accept **practical range of 30-40m** without antenna upgrade
5. You understand **WiFi+BLE trade-offs** (not true parallel)

**Best Practices:**
- Default to NimBLE-Arduino
- Use beacons for lowest complexity
- Plan for ~65KB minimum BLE overhead
- Test power consumption early
- Consider ESP32-S3 if you need >400KB total RAM

---

## SOURCES

**Hardware & Specifications:**
- [ESP32-C3 Wi-Fi & BLE 5 SoC - Espressif Systems](https://www.espressif.com/en/products/socs/esp32-c3)
- [ESP32-C3 Series Datasheet Version 2.2 - Espressif](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [ESP32-C3 Programming Guide - Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/ble/overview.html)

**Memory & Performance:**
- [Minimizing RAM Usage - ESP32-C3 - Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/performance/ram-usage.html)
- [Memory Types - ESP32-C3 - Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/memory-types.html)
- [Memory Availability Comparison - Espressif Developer Portal](https://developer.espressif.com/blog/memory-availability-comparison-between-esp32-and-esp32-c3/)

**BLE Profiles & Services:**
- [Bluetooth LE AT Commands - ESP32-C3 - Espressif](https://docs.espressif.com/projects/esp-at/en/latest/esp32c3/AT_Command_Set/BLE_AT_Commands.html)
- [Bluetooth LE & Bluetooth - Espressif FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/bt/ble.html)

**WiFi/BLE Coexistence:**
- [RF Coexistence - ESP32-C3 - Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/coexist.html)
- [Coexistence FAQ - Espressif](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/coexistence.html)
- [ESP32C3 BLE to WiFi Bridge - Instructables](https://www.instructables.com/ESP32C3-BLE-to-WiFi-Bridge-BLE-WiFi-Running-Togeth/)

**BLE Range:**
- [XIAO ESP32C3 Bluetooth Tutorial & Range Test - Electronic Clinic](https://www.electroniclinic.com/xiao-esp32c3-bluetooth-tutorial-range-test-and-home-automation/)
- [ESP32-C3 Bluetooth & WiFi Testing - Victor Zheng](https://victorzheng.ca/articles/technology/esp32-c3-groove-bluetooth-wifi/)

**HID Support:**
- [asterics/esp32_mouse_keyboard - GitHub](https://github.com/asterics/esp32_mouse_keyboard)
- [Arduino & BLE HID Issues - Espressif GitHub](https://github.com/espressif/arduino-esp32/issues/9171)

**BLE MIDI:**
- [ESP32-BLE-MIDI - GitHub](https://github.com/max22-/ESP32-BLE-MIDI)
- [Arduino-BLE-MIDI - GitHub](https://github.com/lathoub/Arduino-BLE-MIDI)
- [Control Surface MIDI over BLE - Documentation](https://tttapa.github.io/Control-Surface/Doxygen/db/d99/md_pages_MIDI-over-BLE.html)

**ANCS Implementation:**
- [esp32_ANCS - GitHub](https://github.com/S-March/esp32_ANCS)
- [Arduino_ESP32_ANCS - GitHub](https://github.com/takeru/Arduino_ESP32_ANCS)
- [ESP32-iPhone-Notifications - GitHub](https://github.com/asfahanieyad/ESP32-iPhone-Notifications)

**BLE Beacons:**
- [ESP32 BLE Beacon - ESPHome](https://esphome.io/components/esp32_ble_beacon/)
- [XIAO ESP32-C3 iBeacon with BME680 - Seeed Studio Wiki](https://wiki.seeedstudio.com/xiao-c3-ibeacon/)
- [Build ESP32 Bluetooth iBeacon - Circuit Digest](https://circuitdigest.com/microcontroller-projects/esp32-based-bluetooth-ibeacon/)

**BLE Serial Bridge:**
- [esp32-bluetooth-bridge - GitHub](https://github.com/coddingtonbear/esp32-bluetooth-bridge)
- [ESP32_BleSerial - GitHub](https://github.com/avinabmalla/ESP32_BleSerial)
- [esp32_serial_ble_bridge - GitHub](https://github.com/riozebratubo/esp32_serial_ble_bridge)
- [esp32-ble-uart-mx - GitHub](https://github.com/olegv142/esp32-ble-uart-mx)

**Libraries & Arduino:**
- [NimBLE-Arduino - GitHub](https://github.com/h2zero/NimBLE-Arduino)
- [NimBLE-Arduino Documentation](https://h2zero.github.io/NimBLE-Arduino/)
- [NimBLE-Arduino - PlatformIO Registry](https://registry.platformio.org/libraries/h2zero/NimBLE-Arduino)
- [NimBLE-Arduino - Arduino Libraries](https://docs.arduino.cc/libraries/nimble-arduino/)

**Power Consumption:**
- [ESP32-C3 Power Consumption Chart - ESP32 Forum](https://esp32.com/viewtopic.php?t=23014)
- [Power Consumption Tests - GitHub Gist](https://gist.github.com/fabianoriccardi/83fd33797cfb301d6c7f5c4db4b5616d)
- [Low Power Consumption Guide - Seeed Studio](https://files.seeedstudio.com/wiki/Seeed-Studio-XIAO-ESP32/Low_Power_Consumption.pdf)
- [Power Management in Smart Light - Espressif](https://espressif.github.io/esp32-c3-book-en/chapter_12/12.4/index.html)

**Sleep Modes:**
- [Sleep Modes - ESP32-C3 - Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/sleep_modes.html)
- [ESP32 Deep Sleep with Arduino IDE - Random Nerd Tutorials](https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/)
- [Deep Sleep Component - ESPHome](https://esphome.io/components/deep_sleep/)
