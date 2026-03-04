#include "SudokuActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdlib>

#include "components/UITheme.h"
#include "fontIds.h"

// Check if placing num at (row,col) is valid in the given grid
bool SudokuActivity::isValidPlacement(const uint8_t grid[][SIZE], int row, int col, uint8_t num) const {
  for (int i = 0; i < SIZE; i++) {
    if (grid[row][i] == num) return false;
    if (grid[i][col] == num) return false;
  }
  int boxR = (row / BOX) * BOX, boxC = (col / BOX) * BOX;
  for (int r = boxR; r < boxR + BOX; r++)
    for (int c = boxC; c < boxC + BOX; c++)
      if (grid[r][c] == num) return false;
  return true;
}

// Recursively fill grid with a valid complete solution using backtracking
bool SudokuActivity::fillGrid(int pos) {
  if (pos == SIZE * SIZE) return true;
  int row = pos / SIZE, col = pos % SIZE;

  // Try digits in random order for variety
  uint8_t order[SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  for (int i = SIZE - 1; i > 0; i--) {
    int j = random(0, i + 1);
    uint8_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
  }

  for (int i = 0; i < SIZE; i++) {
    uint8_t num = order[i];
    if (isValidPlacement(solution, row, col, num)) {
      solution[row][col] = num;
      if (fillGrid(pos + 1)) return true;
      solution[row][col] = 0;
    }
  }
  return false;
}

void SudokuActivity::generatePuzzle() {
  // Generate a complete valid solution
  memset(solution, 0, sizeof(solution));
  randomSeed(millis());
  fillGrid(0);

  // Copy solution to puzzle, then remove cells
  memcpy(puzzle, solution, sizeof(puzzle));
  memset(fixed, true, sizeof(fixed));

  int toRemove = REMOVE_COUNT[(int)difficulty];
  while (toRemove > 0) {
    int r = random(0, SIZE);
    int c = random(0, SIZE);
    if (puzzle[r][c] != 0) {
      puzzle[r][c] = 0;
      fixed[r][c] = false;
      toRemove--;
    }
  }

  cursorRow = cursorCol = 0;
  completed = false;
}

bool SudokuActivity::checkCompleted() const {
  for (int r = 0; r < SIZE; r++)
    for (int c = 0; c < SIZE; c++)
      if (puzzle[r][c] != solution[r][c]) return false;
  return true;
}

const char* SudokuActivity::difficultyLabel() const {
  switch (difficulty) {
    case Difficulty::EASY:   return tr(STR_SUDOKU_EASY);
    case Difficulty::MEDIUM: return tr(STR_SUDOKU_MEDIUM);
    case Difficulty::HARD:   return tr(STR_SUDOKU_HARD);
  }
  return "";
}

void SudokuActivity::onEnter() {
  Activity::onEnter();
  generatePuzzle();
  requestUpdate();
}

void SudokuActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (completed) {
    // After winning: Confirm = new game, Left/Right = change difficulty
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      generatePuzzle();
      requestUpdate();
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

  // D-pad always moves cursor in all 4 directions
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    cursorCol = (cursorCol - 1 + SIZE) % SIZE;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    cursorCol = (cursorCol + 1) % SIZE;
    changed = true;
  }
  buttonNavigator.onPrevious([&] {
    cursorRow = (cursorRow - 1 + SIZE) % SIZE;
    changed = true;
  });
  buttonNavigator.onNext([&] {
    cursorRow = (cursorRow + 1) % SIZE;
    changed = true;
  });

  // Confirm = cycle number on editable cells (0→1→2→...→9→0)
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!fixed[cursorRow][cursorCol]) {
      uint8_t v = puzzle[cursorRow][cursorCol];
      puzzle[cursorRow][cursorCol] = (v >= 9) ? 0 : v + 1;
      if (puzzle[cursorRow][cursorCol] != 0 && checkCompleted()) completed = true;
      changed = true;
    }
  }

  if (changed) requestUpdate();
}

void SudokuActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SUDOKU));

  const int cellSize = 48;
  const int gridSize = cellSize * SIZE;
  const int gridX = (pageWidth - gridSize) / 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBot = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int gridY = contentTop + (contentBot - contentTop - gridSize - 30) / 2;

  // Draw grid lines
  // Thin lines for all cells
  for (int i = 0; i <= SIZE; i++) {
    int y = gridY + i * cellSize;
    int x = gridX + i * cellSize;
    renderer.drawLine(gridX, y, gridX + gridSize, y);
    renderer.drawLine(x, gridY, x, gridY + gridSize);
  }
  // Thick lines for 3x3 boxes (draw 3px wide)
  for (int i = 0; i <= BOX; i++) {
    int bx = gridX + i * BOX * cellSize;
    int by = gridY + i * BOX * cellSize;
    for (int t = -1; t <= 1; t++) {
      renderer.drawLine(gridX, by + t, gridX + gridSize, by + t);
      renderer.drawLine(bx + t, gridY, bx + t, gridY + gridSize);
    }
  }

  // Draw numbers
  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE; c++) {
      int cx = gridX + c * cellSize;
      int cy = gridY + r * cellSize;

      // Highlight cursor cell
      bool isCursor = (r == cursorRow && c == cursorCol);
      if (isCursor) {
        renderer.fillRect(cx + 1, cy + 1, cellSize - 2, cellSize - 2);
      }

      uint8_t val = puzzle[r][c];
      if (val == 0) continue;

      char buf[2] = {(char)('0' + val), 0};
      int tw = renderer.getTextWidth(UI_10_FONT_ID, buf);
      int th = renderer.getLineHeight(UI_10_FONT_ID);
      int tx = cx + (cellSize - tw) / 2;
      int ty = cy + (cellSize - th) / 2;

      if (isCursor) {
        // White text on black background
        renderer.drawText(UI_10_FONT_ID, tx, ty, buf, false,
                          fixed[r][c] ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      } else {
        // Conflict highlight: temporarily clear cell to check placement validity
        bool conflict = false;
        if (showConflicts && !fixed[r][c]) {
          puzzle[r][c] = 0;
          conflict = !isValidPlacement(puzzle, r, c, val);
          puzzle[r][c] = val;
        }
        if (conflict) {
          // Underline conflicting numbers
          renderer.drawText(UI_10_FONT_ID, tx, ty, buf, true, EpdFontFamily::REGULAR);
          renderer.drawLine(tx, ty + th - 2, tx + tw, ty + th - 2);
        } else {
          renderer.drawText(UI_10_FONT_ID, tx, ty, buf, true,
                            fixed[r][c] ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
        }
      }
    }
  }

  // Status line below grid
  int statusY = gridY + gridSize + 10;
  if (completed) {
    renderer.drawCenteredText(UI_10_FONT_ID, statusY, tr(STR_SUDOKU_COMPLETE), true, EpdFontFamily::BOLD);
  } else {
    // Show difficulty
    renderer.drawCenteredText(SMALL_FONT_ID, statusY, difficultyLabel());
  }

  // Button hints
  const char* btn1 = tr(STR_BACK);
  const char* btn2 = "";
  const char* btn3 = "";
  const char* btn4 = "";

  if (completed) {
    btn2 = tr(STR_NEW_GAME);
    btn4 = difficultyLabel();
  } else if (!fixed[cursorRow][cursorCol]) {
    btn2 = tr(STR_SUDOKU_NUM_HINT);
  }

  const auto labels = mappedInput.mapLabels(btn1, btn2, btn3, btn4);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
