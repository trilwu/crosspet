#include "MinesweeperActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdlib>

#include "components/UITheme.h"
#include "fontIds.h"

int MinesweeperActivity::countAdjacentMines(int row, int col) const {
  int count = 0;
  for (int dr = -1; dr <= 1; dr++)
    for (int dc = -1; dc <= 1; dc++) {
      int r = row + dr, c = col + dc;
      if (r >= 0 && r < ROWS && c >= 0 && c < COLS && grid[r][c] == 9) count++;
    }
  return count;
}

int MinesweeperActivity::countAdjacentFlags(int row, int col) const {
  int count = 0;
  for (int dr = -1; dr <= 1; dr++)
    for (int dc = -1; dc <= 1; dc++) {
      int r = row + dr, c = col + dc;
      if (r >= 0 && r < ROWS && c >= 0 && c < COLS && state[r][c] == CellState::FLAGGED) count++;
    }
  return count;
}

void MinesweeperActivity::placeMines(int safeRow, int safeCol) {
  randomSeed(millis());
  int placed = 0;
  while (placed < mineCount) {
    int r = random(0, ROWS);
    int c = random(0, COLS);
    // Keep safe zone around first click (3x3)
    if (abs(r - safeRow) <= 1 && abs(c - safeCol) <= 1) continue;
    if (grid[r][c] == 9) continue;
    grid[r][c] = 9;
    placed++;
  }
  // Calculate adjacent mine counts for non-mine cells
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (grid[r][c] != 9) grid[r][c] = (uint8_t)countAdjacentMines(r, c);
}

void MinesweeperActivity::reveal(int row, int col) {
  if (row < 0 || row >= ROWS || col < 0 || col >= COLS) return;
  if (state[row][col] != CellState::HIDDEN) return;

  state[row][col] = CellState::REVEALED;
  revealedCount++;

  // Flood-fill for empty cells (0 adjacent mines)
  if (grid[row][col] == 0) {
    for (int dr = -1; dr <= 1; dr++)
      for (int dc = -1; dc <= 1; dc++)
        reveal(row + dr, col + dc);
  }
}

void MinesweeperActivity::revealAll() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      state[r][c] = CellState::REVEALED;
}

bool MinesweeperActivity::checkWin() const {
  return revealedCount == (ROWS * COLS - mineCount);
}

const char* MinesweeperActivity::difficultyLabel() const {
  switch (difficulty) {
    case Difficulty::EASY:   return tr(STR_MINES_EASY);
    case Difficulty::MEDIUM: return tr(STR_MINES_MEDIUM);
    case Difficulty::HARD:   return tr(STR_MINES_HARD);
  }
  return "";
}

void MinesweeperActivity::onEnter() {
  Activity::onEnter();
  mineCount = MINE_COUNTS[(int)difficulty];
  memset(grid, 0, sizeof(grid));
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      state[r][c] = CellState::HIDDEN;
  cursorRow = ROWS / 2;
  cursorCol = COLS / 2;
  flagCount = 0;
  revealedCount = 0;
  gameOver = false;
  won = false;
  firstMove = true;
  requestUpdate();
}

void MinesweeperActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // Game over / won: Confirm = new game, Left/Right = difficulty
  if (gameOver || won) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      onEnter();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      difficulty = (Difficulty)(((int)difficulty - 1 + DIFFICULTY_COUNT) % DIFFICULTY_COUNT);
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      difficulty = (Difficulty)(((int)difficulty + 1) % DIFFICULTY_COUNT);
      requestUpdate();
    }
    return;
  }

  bool changed = false;

  // D-pad moves cursor — use direct button checks (not ButtonNavigator which
  // bundles Left with Up and Right with Down, stealing horizontal events)
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    cursorCol = (cursorCol - 1 + COLS) % COLS;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    cursorCol = (cursorCol + 1) % COLS;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    cursorRow = (cursorRow - 1 + ROWS) % ROWS;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    cursorRow = (cursorRow + 1) % ROWS;
    changed = true;
  }

  // Confirm: tap = reveal, long-press (>500ms) = toggle flag
  static constexpr unsigned long FLAG_HOLD_MS = 500;
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (mappedInput.getHeldTime() >= FLAG_HOLD_MS) {
      // Long-press: toggle flag
      if (state[cursorRow][cursorCol] == CellState::HIDDEN) {
        state[cursorRow][cursorCol] = CellState::FLAGGED;
        flagCount++;
        changed = true;
      } else if (state[cursorRow][cursorCol] == CellState::FLAGGED) {
        state[cursorRow][cursorCol] = CellState::HIDDEN;
        flagCount--;
        changed = true;
      }
    } else {
      // Short tap: reveal cell or chord reveal
      if (state[cursorRow][cursorCol] == CellState::HIDDEN) {
        if (firstMove) {
          placeMines(cursorRow, cursorCol);
          firstMove = false;
        }
        if (grid[cursorRow][cursorCol] == 9) {
          revealAll();
          gameOver = true;
        } else {
          reveal(cursorRow, cursorCol);
          if (checkWin()) { won = true; revealAll(); }
        }
        changed = true;
      } else if (state[cursorRow][cursorCol] == CellState::REVEALED && grid[cursorRow][cursorCol] > 0) {
        // Chord: if flags match number, reveal all hidden neighbors
        if (countAdjacentFlags(cursorRow, cursorCol) == grid[cursorRow][cursorCol]) {
          for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++) {
              int r = cursorRow + dr, c = cursorCol + dc;
              if (r >= 0 && r < ROWS && c >= 0 && c < COLS && state[r][c] == CellState::HIDDEN) {
                if (grid[r][c] == 9) { revealAll(); gameOver = true; }
                else reveal(r, c);
              }
            }
          if (!gameOver && checkWin()) { won = true; revealAll(); }
          changed = true;
        }
      } else if (state[cursorRow][cursorCol] == CellState::FLAGGED) {
        // Tap on flagged = unflag
        state[cursorRow][cursorCol] = CellState::HIDDEN;
        flagCount--;
        changed = true;
      }
    }
  }

  if (changed) requestUpdate();
}

void MinesweeperActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MINESWEEPER));

  const int gridW = CELL * COLS;
  const int gridH = CELL * ROWS;
  const int gridX = (pageWidth - gridW) / 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight + 4;
  const int contentBot = pageHeight - metrics.buttonHintsHeight - 4;
  const int gridY = contentTop + (contentBot - contentTop - gridH - 20) / 2;

  // Draw cells
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      int cx = gridX + c * CELL;
      int cy = gridY + r * CELL;
      bool isCursor = (r == cursorRow && c == cursorCol);

      if (state[r][c] == CellState::HIDDEN || state[r][c] == CellState::FLAGGED) {
        // Hidden cell: filled rectangle
        renderer.fillRect(cx + 1, cy + 1, CELL - 2, CELL - 2);
        if (state[r][c] == CellState::FLAGGED) {
          // Draw flag marker: "F" in white
          int tw = renderer.getTextWidth(SMALL_FONT_ID, "F");
          renderer.drawText(SMALL_FONT_ID, cx + (CELL - tw) / 2,
                            cy + (CELL - renderer.getLineHeight(SMALL_FONT_ID)) / 2, "F", false);
        }
        if (isCursor) {
          // Draw cursor border in white on the filled cell (rounded)
          renderer.drawRoundedRect(cx + 2, cy + 2, CELL - 4, CELL - 4, 2, 4, false);
          renderer.drawRoundedRect(cx + 3, cy + 3, CELL - 6, CELL - 6, 1, 3, false);
        }
      } else {
        // Revealed cell
        renderer.drawRect(cx, cy, CELL, CELL);
        if (isCursor) {
          // Rounded cursor border on revealed cell (2px thick)
          renderer.drawRoundedRect(cx + 1, cy + 1, CELL - 2, CELL - 2, 2, 4, true);
          renderer.drawRoundedRect(cx + 2, cy + 2, CELL - 4, CELL - 4, 1, 3, true);
        }

        if (grid[r][c] == 9) {
          // Mine: draw filled circle approximation (small filled rect)
          int sz = 10;
          int mx = cx + (CELL - sz) / 2;
          int my = cy + (CELL - sz) / 2;
          renderer.fillRect(mx, my, sz, sz);
        } else if (grid[r][c] > 0) {
          char buf[2] = {(char)('0' + grid[r][c]), 0};
          int tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
          renderer.drawText(SMALL_FONT_ID, cx + (CELL - tw) / 2,
                            cy + (CELL - renderer.getLineHeight(SMALL_FONT_ID)) / 2, buf);
        }
      }
    }
  }

  // Status line below grid
  int statusY = gridY + gridH + 4;
  if (gameOver) {
    renderer.drawCenteredText(SMALL_FONT_ID, statusY, tr(STR_GAME_OVER), true, EpdFontFamily::BOLD);
  } else if (won) {
    renderer.drawCenteredText(SMALL_FONT_ID, statusY, tr(STR_YOU_WIN), true, EpdFontFamily::BOLD);
  } else {
    char info[32];
    snprintf(info, sizeof(info), tr(STR_MINES_INFO_FMT), mineCount - flagCount);
    renderer.drawCenteredText(SMALL_FONT_ID, statusY, info);
  }

  // Button hints
  const char* btn1 = tr(STR_BACK);
  const char* btn2 = "";
  const char* btn3 = "";
  const char* btn4 = "";

  if (gameOver || won) {
    btn2 = tr(STR_NEW_GAME);
    btn4 = difficultyLabel();
  } else {
    btn2 = tr(STR_MINES_REVEAL);
  }

  const auto labels = mappedInput.mapLabels(btn1, btn2, btn3, btn4);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
