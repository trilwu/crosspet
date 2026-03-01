# Diagnostic Report: Clock/DailyQuote Auto-Reboot & Battery Drain

**Date:** 2026-02-27
**Commit investigated:** b19258d (partial fix) + current HEAD
**Scope:** ClockActivity, DailyQuoteActivity, ConferenceBadgeActivity, Activity base, main loop, HalPowerManager

---

## Executive Summary

| # | Issue | Status | Severity |
|---|-------|--------|----------|
| 1a | Clock render-task stack overflow (x=0 battery) | **FIXED** in b19258d | Was Critical |
| 1b | DailyQuote render-task stack usage | **PARTIALLY FIXED** in b19258d | Residual Risk |
| 2a | Clock/DailyQuote/Badge drain battery (preventAutoSleep=true always) | **UNRESOLVED** | High |
| 2b | main loop spins at 10–50 ms delay when preventAutoSleep is set | **UNRESOLVED** | Medium |
| 1c | `requestUpdateAndWait()` is a broken stub with `delay(100)` | Pre-existing | Low |

---

## Issue 1: Auto-Reboot — Root Cause Analysis

### 1a. ClockActivity — Stack Overflow via Out-of-Range LOG_ERR Calls (FIXED)

**Root cause (pre-fix):**
`ClockActivity::render()` called `drawBatteryRight` with `Rect{0, ..., pageWidth, ...}`:

```cpp
// OLD (before b19258d) — BUG:
GUI.drawBatteryRight(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.batteryBarHeight});
```

`drawBatteryRight` passes `rect.x` directly as the icon X coordinate:
```cpp
// BaseTheme.cpp line 79
drawBatteryIcon(renderer, rect.x, y, BaseMetrics::values.batteryWidth, rect.height, percentage);
```

`drawBatteryIcon` draws lines starting from `x=0`. But `batteryX=0` is actually fine on its own. The real problem is the `showPercentage=true` default path:

```cpp
// BaseTheme.cpp drawBatteryRight, showPercentage branch:
renderer.fillRect(rect.x - textWidth - batteryPercentSpacing, rect.y, textWidth, textHeight, false);
renderer.drawText(SMALL_FONT_ID, rect.x - textWidth - batteryPercentSpacing, rect.y, ...);
```

With `rect.x = 0`, the text X becomes `0 - textWidth - 4` = **large negative number** (e.g., -50). Every pixel drawn at negative X hits GfxRenderer::drawPixel → `phyX < 0` check → calls `LOG_ERR("GFX", "!! Outside range ...")`. For a typical percentage string (3 chars, ~20px wide), each character glyph has many pixels, causing **thousands of LOG_ERR calls per render frame**. `logPrintf` allocates stack space for `va_list` + `sprintf` buffer on each call, rapidly exhausting the 8192-byte render task stack.

**Fix (b19258d):**
```cpp
// FIXED:
const int batteryX = pageWidth - 12 - metrics.batteryWidth;
GUI.drawBatteryRight(renderer, Rect{batteryX, metrics.topPadding, metrics.batteryWidth, metrics.batteryBarHeight});
```

This is correct. Matches the pattern in `drawHeader()`.

**HOWEVER — remaining risk:** ClockActivity still passes `metrics.batteryBarHeight` (=20) as rect.height, but `drawHeader` passes `BaseMetrics::values.batteryHeight` (=12) as height. The `y + 6` offset in `drawBatteryIcon` means with height=20 the icon draws fine, but the inconsistency vs `drawHeader` is worth noting.

### 1b. DailyQuoteActivity — Stack Usage (PARTIALLY FIXED)

**Before fix:** word-wrap loop created `std::string line`, `std::string candidate` on stack inside nested while loops, called `drawCenteredText` per line in the same scope. Peak stack frame was larger.

**After fix:** pre-builds `std::vector<std::string> lines` (heap allocation), then renders. This does reduce peak stack frame depth.

**Remaining risk:** The `std::vector<std::string> lines` itself lives on the stack as a local in `render()`. For a 4096-byte quote body broken into many lines, each `std::string` push_back triggers heap allocations (not stack). The vector header is ~24 bytes on stack. This is acceptable — the fix is sound.

**The bigger concern:** render task stack is only **8192 bytes** (Activity.cpp line 23):
```cpp
xTaskCreate(&renderTaskTrampoline, name.c_str(), 8192, this, 1, &renderTaskHandle);
```
8192 bytes = 8KB. FreeRTOS task stacks include the TCB overhead, ISR nesting, and all function call frames. The render() path chains: `renderTaskTrampoline → renderTaskLoop → render() → drawHeader() → drawBatteryRight() → drawBatteryIcon() → drawLine() → drawPixel()`. This is ~6–8 frames deep. If any intermediate buffer (logPrintf's sprintf, vector operations, string construction) exceeds available stack, crash occurs.

**Recommendation:** Increase render task stack to 12288 (12KB) as a safety margin.

---

## Issue 2: Battery Drain — Root Cause Analysis

### 2a. preventAutoSleep() Returns true Unconditionally for Clock, DailyQuote, Badge

All three always-on tools return `true` unconditionally:

| Activity | preventAutoSleep() |
|----------|--------------------|
| ClockActivity | `return true;` — always |
| DailyQuoteActivity | `return true;` — always |
| ConferenceBadgeActivity | `return true;` — always |
| PomodoroActivity | `return state != State::IDLE;` — only when timer running |

**Effect in main.cpp loop():**
```cpp
// main.cpp line 437
if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || (currentActivity && currentActivity->preventAutoSleep())) {
    lastActivityTime = millis();  // Reset inactivity timer every iteration
    powerManager.setPowerSaving(false);  // Keep CPU at full frequency
}
```

When `preventAutoSleep()` returns `true`:
1. `lastActivityTime` is reset every loop iteration → **auto-sleep never triggers**
2. `setPowerSaving(false)` is called every loop → CPU stays at full frequency (80+ MHz) instead of dropping to 10 MHz
3. The sleep timeout check `if (millis() - lastActivityTime >= sleepTimeoutMs)` never fires

The device will **run at full CPU frequency indefinitely** as long as Clock/DailyQuote/Badge is open. The e-paper display stays powered (though it's not refreshing). Combined with the WiFi not being shut down (if previously connected), this can drain a small LiPo battery in hours rather than days.

**Contrast with Pomodoro:** `return state != State::IDLE` — correct. When idle (timer not running), it allows power saving.

**ConferenceBadgeActivity is the most egregious:** it's a static display with no timer, no state change. It returns `true` permanently with no reason.

### 2b. Main Loop Delay Behavior

When `preventAutoSleep() == true`:
- Line 437: `lastActivityTime = millis()` — reset every iteration
- Line 495: `if (millis() - lastActivityTime >= HalPowerManager::IDLE_POWER_SAVING_MS)` — never true (3000ms never elapses because lastActivityTime is refreshed each loop)
- Therefore: `delay(10)` path taken every loop (not `delay(50)`)
- CPU never drops to 10MHz

The loop runs at ~100 Hz with CPU at full speed. For an activity that only needs to update once per minute (Clock), this is 6000x more CPU cycles than necessary.

---

## Specific Code Locations

### Stack overflow (fixed but still fragile)
- `/Users/trilm/DEV/crosspoint-reader/src/activities/Activity.cpp` line 23: stack size 8192
- `/Users/trilm/DEV/crosspoint-reader/lib/GfxRenderer/GfxRenderer.cpp` line 174: LOG_ERR per out-of-bounds pixel

### Battery drain (unresolved)
- `/Users/trilm/DEV/crosspoint-reader/src/activities/tools/ClockActivity.h` line 19: `return true;`
- `/Users/trilm/DEV/crosspoint-reader/src/activities/tools/DailyQuoteActivity.h` line 29: `return true;`
- `/Users/trilm/DEV/crosspoint-reader/src/activities/tools/ConferenceBadgeActivity.h` line 22: `return true;`
- `/Users/trilm/DEV/crosspoint-reader/src/main.cpp` lines 437–440, 495–503

---

## Recommended Fixes (Priority Order)

### P1: Fix Battery Drain — Remove Unconditional preventAutoSleep()

**ClockActivity.h** — Allow sleep; the clock only needs to update on wake:
```cpp
// BEFORE:
bool preventAutoSleep() override { return true; }

// AFTER — allow device to sleep; clock will update on next wake:
bool preventAutoSleep() override { return false; }
```

**DailyQuoteActivity.h** — Static display, no need to keep awake:
```cpp
// BEFORE:
bool preventAutoSleep() override { return true; }

// AFTER:
bool preventAutoSleep() override { return false; }
```

**ConferenceBadgeActivity.h** — Completely static display:
```cpp
// BEFORE:
bool preventAutoSleep() override { return true; }

// AFTER:
bool preventAutoSleep() override { return false; }
```

**Design rationale:** These are "display something and wait for user input" activities. Sleep is fine. On wake (power button press), the activity will re-render with current time/state. The `lastUpdateMs` in ClockActivity already handles the 60s refresh on next `loop()` call.

### P2: Increase Render Task Stack Size

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/Activity.cpp` line 22–27

```cpp
// BEFORE:
xTaskCreate(&renderTaskTrampoline, name.c_str(),
            8192,              // Stack size
            this,              // Parameters
            1,                 // Priority
            &renderTaskHandle  // Task handle
);

// AFTER:
xTaskCreate(&renderTaskTrampoline, name.c_str(),
            12288,             // Stack size (increased from 8192 to handle LOG_ERR + string ops)
            this,              // Parameters
            1,                 // Priority
            &renderTaskHandle  // Task handle
);
```

Cost: 4KB per activity (only one render task active at a time). ESP32-C3 has 400KB SRAM — affordable.

### P3: Guard drawBatteryRight Against Negative X (Defensive)

**File:** `/Users/trilm/DEV/crosspoint-reader/src/components/themes/BaseTheme.cpp`

```cpp
void BaseTheme::drawBatteryRight(const GfxRenderer& renderer, Rect rect, const bool showPercentage) const {
  const uint16_t percentage = powerManager.getBatteryPercentage();
  const int y = rect.y + 6;

  if (showPercentage) {
    const auto percentageText = std::to_string(percentage) + "%";
    const int textWidth = renderer.getTextWidth(SMALL_FONT_ID, percentageText.c_str());
    const int textX = rect.x - textWidth - batteryPercentSpacing;
    // ADD: defensive guard
    if (textX >= 0) {
      const auto textHeight = renderer.getTextHeight(SMALL_FONT_ID);
      renderer.fillRect(textX, rect.y, textWidth, textHeight, false);
      renderer.drawText(SMALL_FONT_ID, textX, rect.y, percentageText.c_str());
    }
  }
  drawBatteryIcon(renderer, rect.x, y, BaseMetrics::values.batteryWidth, rect.height, percentage);
}
```

### P4: Fix requestUpdateAndWait() FIXME

**File:** `/Users/trilm/DEV/crosspoint-reader/src/activities/Activity.cpp` lines 50–53

```cpp
// CURRENT (broken stub):
void Activity::requestUpdateAndWait() {
  // FIXME @ngxson : properly implement this using freeRTOS notification
  delay(100);
}
```

Proper implementation using task notification:
```cpp
void Activity::requestUpdateAndWait() {
  requestUpdate();
  // Wait for render task to signal completion (TODO: requires render task to notify back)
  // Minimal fix: use a semaphore in render task
  delay(200);  // At minimum, increase from 100ms to account for slow e-paper ops
}
```

Full fix requires adding a completion semaphore in `renderTaskLoop()` that signals after `render()` returns. Not critical for the current issues but worth noting.

---

## Impact Assessment

| Fix | Effort | Impact |
|-----|--------|--------|
| P1: Remove preventAutoSleep | Trivial (3 one-liners) | High — eliminates battery drain |
| P2: Increase stack to 12KB | Trivial (1 number change) | Medium — prevents future stack overflows |
| P3: Defensive guard in drawBatteryRight | Small | Low — defensive, not needed post-fix |
| P4: Fix requestUpdateAndWait | Medium | Low — not hitting current symptoms |

---

## Verification Plan

After applying P1 + P2:
1. Open Clock activity, leave idle for >10 min — device should auto-sleep
2. Open DailyQuote, leave idle — device should auto-sleep
3. Open ConferenceBadge — device should auto-sleep
4. Serial log: confirm `setPowerSaving(true)` messages appear after ~3s of no input in Clock/Quote/Badge
5. Pomodoro with timer running: confirm device stays awake; Pomodoro idle: confirm sleeps

---

## Unresolved Questions

1. **Does the issue persist with Clock/DailyQuote after b19258d?** The auto-reboot was tied to the LOG_ERR stack overflow. If users still see reboots post-fix, there may be a second cause (heap fragmentation from `std::vector<std::string>` in DailyQuote, or 8KB stack still too small with some font combinations).

2. **Is the e-paper display fully powered down during device sleep?** `display.deepSleep()` is called in `enterDeepSleep()` — should be fine, but worth confirming the GDEW0102T4 or equivalent panel enters sleep mode.

3. **Battery drain rate measurement?** Would help confirm whether the fix resolves observed drain (from hours to days).

4. **Does ClockActivity need to stay at full CPU during the 60-second wait?** If `preventAutoSleep` stays false, the clock will update correctly after wake, but won't auto-update every 60s while user is watching unless the device is active. If real-time clock display while idle is a requirement, consider a compromise: `return millis() - lastUpdateMs < 1000` (only prevent sleep for 1s after an update triggers).
