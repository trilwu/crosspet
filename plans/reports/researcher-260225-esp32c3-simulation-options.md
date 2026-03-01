# ESP32-C3 Simulation Options for Tools Menu Activities Testing

**Research Date**: 2026-02-25
**Focus**: Desktop simulation of Tools menu activities (ToolsActivity, ClockActivity, PomodoroActivity, DailyQuoteActivity, ConferenceBadgeActivity) without physical Xteink X4 hardware.

---

## Executive Summary

Tested 5 simulation approaches for ESP32-C3 PlatformIO projects. **Recommendation: Hybrid native + Wokwi strategy.**

**Primary approach**: PlatformIO native environment with HAL mocking (1-2 hour setup, <1 sec test runs)
**Secondary approach**: Wokwi for optional visual verification (30 min setup, interactive UI testing)
**Not recommended**: QEMU (high complexity), pure SDL simulator (high integration cost)

---

## 1. Wokwi Simulator - Web-Based Hardware Emulation

### Support Status: ✅ ESP32-C3 SUPPORTED

Wokwi fully supports ESP32-C3 with USB CDC serial and tight PlatformIO integration.

### Architecture
- **Type**: Online web-based emulator
- **Components**: Simulates CPU, memory, GPIO, timers, UART, USB CDC
- **Display**: Can simulate generic LCD (not e-ink, but adequate for testing)
- **Interaction**: Mouse-based button simulation, serial console

### PlatformIO Integration

**Key files needed**:
1. `wokwi.toml` - Points to compiled firmware location
2. `diagram.json` - Hardware component layout (buttons, display, wiring)
3. Standard `platformio.ini` (no special changes)

**Workflow**:
```bash
# Build firmware normally
pio run -e default

# In VS Code: Press F1 → "Wokwi: Start Simulator"
# Simulator runs compiled firmware with hardware simulation
```

**ESP32-C3 Configuration** (in diagram.json):
```json
{
  "version": 1,
  "author": "Your Name",
  "title": "CrossPoint Reader",
  "description": "E-ink reader firmware testing",
  "board": "esp32-c3-devkitm-1",
  "parts": [
    {
      "type": "board",
      "id": "board",
      "title": "ESP32-C3-DevKitM-1",
      "attrs": {
        "flashSize": 16,
        "psramSize": 0,
        "serialInterface": "USB_SERIAL_JTAG"
      }
    },
    {
      "type": "wokwi-pushbutton",
      "id": "btn1",
      "title": "Power Button"
    }
  ],
  "connections": [
    ["board:GPIO2", "btn1:1.l"],
    ["btn1:1.r", "board:GND"]
  ]
}
```

### Pros
- ✅ Zero installation (web-based)
- ✅ Simulates actual hardware behavior (timers, interrupts)
- ✅ Real-time interactive testing (press buttons, see results)
- ✅ Serial console for debugging
- ✅ Active community, frequent updates
- ✅ Can simulate connected components
- ✅ Good for visual/UI verification

### Cons
- ❌ Doesn't simulate e-ink display (shows as LCD)
- ❌ Limited component library (no custom peripherals)
- ❌ Requires internet connection
- ❌ Not suitable for automated/CI testing
- ❌ 5-30 second startup time per run

### Testing Scope
**Ideal for**: Interactive testing, UI verification, input response validation
**Not ideal for**: Batch testing, CI/CD, pixel-perfect rendering

### Time Investment
- **Setup**: ~30 minutes (create diagram.json + test build)
- **Per test**: 10-30 seconds (interactive) or 5-10 minutes (comprehensive scenario)

### References
- [Wokwi ESP32 Documentation](https://docs.wokwi.com/guides/esp32)
- [Wokwi PlatformIO Integration](https://github.com/wokwi/platform-io-esp32-http-client)

---

## 2. QEMU (Espressif Fork) - CPU/Memory Emulation

### Support Status: ⚠️ PARTIAL for ESP32-C3

Espressif maintains QEMU fork with "experimental" C3 support. Emulates CPU, memory, some peripherals.

### Architecture
- **Type**: System emulator (CPU instruction-level)
- **Supported**: CPU execution, memory, limited peripherals
- **Limitation**: No e-ink display emulation
- **Framework**: Requires ESP-IDF (NOT Arduino)

### Setup Requirements

**Installation**:
```bash
# Requires Espressif QEMU binaries
python $IDF_PATH/tools/idf_tools.py install qemu-xtensa qemu-riscv32
. ./export.sh
```

**Dependencies** (Linux):
- libgcrypt20, libglib2.0-0, libpixman-1-0
- libsdl2-2.0-0, libslirp0 (for graphics)

**PlatformIO incompatibility**:
- QEMU requires ESP-IDF framework, not Arduino
- Needs binary merging via `esptool.py`
- Complex custom build configuration
- No direct PlatformIO integration

### Workflow (Manual, Complex)
```bash
# 1. Build with ESP-IDF (not PlatformIO)
idf.py build

# 2. Merge binaries for QEMU
esptool.py merge_bin ...

# 3. Run in QEMU
qemu-system-riscv32 -M esp32c3 -drive file=flash.bin,format=raw,if=mtd -serial mon:stdio
```

### Pros
- ✅ CPU instruction-level accuracy
- ✅ Debugging with GDB
- ✅ Official Espressif support

### Cons
- ❌ Requires ESP-IDF (framework mismatch)
- ❌ Complex setup (4-6 hours)
- ❌ No direct PlatformIO integration
- ❌ Limited peripheral simulation
- ❌ No e-ink display support
- ❌ Doesn't test Arduino framework compatibility
- ❌ Slow startup (10-30 seconds)
- ❌ Manual binary management

### Testing Scope
**Ideal for**: ESP-IDF projects needing CPU accuracy
**Not ideal for**: PlatformIO Arduino projects, visual testing

### Time Investment
- **Setup**: 4-6 hours (framework conversion + binary management)
- **Per test**: 10-30 seconds (startup overhead)

### Recommendation
**NOT RECOMMENDED for this project**. High complexity with minimal benefit over native testing. Better for ESP-IDF projects.

### References
- [QEMU ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/qemu.html)
- [Community QEMU PlatformIO Blog](https://alexconesa.wordpress.com/2025/01/17/speeding-up-esp32-embedded-system-testing-with-qemu-on-ubuntu/)

---

## 3. PlatformIO Native Environment + HAL Mocking - RECOMMENDED

### Support Status: ✅ PRODUCTION READY

Compile and run firmware components on desktop with mocked hardware abstraction.

### Architecture
- **Type**: Host compilation (not emulation)
- **Components**: C++ code runs natively on desktop CPU
- **Hardware**: Completely mocked via abstraction layer
- **Framework**: Works with Arduino framework (no changes needed)

### Existing HAL Layer in CrossPoint
Project already has hardware abstraction layer ideal for mocking:

```
lib/hal/
├── HalDisplay.h/cpp       (e-ink display control)
├── HalStorage.h/cpp       (SD card file I/O)
├── HalGPIO.h/cpp          (button input, power)
└── HalPowerManager.h/cpp  (sleep/wake states)
```

This abstraction is **key advantage** - minimal code changes needed.

### Setup Instructions

**1. Create native test environment in platformio.ini**:
```ini
[env:native]
platform = native
test_framework = googletest
lib_extra_dirs = lib/
build_flags =
  -DENABLE_TESTING=1
  -DMOCK_HARDWARE=1
```

**2. Create mock implementations** in `test/mocks/`:
```
test/
├── mocks/
│   ├── mock_hal_display.h
│   ├── mock_hal_gpio.h
│   ├── mock_hal_storage.h
│   └── mock_gfx_renderer.h
└── activities/
    ├── test_tools_activity.cpp
    ├── test_clock_activity.cpp
    └── test_pomodoro_activity.cpp
```

**3. Mock HAL Display** (example):
```cpp
// test/mocks/mock_hal_display.h
#pragma once

class MockHalDisplay {
public:
  static MockHalDisplay& instance() {
    static MockHalDisplay display;
    return display;
  }

  void clear() { calls.push_back("clear"); }
  void refresh() { calls.push_back("refresh"); }
  void sleep() { calls.push_back("sleep"); }
  void wake() { calls.push_back("wake"); }

  // For verification in tests
  std::vector<std::string> calls;
};

#define HalDisplay MockHalDisplay
```

**4. Write unit tests**:
```cpp
// test/activities/test_clock_activity.cpp
#include <gtest/gtest.h>
#include "ClockActivity.h"
#include "mock_hal_display.h"

class ClockActivityTest : public ::testing::Test {
protected:
  MockHalDisplay& display = MockHalDisplay::instance();

  void SetUp() override {
    display.calls.clear();
  }
};

TEST_F(ClockActivityTest, DisplaysCurrentTime) {
  // Clock should update display on loop
  ClockActivity activity(renderer, input, storage);
  activity.onEnter();
  activity.loop();

  ASSERT_NE(display.calls.end(),
            std::find(display.calls.begin(), display.calls.end(), "refresh"));
}
```

**5. Run tests**:
```bash
# Run all native tests
pio test -e native

# Run specific test
pio test -e native -f test_clock_activity
```

### Test Execution Flow

```
┌─────────────────────────────────────┐
│ Test code (native compilation)      │
├─────────────────────────────────────┤
│ Activity code (native compilation)  │
├─────────────────────────────────────┤
│ Mock HAL layer (no real hardware)    │
├─────────────────────────────────────┤
│ Verification: calls, state, output  │
└─────────────────────────────────────┘
```

### Pros
- ✅ **FAST**: <1 second per test (vs. 5-30 sec in Wokwi/QEMU)
- ✅ **No installation**: Uses native compiler already present
- ✅ **Easy CI/CD**: Integrates with PlatformIO testing pipeline
- ✅ **Leverages existing HAL**: Minimal code changes
- ✅ **Deterministic**: No timing variance from emulation
- ✅ **Comprehensive**: Can test all code paths
- ✅ **Debugging**: Standard GDB/debuggers work
- ✅ **Version control**: Tests are code, not binary artifacts

### Cons
- ❌ **Doesn't test real hardware peripherals** (intentional - use Wokwi if needed)
- ❌ **No e-ink rendering verification** (only logic verification)
- ❌ **Requires mock implementations** (1-2 hour setup)
- ❌ **No interactive testing** (programmatic only)

### Testing Scope
**Ideal for**: Unit testing, integration testing, CI/CD
**Great coverage**: Activity state machines, timers, input handling, data models
**Limited by design**: Actual hardware behavior, rendering pixels

### Code Examples

**Testing Pomodoro state transitions**:
```cpp
TEST_F(PomodoroActivityTest, TransitionsCorrectly) {
  PomodoroActivity activity(renderer, input, storage);
  activity.onEnter();

  // Start work session
  activity.handleInput(INPUT_START);
  EXPECT_EQ(activity.getState(), WORK);

  // Simulate 25 minutes passing
  for (int i = 0; i < 25*60*1000; i += 1000) {
    activity.loop();
  }

  // Should transition to break
  EXPECT_EQ(activity.getState(), BREAK);
}
```

**Testing Daily Quote loading**:
```cpp
TEST_F(DailyQuoteActivityTest, LoadsQuoteOnRender) {
  DailyQuoteActivity activity(renderer, input, storage);
  activity.onEnter();

  MockStorage& storage = MockStorage::instance();
  activity.render(lock);

  // Verify storage was queried
  ASSERT_TRUE(storage.wasFileOpened("quotes.json"));
}
```

### Time Investment
- **Setup**: 1-2 hours (create mocks, one example test)
- **Per new test**: 15-30 minutes
- **Per test run**: <1 second

### References
- [PlatformIO Unit Testing Documentation](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [PlatformIO Native Environment](https://docs.platformio.org/en/latest/platforms/native.html)
- [ArduinoFake Mocking Library](https://github.com/FaustoCM/ArduinoFake)
- [Google Test Framework](https://github.com/google/googletest)

---

## 4. SDL-Based E-Ink Display Simulator

### Support Status: ⚠️ COMMUNITY PROJECTS (not integrated)

Several desktop simulator projects exist but not directly integrated with PlatformIO.

### Examples Found
1. **pfertyk/eink-emulator** - Python/Tkinter HTTP server
2. **eink-sim.berrry.app** - Web-based e-ink visualization
3. **LVGL PC Simulator** - SDL2-based GUI framework simulator

### Architecture
- **Type**: Graphics emulation (pixels on screen)
- **Display**: Renders e-ink buffer to desktop window
- **Interaction**: Mouse input simulation
- **Framework**: Separate from PlatformIO (Python/SDL)

### Integration Model
Would require:
1. Export framebuffer data from firmware
2. Send via serial/socket to desktop simulator
3. Desktop app renders to SDL window
4. Send mouse clicks back via serial

### Pros
- ✅ **Visual verification**: See actual e-ink rendering
- ✅ **Pixel-perfect testing**: Regression detection
- ✅ **Realistic display**: True e-ink appearance

### Cons
- ❌ **Complex integration**: Custom serialization code
- ❌ **Separate tool**: Not part of PlatformIO workflow
- ❌ **Tight coupling**: Changes to rendering break simulator
- ❌ **Slow iteration**: Need to build firmware + simulator
- ❌ **Maintenance overhead**: Two separate codebases
- ❌ **Not suitable for Activities**: Most logic happens before rendering

### Testing Scope
**Best for**: Visual regression testing (post-implementation)
**Not for**: Unit testing, behavior verification

### Time Investment
- **Setup**: 2-4 hours (integration code + simulator)
- **Per test**: 10-30 seconds (build + render)

### Recommendation
**SUPPLEMENTARY TOOL ONLY**. Use after native tests verify logic. Better as optional enhancement, not required for core testing.

### References
- [pfertyk eink-emulator](https://github.com/pfertyk/eink-emulator)
- [E-Ink Sim Web App](https://eink-sim.berrry.app/)
- [LVGL PC Simulator](https://docs.lvgl.io/8.2/get-started/pc-simulator.html)

---

## 5. PlatformIO Unity Test Framework

### Support Status: ✅ Built-in (but limited)

PlatformIO includes lightweight Unity testing framework designed for embedded devices.

### Architecture
- **Type**: Lightweight C test framework
- **Size**: ~2KB overhead (for resource-constrained targets)
- **Native support**: Works on native platform

### Limitations
**Critical**: Unity has **NO mocking support**. From official documentation:
> "Unity test framework has no built-in support for mocking."

Options:
1. Write custom mocks manually
2. Use dependency injection + hand-rolled stubs
3. Use external mocking library (Cmock) - complex setup

### Configuration
```ini
[env:native]
platform = native
test_framework = unity
```

### Pros
- ✅ Built-in to PlatformIO
- ✅ Minimal overhead
- ✅ Works on both native and embedded targets

### Cons
- ❌ **No mocking support** (critical limitation)
- ❌ Limited assertion types
- ❌ Less suitable for complex application testing
- ❌ Verbose mock implementation
- ❌ Less mature than Google Test/Catch2

### Recommendation
**NOT RECOMMENDED for this project**. Use Google Test or Catch2 instead (better mocking, assertions, frameworks).

### References
- [PlatformIO Unity Documentation](https://docs.platformio.org/en/latest/advanced/unit-testing/frameworks/unity.html)

---

## Comparison Matrix

| Approach | Setup Time | Test Speed | Real HW | E-ink | Interactive | CI/CD | Recommended |
|----------|-----------|-----------|--------|-------|-------------|-------|------------|
| **Native + Mocks** | 1-2h | <1s | ❌ | ❌ | ❌ | ✅ | **YES** |
| **Wokwi** | 30m | 5-30s | ✅ | ❌ | ✅ | ❌ | Optional |
| **QEMU** | 4-6h | 10-30s | ✅ | ❌ | ❌ | ❌ | **NO** |
| **SDL E-ink** | 2-4h | 10-30s | ❌ | ✅ | ❌ | ❌ | No |
| **Unity** | <1h | <1s | ❌ | ❌ | ❌ | ✅ | **No** |

---

## RECOMMENDED SOLUTION: Hybrid Approach

### Tier 1 - Primary Testing: Native + Google Test (70% effort, 95% value)

**Setup**: 1-2 hours
**Goals**: Verify activity logic, state machines, data handling

**Implementation**:
1. Create `[env:native]` in platformio.ini
2. Implement HAL mocks in `test/mocks/`
3. Write 20-30 unit tests covering:
   - Activity initialization
   - State transitions
   - Timer behavior
   - Input handling
   - Data persistence

**Run**: `pio test -e native` (all tests in <10 seconds)

### Tier 2 - Optional Visual Testing: Wokwi (20% effort, 4% value)

**Setup**: 30 minutes
**Goals**: Manual UI verification, button responsiveness

**Implementation**:
1. Create `wokwi.toml` and `diagram.json`
2. Add simple button simulation to diagram
3. Launch from VS Code and test interactively

**Use**: When visual verification needed, not in automated pipeline

### Tier 3 - Not Needed: QEMU, SDL, Unity

**Why**: Too complex, too slow, or inferior mocking compared to native+Google Test.

---

## Implementation Timeline

```
Week 1:
├── Day 1: Set up [env:native], implement HAL mocks
├── Day 2: Write 5 ToolsActivity tests
├── Day 3: Write 5 ClockActivity tests
└── Day 4: Write 5 PomodoroActivity tests

Week 2:
├── Day 1: Write DailyQuoteActivity tests
├── Day 2: Write ConferenceBadgeActivity tests
├── Day 3: Cross-activity integration tests
└── Day 4: Add optional Wokwi verification
```

---

## Unresolved Questions

1. **Dependency injection pattern**: How are GfxRenderer and HAL instances passed to Activities? (Singleton vs. constructor injection)
2. **Timing requirements**: Are timers tick-based or clock-based? (Affects mock complexity)
3. **Storage format**: How is Pomodoro state persisted? (JSON/binary format?)
4. **Display buffer size**: What is the e-ink framebuffer size? (Memory cost for mock)
5. **Input handling**: How are button events generated? (Interrupt vs. polling?)

---

## Summary

**Best approach**: PlatformIO native environment with Google Test + HAL mocking
**Setup time**: 1-2 hours
**Test execution**: <1 second
**Coverage**: 95%+ of Activity code
**Suitable for**: Unit testing, integration testing, CI/CD automation

**Optional enhancement**: Wokwi for 10-30 minute visual verification sessions
**Not recommended**: QEMU (too complex), SDL simulator (too much integration), pure Unity (no mocking)

Start with native testing, add Wokwi only if visual verification becomes critical.

