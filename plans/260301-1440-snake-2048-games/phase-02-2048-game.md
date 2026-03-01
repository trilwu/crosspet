# Phase 02 — 2048 Game + ToolsActivity Integration

## Overview
- Priority: High
- Status: done
- 4×4 sliding tile game. Input = swipe direction. Render only on change.
- Completed: 2026-03-01 (BUILD SUCCESS: RAM 31.3%, Flash 94.2%)

## Architecture

### Grid Geometry
- `TILE=108, GAP=8` → grid width = 4×108 + 3×8 = 456px, left offset = (480-456)/2 = 12px
- Score area: 40px below header → `GRID_TOP = metrics.topPadding + metrics.headerHeight + 48`
- Grid height = 456px; total used = 80 + 48 + 456 = 584px (fits in 800px, leaves 168px for hints + margin)

### State
```cpp
static constexpr int SIZE = 4;
static constexpr int TILE = 108;
static constexpr int GAP = 8;

uint16_t grid[SIZE][SIZE];  // 0 = empty
uint32_t score = 0;
bool gameOver = false;
bool won = false;           // reached 2048
```

### Core Logic

**Slide + merge in one direction (example: left):**
```cpp
bool slideLeft() {
  bool changed = false;
  for (int r = 0; r < SIZE; r++) {
    uint16_t row[SIZE] = {};
    int out = 0;
    // Pack non-zero tiles
    for (int c = 0; c < SIZE; c++)
      if (grid[r][c]) row[out++] = grid[r][c];
    // Merge adjacent equals (one pass, no double-merge)
    for (int i = 0; i < out - 1; i++) {
      if (row[i] == row[i+1]) {
        row[i] *= 2; score += row[i];
        if (row[i] == 2048) won = true;
        for (int j = i+1; j < out-1; j++) row[j] = row[j+1];
        row[--out] = 0;
      }
    }
    // Write back
    for (int c = 0; c < SIZE; c++) {
      if (grid[r][c] != row[c]) changed = true;
      grid[r][c] = row[c];
    }
  }
  return changed;
}
```
- `slideRight`: reverse each row, slideLeft, reverse back
- `slideUp`: transpose, slideLeft, transpose
- `slideDown`: transpose, slideRight, transpose

**Spawn tile:**
```cpp
void spawnTile() {
  // count empties
  int empty = 0;
  for (auto& row : grid) for (auto v : row) if (!v) empty++;
  if (!empty) return;
  int pick = esp_random() % empty;
  uint16_t val = (esp_random() % 10 == 0) ? 4 : 2;
  for (int r = 0; r < SIZE; r++) for (int c = 0; c < SIZE; c++)
    if (!grid[r][c] && pick-- == 0) { grid[r][c] = val; return; }
}
```

**Game over check:**
```cpp
bool hasMovesLeft() {
  for (int r=0;r<SIZE;r++) for (int c=0;c<SIZE;c++) {
    if (!grid[r][c]) return true;
    if (c+1<SIZE && grid[r][c]==grid[r][c+1]) return true;
    if (r+1<SIZE && grid[r][c]==grid[r+1][c]) return true;
  }
  return false;
}
```

### Input (`loop()`)
```cpp
bool moved = false;
if (wasReleased(Left))  moved = slideLeft();
if (wasReleased(Right)) moved = slideRight();
if (wasReleased(Up))    moved = slideUp();
if (wasReleased(Down))  moved = slideDown();
if (moved) { spawnTile(); if (!hasMovesLeft()) gameOver=true; requestUpdate(); }
if (wasReleased(Confirm)) { /* new game */ onEnter(); }
if (wasReleased(Back)) { finish(); }
```

### Render
```cpp
// Header
GUI.drawHeader(renderer, ..., "2048");

// Score line
char scoreStr[32]; snprintf(scoreStr, sizeof(scoreStr), "Score: %lu", (unsigned long)score);
renderer.drawCenteredText(SMALL_FONT_ID, metrics.topPadding+metrics.headerHeight+12, scoreStr);

// Tiles
const int left = (pageWidth - (SIZE*TILE + (SIZE-1)*GAP)) / 2;
for (int r=0; r<SIZE; r++) for (int c=0; c<SIZE; c++) {
  int x = left + c*(TILE+GAP);
  int y = GRID_TOP + r*(TILE+GAP);
  renderer.drawRect(x, y, TILE, TILE);        // always draw border
  if (grid[r][c]) {
    char buf[8]; snprintf(buf, sizeof(buf), "%u", grid[r][c]);
    renderer.drawCenteredTextInRect(UI_10_FONT_ID, x, y, TILE, TILE, buf);
    // Fallback if no drawCenteredTextInRect: compute manually
    // renderer.drawCenteredText(UI_10_FONT_ID, y + TILE/2 - lineH/2, buf);
  }
}

// Overlay messages
if (won && !gameOver)
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight/2, "You Win! Keep going...");
if (gameOver)
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight/2, "Game Over!");

// Hints
const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NEW_GAME), "", "");
GUI.drawButtonHints(...);
renderer.displayBuffer();
```

**Note on text centering in tiles:** If `drawCenteredTextInRect` doesn't exist, use `renderer.drawCenteredText()` with Y = tile_center_y. The tile is wide enough that `drawCenteredText` (centered on screen width) won't align per-tile. Use `renderer.drawText()` with manually computed X:
```cpp
int textW = renderer.getTextWidth(UI_10_FONT_ID, buf);
renderer.drawText(UI_10_FONT_ID, x + (TILE-textW)/2, y + (TILE-lineH)/2, buf);
```
Check `GfxRenderer` API — `getTextWidth` likely exists given existing usage.

## ToolsActivity Integration

### `ToolsActivity.h`
```cpp
static constexpr int MENU_COUNT = 10;  // was 8
```

### `ToolsActivity.cpp`
Add includes:
```cpp
#include "SnakeActivity.h"
#include "TwentyFortyEightActivity.h"
```
Add switch cases:
```cpp
case 8:
  activityManager.pushActivity(std::make_unique<SnakeActivity>(renderer, mappedInput));
  break;
case 9:
  activityManager.pushActivity(std::make_unique<TwentyFortyEightActivity>(renderer, mappedInput));
  break;
```
Extend `menuLabels[]`:
```cpp
const char* menuLabels[] = {
  tr(STR_CLOCK), tr(STR_POMODORO), tr(STR_DAILY_QUOTE), tr(STR_CONFERENCE_BADGE),
  tr(STR_VIRTUAL_PET), tr(STR_PHOTO_FRAME), tr(STR_MAZE_GAME), tr(STR_GAME_OF_LIFE),
  tr(STR_SNAKE), tr(STR_2048)
};
```

## I18n Strings

### `lib/I18n/translations/english.yaml`
```yaml
STR_SNAKE: "Snake"
STR_2048: "2048"
STR_NEW_GAME: "New Game"   # reuse if already exists, else add
```

### `lib/I18n/translations/vietnamese.yaml`
```yaml
STR_SNAKE: "Rắn"
STR_2048: "2048"
STR_NEW_GAME: "Chơi lại"
```

## Files to Create/Modify
- **Create**: `src/activities/tools/TwentyFortyEightActivity.h`
- **Create**: `src/activities/tools/TwentyFortyEightActivity.cpp`
- **Modify**: `src/activities/tools/ToolsActivity.h` (MENU_COUNT 8→10)
- **Modify**: `src/activities/tools/ToolsActivity.cpp` (includes, cases, labels)
- **Modify**: `lib/I18n/translations/english.yaml`
- **Modify**: `lib/I18n/translations/vietnamese.yaml`

## Implementation Steps
1. Create `TwentyFortyEightActivity.h`
2. Create `TwentyFortyEightActivity.cpp` (slideLeft + 3 transforms + spawnTile + render)
3. Update `ToolsActivity.h/.cpp`
4. Update I18n YAML files
5. Full compile check

## Todo
- [x] Create `TwentyFortyEightActivity.h`
- [x] Create `TwentyFortyEightActivity.cpp`
- [x] Verify `GfxRenderer` API for text width (check `drawText` signature)
- [x] Update `ToolsActivity.h` MENU_COUNT (8→10)
- [x] Update `ToolsActivity.cpp` includes + cases + labels
- [x] Update I18n YAML (STR_SNAKE, STR_2048 added)
- [x] Compile, fix errors
- [x] Post-review fix: else-if chain to prevent double-mutation

## Success Criteria
- 4×4 grid renders with tile values
- All 4 swipe directions work correctly (no double-merge bug)
- Random tile spawns after each valid move
- Game over detected when no moves remain
- Win message when 2048 reached (continue allowed)
- Score accumulates correctly
- Confirm → new game, Back → exit
- Appears in Tools menu at index 9
- No compile errors, build succeeds

## Risk Assessment
- **Slide logic correctness**: The pack→merge→writeback pattern avoids double-merge. Test edge cases: two pairs in a row `[2,2,2,2]→[4,4,0,0]` ✓; three in a row `[2,2,2,0]→[4,2,0,0]` ✓
- **Text rendering in tiles**: Small numbers (2,4,8) are fine. "2048","4096","8192" may need SMALL_FONT_ID instead of UI_10_FONT_ID for larger values — check font width vs TILE=108
- **`STR_NEW_GAME`**: May already exist; check english.yaml before adding

## Notes
- `uint16_t` max = 65535; max tile in 2048 = 131072 which overflows uint16. If user wants to play past 2048 to 4096/8192/16384/32768/65536 that's fine; 131072 would overflow. Acceptable — winning at 2048 is the standard goal. Or use `uint32_t` to be safe (costs 16 extra bytes for the grid — negligible).
- Actually, use `uint32_t grid[4][4]` to be safe against super-play. Only 64 bytes.
