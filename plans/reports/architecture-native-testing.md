# Native Testing Architecture for CrossPoint Reader

---

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    TEST EXECUTION (Native)                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Google Test Framework (gtest)                  │
│  - Test discovery and execution                             │
│  - Assertions and matchers                                  │
│  - Test fixtures and setup/teardown                         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│          Activity Under Test (Native Compiled)              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ ToolsActivity / ClockActivity / etc.                 │   │
│  │ - State machine logic                                │   │
│  │ - User input handling                                │   │
│  │ - Data model operations                              │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│         HAL Interface (Abstract C++ Classes)                │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ HalDisplay | HalGPIO | HalStorage | HalPowerManager │   │
│  │ (Virtual functions only, no implementation)          │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│        Mock HAL Implementations (Test Only)                 │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ MockHalDisplay / MockHalGPIO / etc.                  │   │
│  │ - Record method calls                                │   │
│  │ - Simulate hardware behavior                         │   │
│  │ - Return controllable state                          │   │
│  │ (Compiled only for native tests, not in release)     │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │  Desktop Memory  │
                    │  (No Hardware)   │
                    └──────────────────┘
```

---

## Directory Structure

```
crosspoint-reader/
├── platformio.ini
│   └── [env:native]  ← New test environment
├── src/
│   ├── activities/
│   │   └── tools/
│   │       ├── ToolsActivity.h/cpp
│   │       ├── ClockActivity.h/cpp
│   │       ├── PomodoroActivity.h/cpp
│   │       ├── DailyQuoteActivity.h/cpp
│   │       └── ConferenceBadgeActivity.h/cpp
│   └── ...
├── lib/
│   ├── hal/
│   │   ├── HalDisplay.h/cpp       ← Existing abstraction
│   │   ├── HalGPIO.h/cpp
│   │   ├── HalStorage.h/cpp
│   │   └── HalPowerManager.h/cpp
│   ├── GfxRenderer/               ← Existing renderer
│   └── ...
└── test/
    ├── mocks/                      ← NEW: Test implementations
    │   ├── mock_hal_display.h
    │   ├── mock_hal_gpio.h
    │   ├── mock_hal_storage.h
    │   ├── mock_hal_power_manager.h
    │   └── mock_gfx_renderer.h
    ├── activities/                 ← NEW: Test files
    │   ├── test_tools_activity.cpp
    │   ├── test_clock_activity.cpp
    │   ├── test_pomodoro_activity.cpp
    │   ├── test_daily_quote_activity.cpp
    │   └── test_conference_badge_activity.cpp
    └── README                      ← NEW: Test documentation
```

---

## Compilation Flow: Native vs. Device

### Device Build (Existing)
```
platformio.ini [env:default]
  ↓
Compiler: arm-gcc for ESP32-C3
  ↓
Link with: Real HAL implementations (HalDisplay, HalGPIO, etc.)
  ↓
Linker: arm-elf-ld (cross-compiler)
  ↓
Output: firmware.elf → firmware.bin (flashed to device)
  ↓
Runtime: Runs on ESP32-C3 hardware
```

### Native Test Build (New)
```
platformio.ini [env:native]
  ↓
Compiler: g++/clang for macOS/Linux/Windows
  ↓
Link with: Mock HAL implementations (MockHalDisplay, MockHalGPIO, etc.)
  ↓
Linker: macOS/Linux linker (native)
  ↓
Output: test executable (runs on development machine)
  ↓
Runtime: Runs on desktop CPU (x86-64 / Apple Silicon)
```

---

## Data Flow: Test Scenario (Pomodoro Activity)

### Scenario: User starts 25-minute work timer

```
┌───────────────────────────────────────────────────────────────┐
│ TEST CODE                                                     │
├───────────────────────────────────────────────────────────────┤
│ PomodoroActivity activity(renderer, input);                   │
│ activity.onEnter();                                           │
│ activity.handleInput(INPUT_START);                            │
│                                                               │
│ Verify: activity.getState() == WORK                           │
│ Verify: display.refresh() was called                          │
└───────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌───────────────────────────────────────────────────────────────┐
│ ACTIVITY CODE (Native Compiled)                              │
├───────────────────────────────────────────────────────────────┤
│ void PomodoroActivity::handleInput(InputEvent event) {        │
│   if (event == INPUT_START) {                                 │
│     state = WORK;                                             │
│     startTimer(WORK, 25*60*1000);                            │
│     renderer.render(this);                                    │
│   }                                                            │
│ }                                                              │
│                                                               │
│ void PomodoroActivity::render(RenderLock&&) {                │
│   display.refresh();    ← Mock receives call                 │
│ }                                                              │
└───────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌───────────────────────────────────────────────────────────────┐
│ HAL ABSTRACTION                                              │
├───────────────────────────────────────────────────────────────┤
│ Virtual class HalDisplay {                                    │
│   virtual void refresh() = 0;                                 │
│ }                                                              │
└───────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌───────────────────────────────────────────────────────────────┐
│ MOCK IMPLEMENTATION (Test Only)                              │
├───────────────────────────────────────────────────────────────┤
│ class MockHalDisplay : public HalDisplay {                    │
│   void refresh() override {                                   │
│     calls.push_back("refresh");  ← Recorded                 │
│   }                                                            │
│ }                                                              │
└───────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌───────────────────────────────────────────────────────────────┐
│ TEST VERIFICATION                                            │
├───────────────────────────────────────────────────────────────┤
│ ASSERT_NE(display.calls.end(),                               │
│           std::find(display.calls.begin(),                    │
│                     display.calls.end(),                      │
│                     "refresh"));                              │
│                                                               │
│ ✓ PASS: refresh() was called                                 │
└───────────────────────────────────────────────────────────────┘
```

---

## Memory Model: Native Tests vs. Device

### Device Execution
```
ESP32-C3 RAM: ~380KB
┌──────────────────────┐
│ Firmware Code  ────┐ │
│ Const Data     ────┤ │
│ Stack & Heap   ────┤ │  Limited by hardware
│ (Dynamic Memory)   └─┤
│ (Careful caching)   │
└──────────────────────┘
```

### Native Test Execution
```
Desktop RAM: Gigabytes
┌────────────────────────────┐
│ Activity Code              │
│ Mock Objects               │
│ Test Fixtures              │
│ Google Test Framework       │  No memory constraints
│ (As much as needed)         │
└────────────────────────────┘
```

**Benefit**: No need for careful optimization during testing. Discover logic bugs easily.

---

## Mock HAL: Responsibility Matrix

| Mock Component | Responsibility | Tested |
|---|---|---|
| **MockHalDisplay** | Record drawing calls, simulate framebuffer | ✓ |
| **MockHalGPIO** | Simulate button presses, vibration | ✓ |
| **MockHalStorage** | In-memory file system | ✓ |
| **MockHalPowerManager** | Simulate sleep/wake states | ✓ |
| **MockGfxRenderer** | Verify drawing commands | ✓ |

---

## Test Organization: By Activity

```
test/activities/
├── test_tools_activity.cpp
│   ├── ToolsActivityTest::MenuNavigates
│   ├── ToolsActivityTest::EntersSubactivity
│   └── ToolsActivityTest::ReturnsFromSubactivity
│
├── test_clock_activity.cpp
│   ├── ClockActivityTest::DisplaysTime
│   ├── ClockActivityTest::UpdatesEverySecond
│   └── ClockActivityTest::HandlesFormat12vs24
│
├── test_pomodoro_activity.cpp
│   ├── PomodoroActivityTest::StartsTimer
│   ├── PomodoroActivityTest::TransitionsStates
│   ├── PomodoroActivityTest::PersistsState
│   └── PomodoroActivityTest::HandlesInterruption
│
├── test_daily_quote_activity.cpp
│   ├── DailyQuoteActivityTest::LoadsQuote
│   ├── DailyQuoteActivityTest::FormatsText
│   └── DailyQuoteActivityTest::CachesQuotes
│
└── test_conference_badge_activity.cpp
    ├── ConferenceBadgeActivityTest::GeneratesQR
    ├── ConferenceBadgeActivityTest::DisplaysBadge
    └── ConferenceBadgeActivityTest::HandlesInput
```

---

## Test Execution: Timeline

```
$ pio test -e native

[TIMING]
├── [0.1s] Discover tests (gtest finds all test_*.cpp)
├── [0.5s] Compile Activity code (native)
├── [0.2s] Link with mocks
├── [0.3s] Run all tests in parallel
│   ├── test_tools_activity.cpp (5 tests, 10ms)
│   ├── test_clock_activity.cpp (3 tests, 5ms)
│   ├── test_pomodoro_activity.cpp (4 tests, 20ms)
│   ├── test_daily_quote_activity.cpp (3 tests, 8ms)
│   └── test_conference_badge_activity.cpp (3 tests, 15ms)
│
└── [0.0s] Report results (20 passed, 0 failed)

[TOTAL] 1.5 seconds
```

---

## Mock Pattern: Call Verification Example

### Pattern: Record all calls, verify in test

```cpp
// mock_hal_display.h
class MockHalDisplay {
  std::vector<std::string> calls;  ← Call history

  void clear() { calls.push_back("clear"); }
  void refresh() { calls.push_back("refresh"); }
  void drawPixel(int x, int y, bool color) {
    calls.push_back("drawPixel:" + std::to_string(x) + "," + std::to_string(y));
  }

  bool wasCalledWith(const std::string& call) {
    return std::find(calls.begin(), calls.end(), call) != calls.end();
  }

  void clearCalls() { calls.clear(); }
};

// test_example.cpp
TEST_F(ActivityTest, ClearsDisplayBeforeRender) {
  display.clearCalls();

  activity.render(lock);

  EXPECT_TRUE(display.wasCalledWith("clear"));
  EXPECT_TRUE(display.wasCalledWith("refresh"));
}
```

---

## Integration Points: Activity ↔ Mock

### Injection Pattern

```cpp
// Option 1: Singleton mocks (simplest, works for testing)
HalDisplay& display = HalDisplay::instance();
display.refresh();  ← Will use MockHalDisplay in tests

// Option 2: Constructor injection (better for DI)
Activity activity(renderer, input, &display);
// In test, pass mock: Activity activity(renderer, input, &mockDisplay);
```

**Recommendation**: Start with singleton pattern for quick setup, migrate to DI later if needed.

---

## Test Lifecycle: Fixture Pattern

```cpp
class ActivityTest : public ::testing::Test {
  // 1. SETUP: Runs before each test
  void SetUp() override {
    display.clearCalls();
    gpio.clearState();
    storage.clearFiles();
    // Activity ready for testing
  }

  // 2. TEST RUNS: Single test function
  // TEST_F(ActivityTest, SomeTest) { ... }

  // 3. TEARDOWN: Runs after each test
  void TearDown() override {
    // Cleanup if needed
    // (Usually not needed for mocks)
  }
};
```

---

## Success Metrics

After implementation, you should achieve:

| Metric | Target | Method |
|--------|--------|--------|
| **Test coverage** | >80% | `pio test -e native --cov` |
| **Execution time** | <1 sec | Baseline test run |
| **Debugging speed** | 5-10 min per issue | Set breakpoints in IDE |
| **CI/CD integration** | <5 sec | Run in GitHub Actions |
| **Activity testing** | All 5 activities | 20+ tests total |

---

## Next: Optional Wokwi Integration

After native tests are working, optionally add Wokwi:

```json
// wokwi.toml (optional)
[wokwi]
version = 1
pio_home = "/path/to/pio"
firmware = ".pio/build/default/firmware.elf"

// diagram.json (optional)
{
  "version": 1,
  "author": "Your Name",
  "board": "esp32-c3-devkitm-1",
  "parts": [
    {"type": "board", "id": "board"},
    {"type": "wokwi-pushbutton", "id": "btn1"}
  ]
}
```

Use Wokwi for:
- Interactive button testing (manual)
- Visual verification of UI
- Serial console debugging
- Integration smoke tests

---

## Debugging Native Tests

### In VS Code
1. Set breakpoint in test or Activity code
2. `Debug` → `C++ (gdb)` configuration
3. Run `pio test -e native` with debugger
4. Step through code, inspect variables

### Command Line
```bash
# Verbose output
pio test -e native -v

# Run specific test
pio test -e native -f test_clock_activity

# With output capturing disabled
pio test -e native --capture=no
```

### Common Assertions
```cpp
// Equality
EXPECT_EQ(activity.getState(), WORK);
EXPECT_NE(display.calls.size(), 0);

// Boolean
EXPECT_TRUE(storage.fileExists("timer.json"));
EXPECT_FALSE(gpio.isButtonPressed());

// Collections
EXPECT_EQ(display.calls.size(), 3);
ASSERT_THAT(display.calls, ::testing::Contains("refresh"));
```

