# Phase 01 — Snake Game

## Overview
- Priority: High
- Status: done
- Turn-based Snake: each D-pad press = 1 cell move. No timer. Perfect for e-ink.
- Completed: 2026-03-01 (BUILD SUCCESS: RAM 31.3%, Flash 94.2%)

## Architecture

### Grid
- `COLS=30, ROWS=40, CELL_SIZE=16` → game area 480×640px
- `GAME_TOP = metrics.topPadding + metrics.headerHeight`
- 480×(800-80-48)=672px available → 640px used ✓

### State
```cpp
static constexpr int COLS = 30;
static constexpr int ROWS = 40;
static constexpr int CELL_SIZE = 16;

std::deque<std::pair<int8_t, int8_t>> body; // head = front
bool occupied[ROWS][COLS];                   // O(1) collision check
int8_t foodX, foodY;
int8_t dirX = 1, dirY = 0;                  // initial dir: right
bool gameOver = false;
int score = 0;
```

### Logic
- **Initial snake**: length 3, centered, heading right
  - `body = {{15,20},{14,20},{13,20}}`, all marked in `occupied`
- **Move on input**: new head = `(body.front().first + dirX, body.front().second + dirY)`
  - Out of bounds OR `occupied[ny][nx]` → `gameOver = true`, requestUpdate, return
  - New head == food → push front, don't pop tail (grow), score++, spawn food
  - Else → push front, set occupied true; pop tail, set occupied false
- **Direction change**: prevent 180° reversal (`if (newDirX != -dirX || newDirY != -dirY)`)
- **Food spawn**: pick random empty cell using `esp_random()`:
  ```cpp
  void spawnFood() {
    int emptyCount = COLS*ROWS - (int)body.size();
    int pick = (int)(esp_random() % emptyCount);
    int idx = 0;
    for (int y=0; y<ROWS; y++) for (int x=0; x<COLS; x++) {
      if (!occupied[y][x]) { if (idx++ == pick) { foodX=x; foodY=y; return; } }
    }
  }
  ```

### Input (in `loop()`)
```cpp
// Direction buttons — only change dir, don't move yet
bool moved = false;
if (wasReleased(Up)    && dirY != 1) { dirX=0; dirY=-1; moved=true; }
if (wasReleased(Down)  && dirY !=-1) { dirX=0; dirY= 1; moved=true; }
if (wasReleased(Left)  && dirX != 1) { dirX=-1; dirY=0; moved=true; }
if (wasReleased(Right) && dirX !=-1) { dirX= 1; dirY=0; moved=true; }
if (moved && !gameOver) { doMove(); requestUpdate(); }
if (wasReleased(Confirm)) { /* restart */ onEnter(); }
if (wasReleased(Back)) { finish(); }
```

### Render
```cpp
// Header with score
GUI.drawHeader(renderer, ..., "Snake");
char scoreStr[24]; snprintf(scoreStr, sizeof(scoreStr), "Score: %d", score);
renderer.drawCenteredText(SMALL_FONT_ID, ..., scoreStr);

// Draw food as outline square
renderer.drawRect(foodX*CELL_SIZE + px, GAME_TOP + foodY*CELL_SIZE + py, CELL_SIZE, CELL_SIZE);

// Draw snake body as filled squares
for (auto& [x,y] : body)
  renderer.fillRect(x*CELL_SIZE + px, GAME_TOP + y*CELL_SIZE + py, CELL_SIZE-1, CELL_SIZE-1);

// Game over overlay
if (gameOver) renderer.drawCenteredText(UI_10_FONT_ID, pageHeight/2, "Game Over!");

// Button hints
// Back=Exit, Confirm=New Game, dirs show as navigation
renderer.displayBuffer();
```

## Files to Create
- `src/activities/tools/SnakeActivity.h`
- `src/activities/tools/SnakeActivity.cpp`

## Implementation Steps
1. Create `SnakeActivity.h` with class definition, constants, state fields
2. Create `SnakeActivity.cpp` with `onEnter`, `loop`, `doMove`, `spawnFood`, `render`
3. Compile check

## Todo
- [x] Create `SnakeActivity.h`
- [x] Create `SnakeActivity.cpp`
- [x] Compile, fix errors
- [x] Post-review fix: score shown in button hints btn3 slot

## Success Criteria
- Snake moves one cell per button press
- Hitting wall or self → game over message
- Eating food → grows, score increments, new food spawns
- Confirm → restarts game
- Back → exits to Tools menu
- No compile errors

## Risk Assessment
- **`std::deque` on ESP32**: heap-allocated, ~380KB RAM available, deque of 30×40=1200 max cells × 2 bytes = ~2.4KB max body — fine
- **`occupied` array**: 30×40 = 1200 bytes on stack — fine
- **Score overflow**: `int` safe up to 2^31; max score = 1200 (fill grid) — fine

## Notes
- `px = (pageWidth - COLS*CELL_SIZE) / 2` for horizontal centering (will be 0 since 30×16=480)
- `py = 0` since game starts right after header
- Use `CELL_SIZE-1` for cell fill to leave 1px gap (cleaner visual, shows grid structure)
