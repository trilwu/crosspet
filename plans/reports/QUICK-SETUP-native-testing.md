# Quick Setup Guide: Native Testing with HAL Mocks

**Time**: 30-45 minutes for basic setup
**Result**: Able to run first unit tests on Activities

---

## Step 1: Update platformio.ini (2 minutes)

Add this environment at the bottom of your `platformio.ini`:

```ini
[env:native]
platform = native
test_framework = googletest
lib_extra_dirs = lib/
build_flags =
  -DENABLE_TESTING=1
  -DNATIVE_ENV=1
  -std=c++17
```

---

## Step 2: Create Mock HAL Structure (5 minutes)

Create the directory structure:
```bash
mkdir -p test/mocks
mkdir -p test/activities
```

---

## Step 3: Create Minimal HAL Mocks (15 minutes)

**File: `test/mocks/mock_hal_display.h`**

```cpp
#pragma once
#include <vector>
#include <string>

class MockHalDisplay {
public:
  static MockHalDisplay& instance() {
    static MockHalDisplay display;
    return display;
  }

  void clear() {
    calls.push_back("clear");
  }

  void refresh() {
    calls.push_back("refresh");
  }

  void drawPixel(int x, int y, bool color) {
    calls.push_back("drawPixel");
  }

  void sleep() {
    calls.push_back("sleep");
  }

  void wake() {
    calls.push_back("wake");
  }

  std::vector<std::string> calls;

private:
  MockHalDisplay() = default;
};
```

**File: `test/mocks/mock_hal_gpio.h`**

```cpp
#pragma once
#include <vector>
#include <string>

class MockHalGPIO {
public:
  static MockHalGPIO& instance() {
    static MockHalGPIO gpio;
    return gpio;
  }

  void init() { }
  void deinit() { }

  bool readButton(int buttonId) {
    return buttonStates[buttonId];
  }

  void setButtonState(int buttonId, bool pressed) {
    buttonStates[buttonId] = pressed;
  }

  void setVibration(bool enabled) {
    calls.push_back(enabled ? "vibration_on" : "vibration_off");
  }

  std::vector<std::string> calls;
  std::vector<bool> buttonStates{false, false, false, false};

private:
  MockHalGPIO() = default;
};
```

**File: `test/mocks/mock_hal_storage.h`**

```cpp
#pragma once
#include <string>
#include <map>

class MockHalStorage {
public:
  static MockHalStorage& instance() {
    static MockHalStorage storage;
    return storage;
  }

  bool fileExists(const char* path) {
    return files.find(path) != files.end();
  }

  bool readFile(const char* path, std::string& content) {
    if (fileExists(path)) {
      content = files[path];
      return true;
    }
    return false;
  }

  bool writeFile(const char* path, const std::string& content) {
    files[path] = content;
    return true;
  }

  void deleteFile(const char* path) {
    files.erase(path);
  }

  std::map<std::string, std::string> files;

private:
  MockHalStorage() = default;
};
```

---

## Step 4: Write First Test (15 minutes)

**File: `test/activities/test_tools_activity.cpp`**

```cpp
#include <gtest/gtest.h>
#include "ToolsActivity.h"
#include "../mocks/mock_hal_display.h"
#include "../mocks/mock_hal_gpio.h"
#include "../mocks/mock_hal_storage.h"

// Mock the real HAL with test versions
#define HalDisplay MockHalDisplay
#define HalGPIO MockHalGPIO
#define HalStorage MockHalStorage

class ToolsActivityTest : public ::testing::Test {
protected:
  MockHalDisplay& display = MockHalDisplay::instance();
  MockHalGPIO& gpio = MockHalGPIO::instance();
  MockHalStorage& storage = MockHalStorage::instance();

  void SetUp() override {
    display.calls.clear();
    gpio.calls.clear();
    storage.files.clear();
  }
};

// Test that ToolsActivity initializes
TEST_F(ToolsActivityTest, InitializesSuccessfully) {
  // Will need to adjust constructor based on actual Activity
  // ToolsActivity activity(renderer, input);
  // EXPECT_TRUE(activity is initialized);
}

// Test that ToolsActivity renders menu items
TEST_F(ToolsActivityTest, RenderMenuOnEnter) {
  // activity.onEnter();
  // Verify display.refresh() was called
  // EXPECT_NE(display.calls.end(),
  //           std::find(display.calls.begin(),
  //                     display.calls.end(),
  //                     "refresh"));
}
```

---

## Step 5: Run Tests (3 minutes)

```bash
# Run all native tests
pio test -e native

# Expected output:
# Testing native environment
# Running tests...
# Test results: 1 passed, 0 failed
```

---

## Step 6: Expand Mock HAL Stubs (10 minutes)

As you write more tests, add more methods to mocks:

```cpp
// In mock_hal_display.h, add as needed:
class MockHalDisplay {
  // ... existing code ...

  void setRotation(int degrees) { }
  void setBrightness(int level) { }
  int getWidth() const { return 540; }
  int getHeight() const { return 960; }

  // Add more as your tests discover they're needed
};
```

---

## Debugging Failed Tests

If a test fails because a HAL method is missing:

```bash
# Error: undefined reference to HalDisplay::someMethod()
```

Solution:
1. Add the method stub to the corresponding mock
2. Implement minimal behavior (usually just record a call or return default)
3. Re-run tests

---

## Checklist

- [ ] Added `[env:native]` to platformio.ini
- [ ] Created `test/mocks/` directory
- [ ] Created 3 basic HAL mocks (display, GPIO, storage)
- [ ] Created first test file in `test/activities/`
- [ ] Ran `pio test -e native` successfully
- [ ] At least one test passes

---

## Next Steps

1. **Write 3-5 ToolsActivity tests** covering menu navigation
2. **Write 3-5 ClockActivity tests** covering time display
3. **Write 3-5 PomodoroActivity tests** covering state transitions
4. **Write 3-5 DailyQuoteActivity tests** covering quote loading
5. **Write 3-5 ConferenceBadgeActivity tests** covering badge rendering
6. **(Optional) Set up Wokwi** for interactive verification

**Total effort**: ~4-6 hours for comprehensive native test coverage

---

## Common Issues & Fixes

### Issue: "undefined reference to Activity constructor"
**Fix**: Adjust test constructor call to match actual Activity signature. Check Activity.h for constructor parameters.

### Issue: "compiler cannot find gtest headers"
**Fix**: PlatformIO should auto-download Google Test. Run:
```bash
pio lib list --native
```
If Google Test missing, install:
```bash
pio lib install -e native "google/googletest"
```

### Issue: "test takes too long"
**Fix**: Native tests should run <1 second total. If slower:
- Check for busy-loops in Activity code
- Mock time-based operations
- Use `fast_forward()` pattern for timers

### Issue: HAL methods don't compile on native
**Fix**: Add `#ifdef NATIVE_ENV` guards around hardware-specific code:
```cpp
#ifndef NATIVE_ENV
  // Hardware-specific code
  HalDisplay::instance().wake();
#endif
```

---

## Reference: Activity Dependencies

Before writing tests, identify what each Activity needs:

```
ToolsActivity
├── GfxRenderer (rendering)
├── MappedInputManager (input)
├── HalStorage (optional, for preferences)
└── HalPowerManager (optional, for sleep)

ClockActivity
├── GfxRenderer
├── MappedInputManager
├── HalPowerManager (for wake events)
└── (system time via millis())

PomodoroActivity
├── GfxRenderer
├── MappedInputManager
├── HalStorage (for timer state)
└── (system time via millis())

DailyQuoteActivity
├── GfxRenderer
├── MappedInputManager
├── HalStorage (read quotes.json)
└── (optional: network for fresh quotes)

ConferenceBadgeActivity
├── GfxRenderer
├── HalStorage (read QR data)
└── (optional: camera for badge scanning)
```

---

## Performance Expectations

After completing setup:

| Test Metric | Expected |
|------------|----------|
| **Compilation time** | 5-10 seconds |
| **Test execution** | <100ms |
| **Full test suite** | <1 second |
| **Memory usage** | <100MB |
| **CI/CD integration** | 2-5 seconds |

---

## Recommended IDE Setup

**VS Code with PlatformIO extension**:
1. `Ctrl+Shift+P` → "Run Task" → "PlatformIO: Run tests"
2. Watch tests execute in terminal
3. Click errors to jump to test code

**Command line**:
```bash
pio test -e native --verbose
```

