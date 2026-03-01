# Code Review: Fun Features (PhotoFrame, Maze, GameOfLife)

**Date:** 2026-03-01
**Files reviewed:** 8 files, ~530 LOC
**Build:** PASS (RAM 31.3%, Flash 94.1%)

---

## Scope

| File | LOC | Notes |
|------|-----|-------|
| `PhotoFrameActivity.h/.cpp` | 30 + 140 | BMP slideshow |
| `MazeGameActivity.h/.cpp` | 35 + 147 | Iterative DFS maze game |
| `GameOfLifeActivity.h/.cpp` | 44 + 144 | Bit-packed Conway's GoL |
| `ToolsActivity.h/.cpp` | 18 + 90 | Menu integration (cases 5-7) |

---

## Overall Assessment

High quality. All three activities are correct, memory-safe, and follow established project patterns. The algorithmic implementations are particularly solid. Issues found are minor or cosmetic.

---

## Critical Issues

None.

---

## High Priority

### H1 — GoL at 200ms may saturate e-ink display

`GameOfLifeActivity` auto-steps every 200ms at max speed. E-ink displays typically require 200-500ms per full update cycle. `renderer.displayBuffer()` (no arg = default refresh) is called each step.

At 200ms, the render task (via FreeRTOS `ulTaskNotifyTake`) will collapse rapid notifications, but the display hardware itself may not complete its refresh before the next frame is pushed. This can cause visible ghosting or corruption on the `current` cells display.

The existing `BmpViewerActivity` and `SleepActivity` use `FULL_REFRESH` for bitmaps; other activities use `HALF_REFRESH`. GoL uses bare `displayBuffer()`.

**Recommendation:** Either enforce a minimum 400-500ms floor on `SPEED_MS[0]`, or use `FAST_REFRESH` mode if available and documented. The current 200ms value is optimistic for e-ink hardware.

---

## Medium Priority

### M1 — PhotoFrame: no hidden-file filter in `scanPhotos()`

`scanPhotos()` adds all `.bmp` files but doesn't skip `._*.bmp` or `.DS_Store`-adjacent files (macOS artifact files starting with `._`). If a Mac user copies photos to the SD card, `._photo.bmp` files will be scanned and cause `Storage.openFileForRead()` to fail silently, showing a blank screen for that slot.

`SleepActivity::renderCustomSleepScreen()` filters `fname[0] == '.'` — the same guard should be applied here.

**Fix (3 lines in `scanPhotos()`):**
```cpp
// After: f.getName(name, sizeof(name));
std::string fname(name);
if (fname.empty() || fname[0] == '.') { f.close(); continue; }
```

### M2 — PhotoFrame: case-sensitive `.bmp` extension check

The filter `fname.substr(fname.size() - 4) == ".bmp"` misses `.BMP` (uppercase). This is a pre-existing pattern also present in `SleepActivity`. On FAT32 SD cards, the filesystem may return uppercase extensions. Low risk if the user controls the files, but worth documenting as a known limitation.

**Note:** SleepActivity has the same issue — not regretting in this review.

### M3 — Maze: `requestUpdate()` fired even when move is wall-blocked

In `MazeGameActivity::loop()`, `moved = true` is set unconditionally after `wasReleased()` checks, regardless of whether `tryMove()` actually moved the player (it silently no-ops on wall collision).

```cpp
if (mappedInput.wasReleased(Button::Right)) { tryMove(1, 0, 1); moved = true; }
```

This triggers a full e-ink redraw for button presses that produce no visual change. On e-ink this is a meaningful penalty (~200ms update blocking input).

**Fix:** Have `tryMove()` return `bool` and only set `moved = true` on successful movement:
```cpp
bool moved = false;
if (mappedInput.wasReleased(Button::Right)) moved |= tryMove(1, 0, 1);
```

---

## Low Priority

### L1 — `millis()` overflow in `overlayEndMs` (PhotoFrame)

```cpp
overlayEndMs = millis() + 2000;
// ...
if (showOverlay && millis() >= overlayEndMs) { ... }
```

If `millis()` is within 2 seconds of `0xFFFFFFFF` (49.7-day uptime), `overlayEndMs` wraps to a small value and the condition `millis() >= overlayEndMs` is immediately true, causing the overlay to flash and vanish instantly.

The auto-advance comparison `(millis() - lastAdvanceMs) >= interval` is correctly overflow-safe (unsigned subtraction). The overlay comparison is not.

**Impact:** Cosmetic only. Effectively impossible in normal use.

**Safe alternative:** `if (showOverlay && (millis() - overlayStartMs) >= 2000)` (store start time instead of end time).

### L2 — Maze wall-check for border rendering is redundant but harmless

```cpp
for (int x = 0; x < COLS; x++) {
  if (hasWall(x, ROWS - 1, 2))  // S wall of bottom row
```

DFS backtracker never crosses grid bounds (bounds-checked before visiting), so border cells always retain their outer walls. The `hasWall()` check is always true for these border walls. Drawing them unconditionally would be equivalent and slightly cleaner.

**Impact:** None — correctness is unaffected.

### L3 — Large photo library heap growth

`std::vector<std::string> photos` in `PhotoFrameActivity` has no upper bound. If `/photos/` contains thousands of `.bmp` files, this could exhaust the ESP32's ~200KB free heap. With current RAM at 31.3% (before activity allocation), this is low risk but worth noting.

---

## Positive Observations

- **Maze DFS is correct.** Direction table (`DX`, `DY`, `OPP`), wall bit encoding (bit0=N, bit1=E, bit2=S, bit3=W), iterative backtracker, and visited tracking are all implemented without errors. The Fisher-Yates shuffle using `esp_random()` is correct.

- **Conway's rules are correct.** Alive: survive on 2 or 3 neighbors. Dead: born on exactly 3. Double-buffer via pointer swap is clean, toroidal boundary wrapping is standard.

- **Bit packing is consistent.** `getCell` and `setCell` use identical LSB-first bit addressing (`x % 8` bit index). No off-by-one in `BYTES_PER_ROW = (COLS + 7) / 8`.

- **Screen geometry verified.** Maze fits 480x698px within 480x800 portrait screen, leaving 102px for button hints (40px needed). GoL grid uses 480x720px, 80px HUD below — fits cleanly.

- **Memory footprint is acceptable.** `MazeGameActivity` object: ~1080 bytes (walls + visited arrays). `GameOfLifeActivity` object: ~2400 bytes (two 1200-byte grids). DFS heap stack worst case ~4.3KB. All allocated on heap via `make_unique`. No stack overflow risk.

- **`Bitmap::parseHeaders()` guards zero dimensions** via `BadDimensions` error code — the division-by-zero risk in PhotoFrame's aspect ratio calculation is fully mitigated.

- **`snprintf` bounds are correct.** `info[64]` in PhotoFrame overlay, `status[40]` in Maze, `genStr[24]` in GoL — all large enough for their format strings' maximum output.

- **`FsFile` lifecycle is correct** in PhotoFrame: file opened, bitmap read, `file.close()` called in all code paths within render().

- **I18n strings** (`STR_PHOTO_FRAME`, `STR_MAZE_GAME`, `STR_GAME_OF_LIFE`, `STR_NEW_MAZE`) are defined in `english.yaml`. No missing string keys.

- **ToolsActivity MENU_COUNT=8 matches** 8 entries in `menuLabels[]` and 8 switch cases (0-7). No off-by-one.

- **Render throttling is safe.** FreeRTOS `ulTaskNotifyTake(pdTRUE)` collapses burst `requestUpdate()` calls into a single render. No render storm possible.

- **`Activity::loop()` base not called** in the new activities — correct, since the base is an empty virtual (`{}`). Consistent with ClockActivity and PomodoroActivity.

---

## Recommended Actions (Priority Order)

1. **(H1)** Raise `SPEED_MS[0]` from 200ms to 400-500ms, or switch GoL to use `FAST_REFRESH`/`HALF_REFRESH` display mode to mitigate e-ink ghosting.
2. **(M1)** Add `fname[0] == '.'` guard in `PhotoFrameActivity::scanPhotos()` to skip hidden/system files.
3. **(M3)** Make `tryMove()` return `bool` and only call `requestUpdate()` on actual player movement.
4. **(L1)** Replace `overlayEndMs` with `overlayStartMs` + duration comparison for overflow-safe overlay timing.

---

## Metrics

- Type Coverage: N/A (C++ with Arduino)
- Test Coverage: N/A (no unit test infrastructure for activities)
- Linting Issues: 0 (build passed clean)
- File size compliance: All files <= 147 LOC (within 200-line rule)

---

## Unresolved Questions

1. Does `renderer.displayBuffer()` (no argument) default to `FAST_REFRESH`, `HALF_REFRESH`, or `FULL_REFRESH`? The answer determines whether GoL ghosting (H1) is an actual problem or not.
2. On this SD card/filesystem implementation, does `file.getName()` ever return a path-prefixed name (e.g., `/photos/img.bmp`) rather than just the filename? If so, the PhotoFrame path construction `/photos/` + name would double the prefix.
