#include "TwentyFortyEightActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <cstdio>
#include <cstring>
#include <esp_random.h>

#include "components/UITheme.h"
#include "fontIds.h"

// --- Core slide logic ---

// Canonical direction: slide all tiles left within each row.
// Pack non-zero tiles left, then merge equal adjacent pairs (single pass,
// no double-merge). Returns true if any cell changed.
bool TwentyFortyEightActivity::slideLeft() {
  bool changed = false;
  for (int r = 0; r < SIZE; r++) {
    uint32_t row[SIZE] = {};
    int out = 0;

    // Pack non-zero values to the left
    for (int c = 0; c < SIZE; c++) {
      if (grid[r][c]) row[out++] = grid[r][c];
    }

    // Merge adjacent equal pairs (one pass — prevents double-merge)
    for (int i = 0; i < out - 1; i++) {
      if (row[i] == row[i + 1]) {
        row[i] *= 2;
        score += row[i];
        if (row[i] == 2048) won = true;
        // Shift remaining tiles left to fill merged slot
        for (int j = i + 1; j < out - 1; j++) row[j] = row[j + 1];
        row[--out] = 0;
      }
    }

    // Write back and detect change
    for (int c = 0; c < SIZE; c++) {
      if (grid[r][c] != row[c]) changed = true;
      grid[r][c] = row[c];
    }
  }
  return changed;
}

// Slide right: reverse each row, apply slideLeft, reverse back
bool TwentyFortyEightActivity::slideRight() {
  // Reverse rows
  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE / 2; c++) {
      uint32_t tmp = grid[r][c];
      grid[r][c] = grid[r][SIZE - 1 - c];
      grid[r][SIZE - 1 - c] = tmp;
    }
  }
  bool changed = slideLeft();
  // Reverse back
  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE / 2; c++) {
      uint32_t tmp = grid[r][c];
      grid[r][c] = grid[r][SIZE - 1 - c];
      grid[r][SIZE - 1 - c] = tmp;
    }
  }
  return changed;
}

// Transpose grid in-place (swap rows/columns)
static void transposeGrid(uint32_t grid[4][4]) {
  for (int r = 0; r < 4; r++) {
    for (int c = r + 1; c < 4; c++) {
      uint32_t tmp = grid[r][c];
      grid[r][c] = grid[c][r];
      grid[c][r] = tmp;
    }
  }
}

// Slide up: transpose → slideLeft → transpose back
bool TwentyFortyEightActivity::slideUp() {
  transposeGrid(grid);
  bool changed = slideLeft();
  transposeGrid(grid);
  return changed;
}

// Slide down: transpose → slideRight → transpose back
bool TwentyFortyEightActivity::slideDown() {
  transposeGrid(grid);
  bool changed = slideRight();
  transposeGrid(grid);
  return changed;
}

// --- Spawn a tile (2 with 90% probability, 4 with 10%) ---
void TwentyFortyEightActivity::spawnTile() {
  int empty = 0;
  for (int r = 0; r < SIZE; r++)
    for (int c = 0; c < SIZE; c++)
      if (!grid[r][c]) empty++;
  if (!empty) return;

  int pick = (int)(esp_random() % (uint32_t)empty);
  uint32_t val = (esp_random() % 10 == 0) ? 4u : 2u;

  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE; c++) {
      if (!grid[r][c] && pick-- == 0) {
        grid[r][c] = val;
        return;
      }
    }
  }
}

// --- Check if any move is still possible ---
bool TwentyFortyEightActivity::hasMovesLeft() const {
  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE; c++) {
      if (!grid[r][c]) return true;                              // empty cell
      if (c + 1 < SIZE && grid[r][c] == grid[r][c + 1]) return true;  // horizontal merge
      if (r + 1 < SIZE && grid[r][c] == grid[r + 1][c]) return true;  // vertical merge
    }
  }
  return false;
}

// --- Lifecycle ---

void TwentyFortyEightActivity::onEnter() {
  Activity::onEnter();
  memset(grid, 0, sizeof(grid));
  score = 0;
  gameOver = false;
  won = false;
  // Start with two tiles
  spawnTile();
  spawnTile();
  requestUpdate();
}

void TwentyFortyEightActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onEnter();
    return;
  }

  if (gameOver) return;

  bool moved = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Left))       moved = slideLeft();
  else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) moved = slideRight();
  else if (mappedInput.wasReleased(MappedInputManager::Button::Up))    moved = slideUp();
  else if (mappedInput.wasReleased(MappedInputManager::Button::Down))  moved = slideDown();

  if (moved) {
    spawnTile();
    if (!hasMovesLeft()) gameOver = true;
    requestUpdate();
  }
}

// --- Render ---

void TwentyFortyEightActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  // Grid layout: 4*108 + 3*8 = 456px wide; center in 480px → left offset = 12
  const int gridLeft = (pageWidth - (SIZE * TILE + (SIZE - 1) * GAP)) / 2;
  // Score row: 48px below header
  const int scoreY = metrics.topPadding + metrics.headerHeight + 12;
  const int gridTop = metrics.topPadding + metrics.headerHeight + 48;

  renderer.clearScreen();

  // Header
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_2048));

  // Score line
  char scoreStr[32];
  snprintf(scoreStr, sizeof(scoreStr), "Score: %lu", (unsigned long)score);
  renderer.drawCenteredText(SMALL_FONT_ID, scoreY, scoreStr);

  // Tile grid
  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE; c++) {
      const int x = gridLeft + c * (TILE + GAP);
      const int y = gridTop + r * (TILE + GAP);

      // Always draw tile border
      renderer.drawRect(x, y, TILE, TILE);

      if (grid[r][c]) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)grid[r][c]);

        // Center text within tile using getTextWidth + getLineHeight
        const int textW = renderer.getTextWidth(UI_10_FONT_ID, buf);
        const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
        renderer.drawText(UI_10_FONT_ID,
                          x + (TILE - textW) / 2,
                          y + (TILE - lineH) / 2,
                          buf);
      }
    }
  }

  // Overlay messages
  if (won && !gameOver) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, "You Win!");
  }
  if (gameOver) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Game Over!");
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NEW_GAME), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
