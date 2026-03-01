# Code Review: Tools Menu Implementation

## Scope
- Files reviewed: 12 new + 6 modified (see list below)
- New: `QRCodeUtil.h/cpp`, `ToolsActivity.h/cpp`, `ClockActivity.h/cpp`, `PomodoroActivity.h/cpp`, `DailyQuoteActivity.h/cpp`, `ConferenceBadgeActivity.h/cpp`
- Modified: `english.yaml`, `BaseTheme.h`, `LyraTheme.cpp`, `HomeActivity.h/cpp`, `main.cpp`, `CrossPointWebServerActivity.cpp`
- Review focus: compile errors, logic errors, memory safety, codebase consistency

---

## Overall Assessment

Implementation is clean and well-structured. Follows existing codebase patterns consistently. No critical issues. **Two compile errors found** (one near-certain, one probable), plus several medium-priority logic issues.

---

## Critical Issues

None.

---

## High Priority Findings

### H1 — COMPILE ERROR: `ToolsActivity::render` — `menuIcons` vector uses undeclared `Tools` (unqualified)

**File:** `src/activities/tools/ToolsActivity.cpp`, line 62

```cpp
std::vector<UIIcon> menuIcons = {Tools, Tools, Tools, Tools};
```

`UIIcon::Tools` is declared in `BaseTheme.h` as part of the `enum UIIcon { ..., Tools }`. The enum is not scoped (`enum class`), so plain `Tools` should be accessible — **however**, the include chain must reach `BaseTheme.h`. The `.cpp` includes `components/UITheme.h`, which presumably includes `BaseTheme.h`. Verify `UITheme.h` pulls in `BaseTheme.h` transitively.

More critically: the `menuIcons` vector is **never used**. `drawList` is called without an icon lambda:

```cpp
GUI.drawList(
    renderer, Rect{...}, MENU_COUNT, selectorIndex,
    [&menuLabels](int index) { return menuLabels[index]; });
    // ^ no icon lambda passed
```

The `menuIcons` vector is dead code. This is not a compile error but a logic gap — the Tools submenu has no icons unlike the Home menu. Either the icons are intentionally omitted (fine, but remove the dead vector) or the icon lambda was forgotten.

**Fix:** Remove the dead `menuIcons` vector, or pass it as the icon lambda:
```cpp
// Option A: remove dead code
// (delete the menuIcons line)

// Option B: pass icons
GUI.drawList(renderer, Rect{...}, MENU_COUNT, selectorIndex,
    [&menuLabels](int index) { return menuLabels[index]; },
    nullptr,  // rowSubtitle
    [&menuIcons](int index) { return menuIcons[index]; });
```

---

### H2 — COMPILE ERROR (probable): `QRCodeUtil::draw` takes `const GfxRenderer&` but `fillRect` is `const`

**File:** `src/util/QRCodeUtil.h`, line 8

```cpp
void draw(const GfxRenderer& renderer, int x, int y, const std::string& data);
```

Looking at `GfxRenderer.h`:
```cpp
void fillRect(int x, int y, int width, int height, bool state = true) const;
```

`fillRect` is `const` on `GfxRenderer`, so calling it on a `const GfxRenderer&` is valid. **No compile error here** — this is fine.

---

### H3 — COMPILE ERROR: `DailyQuoteActivity.cpp` line 47 — out-of-bounds read on `line[2]`

**File:** `src/activities/tools/DailyQuoteActivity.cpp`, line 47

```cpp
if ((line.size() >= 2 && line[0] == '-' && line[1] == '-' && line[2] != '-') ||
```

When `line.size() == 2`, `line[2]` is out of bounds (UB / potential crash). The condition checks `size() >= 2` but then accesses index 2 which requires `size() >= 3`.

**Fix:**
```cpp
if ((line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] != '-') ||
```

---

## Medium Priority Findings

### M1 — Logic: `HomeActivity::getMenuItemCount` hardcodes count=5 but comment is wrong

**File:** `src/activities/home/HomeActivity.cpp`, line 23

```cpp
int count = 5;  // My Library, Recents, File transfer, Tools, Settings
```

The menu has 5 items: Browse Files, Recent Books, File Transfer, Tools, Settings. The comment lists only 4. Minor, but misleading. Not a bug — the count is correct.

---

### M2 — Logic: `ConferenceBadgeActivity::render` uses hardcoded pixel positions

**File:** `src/activities/tools/ConferenceBadgeActivity.cpp`, lines 82, 87, 94

```cpp
const int nameY = 250;
...
const int titleY = nameY + renderer.getLineHeight(UI_12_FONT_ID) + 20;
...
const int qrY = 420;
```

`nameY = 250` and `qrY = 420` are hardcoded for a specific display height (800px). If the display height varies, layout breaks. All other activities derive positions from `pageHeight`. Should use:
```cpp
const int nameY = pageHeight / 2 - 100;  // or similar metric-derived value
const int qrY = pageHeight - QRCodeUtil::SIZE - metrics.buttonHintsHeight - metrics.verticalSpacing;
```

---

### M3 — Logic: `ClockActivity::loop` — clock only refreshes once per minute but re-checks time validity every loop iteration (tight polling when no time)

**File:** `src/activities/tools/ClockActivity.cpp`, lines 38-41

```cpp
if (!timeAvailable) {
    timeAvailable = isTimeValid();
    if (timeAvailable) requestUpdate();
}
```

`getLocalTime` is called every loop iteration (every ~10-50ms) when time is not available. On an ESP32-C3 this is cheap but wasteful. Low severity given the embedded context, but worth noting.

---

### M4 — Logic: `PomodoroActivity` — inverted Up/Down direction for duration adjustment

**File:** `src/activities/tools/PomodoroActivity.cpp`, lines 99-113

```cpp
buttonNavigator.onNext([this] {   // Down/Right buttons
    if (focusDurationMs > 5 * 60 * 1000) {
        focusDurationMs -= 5 * 60 * 1000;  // Down = decrease
        ...
    }
});
buttonNavigator.onPrevious([this] {  // Up/Left buttons
    if (focusDurationMs < 60 * 60 * 1000) {
        focusDurationMs += 5 * 60 * 1000;  // Up = increase
        ...
    }
});
```

`onNext` maps to Down/Right and decreases duration; `onPrevious` maps to Up/Left and increases duration. The button hints display `STR_DIR_UP` for btn3 (Up) and `STR_DIR_DOWN` for btn4 (Down):

```cpp
btn3 = tr(STR_DIR_UP);
btn4 = tr(STR_DIR_DOWN);
```

This is consistent with `ButtonNavigator::onPrevious` = Up and `onNext` = Down (confirmed in `ButtonNavigator.h`). The UX question is: does Up = increase duration feel natural? This is a design choice, not a bug. Verify intentionality.

---

### M5 — Logic: `DailyQuoteActivity::render` — word-wrap loop has potential infinite loop

**File:** `src/activities/tools/DailyQuoteActivity.cpp`, lines 133-155

If a single word is wider than `maxWidth` (e.g., a very long URL with no spaces), the inner `while (*ptr)` loop will never append the word to `line` (because `candidate` would exceed `maxWidth`), but it also never advances `ptr`. The outer `if (line.empty()) break;` only fires if `line` is empty at line-end, not if `ptr` is stuck mid-word.

Trace through: `line` is empty, `candidate = word`, `renderer.getTextWidth(..., candidate) > maxWidth`, condition on line 144 is `!line.empty() && width > maxWidth` = `false && ... = false`, so `line = candidate`, `ptr = nextSpace`. Actually this is fine — the `!line.empty()` guard means a word that's too wide just gets added anyway. No infinite loop. **False alarm — retracting this.**

---

### M6 — Memory: `DailyQuoteActivity` — 4KB stack allocation in `onEnter`

**File:** `src/activities/tools/DailyQuoteActivity.cpp`, line 72

```cpp
char buffer[MAX_FILE_SIZE];  // MAX_FILE_SIZE = 4096
```

4KB stack allocation is significant on ESP32-C3 (default stack is typically 8KB for tasks). If the rendering task has a smaller stack, this could cause a stack overflow. Check task stack size for the rendering task.

Recommend: use `std::unique_ptr<char[]>` or heap allocation:
```cpp
auto buffer = std::make_unique<char[]>(MAX_FILE_SIZE);
size_t bytesRead = Storage.readFileToBuffer(QUOTES_FILE, buffer.get(), MAX_FILE_SIZE);
```

---

## Low Priority Suggestions

### L1 — `ToolsActivity::render`: dead `menuIcons` vector (same as H1 — remove it)

### L2 — `ClockActivity` uses hardcoded English day/month names

**File:** `src/activities/tools/ClockActivity.cpp`, lines 12-15

```cpp
const char* DAY_NAMES[] = {"Sunday", "Monday", ...};
const char* MONTH_NAMES[] = {"January", "February", ...};
```

These are not localized. Not a compile error, but inconsistent with the I18n pattern used elsewhere. Low priority for an MVP.

### L3 — `ConferenceBadgeActivity` does not reset `badgeName`/`badgeTitle`/`qrData` on re-enter

If `onEnter` is called a second time (user goes back and re-opens badge), the old data persists. The `fileLoaded = false` is not reset either (actually it starts false but never resets). If the file is removed between visits, stale data shows. Consider resetting state in `onEnter`:
```cpp
badgeName.clear(); badgeTitle.clear(); qrData.clear(); fileLoaded = false;
```

### L4 — `PomodoroActivity::onEnter` does not reset `pausedElapsedMs` or `lastRenderMs`

Already resets `pausedElapsedMs = 0` (line 63). `lastRenderMs` is not reset but this is harmless since it's only used when running. Fine.

### L5 — `main.cpp` forward-declaration ordering

`onGoToTools` is forward-declared on line 260, then `onGoToClock`/`onGoToPomodoro`/`onGoToDailyQuote`/`onGoToBadge` follow (lines 262-280), then `onGoToTools` is defined (line 282). This works because C++ only requires declaration before use, but the pattern is slightly inconsistent with how other navigation functions are arranged (defined before use without forward declaration). Minor style point.

---

## Positive Observations

1. **QRCodeUtil extraction** is clean — single-responsibility util, correct const-correctness on `GfxRenderer&`.
2. **PomodoroActivity timer logic** is solid — handles pause/resume with accumulated elapsed time correctly, avoids timer drift.
3. **DailyQuoteActivity parser** handles `\r\n`, em-dash attribution, and the trailing-entry case correctly.
4. **ConferenceBadgeActivity file parser** properly skips `\r` for Windows line endings.
5. **HomeActivity wiring** — the dynamic index calculation for menu items (`menuSelectedIndex = selectorIndex - recentBooks.size()`) correctly handles the variable number of recent books before fixed menu items.
6. **All new activities** call `Activity::onEnter()` as first statement in `onEnter()` overrides — correct.
7. **`preventAutoSleep()`** added to `ClockActivity` and `ConferenceBadgeActivity` — appropriate for always-on displays.
8. I18n strings are complete — all 17 new strings present and used.

---

## Recommended Actions (priority order)

1. **Fix H3 (compile/UB):** Change `line.size() >= 2` to `line.size() >= 3` in `DailyQuoteActivity.cpp:47`.
2. **Fix H1 (dead code + potential missing icons):** Remove dead `menuIcons` vector from `ToolsActivity::render`, or wire it into the `drawList` call.
3. **Fix M2:** Replace hardcoded `nameY=250` and `qrY=420` in `ConferenceBadgeActivity` with `pageHeight`-relative values.
4. **Fix M6:** Change `char buffer[4096]` in `DailyQuoteActivity::onEnter` to heap allocation.
5. **Fix L3:** Reset badge fields in `ConferenceBadgeActivity::onEnter`.

---

## Metrics
- Type Coverage: N/A (C++ with no static analysis run)
- Test Coverage: N/A
- Linting Issues: 1 UB (H3), 1 dead code (H1), 2 hardcoded layout values (M2)

---

## Unresolved Questions

- Is the Tools submenu intentionally icon-free? If so, remove the dead `menuIcons` vector in `ToolsActivity.cpp:62`.
- What is the stack size of the rendering/activity task? The 4KB stack buffer in `DailyQuoteActivity` may be safe or dangerous depending on this.
- `PomodoroActivity` Up=increase/Down=decrease duration: confirm this is the intended UX direction.
