#include "TwentyFortyEightActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <cstdio>
#include <cstring>
#include <esp_random.h>

#include "GameScores.h"
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
  // Persist best score before resetting for a new game
  if (score > GAME_SCORES.best2048) {
    GAME_SCORES.best2048 = score;
    GAME_SCORES.saveToFile();
  }
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

  // Score + best score on one line
  char scoreStr[48];
  const uint32_t bestScore = (score > GAME_SCORES.best2048) ? score : GAME_SCORES.best2048;
  snprintf(scoreStr, sizeof(scoreStr), tr(STR_SCORE_FORMAT), (unsigned long)score, (unsigned long)bestScore);
  renderer.drawCenteredText(SMALL_FONT_ID, scoreY, scoreStr);

  // Tile grid with value-based fill patterns for visual distinction
  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE; c++) {
      const int x = gridLeft + c * (TILE + GAP);
      const int y = gridTop + r * (TILE + GAP);
      const uint32_t val = grid[r][c];

      // Draw tile border
      renderer.drawRect(x, y, TILE, TILE);

      // Fill pattern based on tile value (e-ink monochrome: use dot density)
      if (val >= 128) {
        // High values: fill background, draw inverted text
        bool inverted = (val >= 512);  // solid fill for 512+
        if (inverted) {
          renderer.fillRect(x + 1, y + 1, TILE - 2, TILE - 2);
        } else {
          // 128-256: cross-hatch pattern (every 3rd pixel)
          for (int py = y + 1; py < y + TILE - 1; py++) {
            for (int px = x + 1; px < x + TILE - 1; px++) {
              if ((px + py) % 3 == 0) renderer.drawPixel(px, py, true);
            }
          }
        }
      } else if (val >= 32) {
        // 32-64: sparse dot pattern (every 4th pixel in checkerboard)
        for (int py = y + 2; py < y + TILE - 2; py += 4) {
          for (int px = x + 2; px < x + TILE - 2; px += 4) {
            renderer.drawPixel(px, py, true);
          }
        }
      } else if (val >= 8) {
        // 8-16: very sparse dots (corners and edges hint)
        for (int py = y + 4; py < y + TILE - 4; py += 8) {
          for (int px = x + 4; px < x + TILE - 4; px += 8) {
            renderer.drawPixel(px, py, true);
          }
        }
      }
      // 2-4: empty (border only) — no fill

      if (val) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)val);

        // Font chain: try largest first, fall back for wide numbers
        const int maxW = TILE - 8;
        int fontId = UI_12_FONT_ID;
        if (renderer.getTextWidth(UI_12_FONT_ID, buf) > maxW) {
          fontId = UI_10_FONT_ID;
          if (renderer.getTextWidth(UI_10_FONT_ID, buf) > maxW) {
            fontId = SMALL_FONT_ID;
          }
        }
        const int textW = renderer.getTextWidth(fontId, buf);
        const int lineH = renderer.getLineHeight(fontId);

        if (val >= 512) {
          // Inverted text on solid background: clear text area then draw
          const int tx = x + (TILE - textW) / 2;
          const int ty = y + (TILE - lineH) / 2;
          // Clear a rect behind text for readability
          renderer.fillRect(tx - 2, ty - 1, textW + 4, lineH + 2, false);
          renderer.drawText(fontId, tx, ty, buf);
        } else {
          renderer.drawText(fontId,
                            x + (TILE - textW) / 2,
                            y + (TILE - lineH) / 2,
                            buf);
        }
      }
    }
  }

  // Overlay messages
  if (won && !gameOver) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_YOU_WIN));
  }
  if (gameOver) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_GAME_OVER));
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NEW_GAME), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
