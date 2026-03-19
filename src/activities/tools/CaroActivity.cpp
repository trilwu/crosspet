#include "CaroActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdlib>
#include <cstring>

#include "components/UITheme.h"
#include "fontIds.h"

// Count consecutive stones of `player` starting from (row,col) in direction (dr,dc)
int CaroActivity::countDirection(int row, int col, int dr, int dc, uint8_t player) const {
  int count = 0;
  int r = row + dr, c = col + dc;
  while (r >= 0 && r < SIZE && c >= 0 && c < SIZE && grid[r][c] == player) {
    count++;
    r += dr;
    c += dc;
  }
  return count;
}

// Check if placing at (row,col) creates 5-in-a-row for player
bool CaroActivity::checkWin(int row, int col, uint8_t player) const {
  static constexpr int dirs[][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  for (const auto& d : dirs) {
    int total = 1 + countDirection(row, col, d[0], d[1], player) +
                countDirection(row, col, -d[0], -d[1], player);
    if (total >= 5) return true;
  }
  return false;
}

// Score a position for AI move evaluation
// Higher score = more desirable move for `player`
int CaroActivity::scorePosition(int row, int col, uint8_t player) const {
  static constexpr int dirs[][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  int totalScore = 0;
  for (const auto& d : dirs) {
    int count = 1 + countDirection(row, col, d[0], d[1], player) +
                countDirection(row, col, -d[0], -d[1], player);
    if (count >= 5) totalScore += 100000;
    else if (count == 4) totalScore += 10000;
    else if (count == 3) totalScore += 1000;
    else if (count == 2) totalScore += 100;
  }
  // Bonus for center proximity
  int centerDist = abs(row - SIZE / 2) + abs(col - SIZE / 2);
  totalScore += (SIZE - centerDist);
  return totalScore;
}

void CaroActivity::doAiMove() {

  if (difficulty == Difficulty::EASY) {
    // Random empty cell, biased toward center area
    int bestR = -1, bestC = -1, bestDist = SIZE * 2;
    int attempts = 0;
    while (attempts < 200) {
      int r = random(0, SIZE);
      int c = random(0, SIZE);
      if (grid[r][c] == 0) {
        int dist = abs(r - SIZE / 2) + abs(c - SIZE / 2);
        if (bestR < 0 || dist < bestDist) {
          bestR = r; bestC = c; bestDist = dist;
        }
        if (attempts > 20) break;
      }
      attempts++;
    }
    // Fallback: linear scan if random missed (near-full board)
    if (bestR < 0) {
      for (int r = 0; r < SIZE && bestR < 0; r++)
        for (int c = 0; c < SIZE && bestR < 0; c++)
          if (grid[r][c] == 0) { bestR = r; bestC = c; }
    }
    if (bestR < 0) { gameOver = true; winner = 0; return; }
    grid[bestR][bestC] = 2;
    moveCount++;
    if (checkWin(bestR, bestC, 2)) { gameOver = true; winner = 2; }
    else if (moveCount >= SIZE * SIZE) { gameOver = true; winner = 0; }
    return;
  }

  // Medium & Hard: score-based AI
  int bestScore = -1;
  int bestR = -1, bestC = -1;

  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE; c++) {
      if (grid[r][c] != 0) continue;

      // Skip cells with no neighbors (optimization)
      bool hasNeighbor = false;
      for (int dr = -2; dr <= 2 && !hasNeighbor; dr++)
        for (int dc = -2; dc <= 2 && !hasNeighbor; dc++) {
          int nr = r + dr, nc = c + dc;
          if (nr >= 0 && nr < SIZE && nc >= 0 && nc < SIZE && grid[nr][nc] != 0)
            hasNeighbor = true;
        }
      if (!hasNeighbor && moveCount > 0) continue;

      // Score for AI placing here (offensive)
      int offenseScore = scorePosition(r, c, 2);
      // Score for blocking human (defensive)
      int defenseScore = scorePosition(r, c, 1);

      int score;
      if (difficulty == Difficulty::HARD) {
        // Hard: balanced offense/defense, slight offense preference
        score = offenseScore + defenseScore * 9 / 10;
      } else {
        // Medium: mostly defensive, weaker offense
        score = offenseScore * 7 / 10 + defenseScore;
      }

      // Add small random factor for variety
      score += random(0, 50);

      if (score > bestScore) {
        bestScore = score;
        bestR = r;
        bestC = c;
      }
    }
  }

  if (bestR >= 0) {
    grid[bestR][bestC] = 2;
    moveCount++;
    if (checkWin(bestR, bestC, 2)) { gameOver = true; winner = 2; }
    else if (moveCount >= SIZE * SIZE) { gameOver = true; winner = 0; }
  }
}

const char* CaroActivity::difficultyLabel() const {
  switch (difficulty) {
    case Difficulty::EASY:   return tr(STR_CARO_EASY);
    case Difficulty::MEDIUM: return tr(STR_CARO_MEDIUM);
    case Difficulty::HARD:   return tr(STR_CARO_HARD);
  }
  return "";
}

void CaroActivity::initBoard() {
  memset(grid, 0, sizeof(grid));
  cursorRow = SIZE / 2;
  cursorCol = SIZE / 2;
  gameOver = false;
  winner = 0;
  moveCount = 0;
  randomSeed(millis());
}

void CaroActivity::onEnter() {
  Activity::onEnter();
  showingDifficultySelect = true;
  requestUpdate();
}

void CaroActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (showingDifficultySelect) {
      finish();
    } else if (gameOver) {
      showingDifficultySelect = true;
      requestUpdate();
    } else {
      finish();
    }
    return;
  }

  // Difficulty selection screen
  if (showingDifficultySelect) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      difficulty = (Difficulty)(((int)difficulty - 1 + DIFFICULTY_COUNT) % DIFFICULTY_COUNT);
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      difficulty = (Difficulty)(((int)difficulty + 1) % DIFFICULTY_COUNT);
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      showingDifficultySelect = false;
      initBoard();
      requestUpdate();
    }
    return;
  }

  // Game over: Confirm = difficulty select screen
  if (gameOver) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      showingDifficultySelect = true;
      requestUpdate();
      return;
    }
    return;
  }

  bool changed = false;

  // D-pad cursor movement
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    cursorCol = (cursorCol - 1 + SIZE) % SIZE;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    cursorCol = (cursorCol + 1) % SIZE;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    cursorRow = (cursorRow - 1 + SIZE) % SIZE;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    cursorRow = (cursorRow + 1) % SIZE;
    changed = true;
  }

  // Confirm = place stone on empty cell, then AI responds
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (grid[cursorRow][cursorCol] == 0) {
      grid[cursorRow][cursorCol] = 1;  // human = X
      moveCount++;

      if (checkWin(cursorRow, cursorCol, 1)) {
        gameOver = true;
        winner = 1;
      } else if (moveCount >= SIZE * SIZE) {
        gameOver = true;
        winner = 0;
      } else {
        // AI turn
        doAiMove();
      }
      changed = true;
    }
  }

  if (changed) requestUpdate();
}

void CaroActivity::renderDifficultySelect() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CARO));

  // Draw "Select Difficulty" title
  const int centerY = (metrics.topPadding + metrics.headerHeight + pageHeight - metrics.buttonHintsHeight) / 2;
  renderer.drawCenteredText(UI_12_FONT_ID, centerY - 40, tr(STR_SELECT_DIFFICULTY));

  // Draw difficulty with left/right arrows
  char label[32];
  snprintf(label, sizeof(label), "< %s >", difficultyLabel());
  renderer.drawCenteredText(UI_10_FONT_ID, centerY, label, true, EpdFontFamily::BOLD);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void CaroActivity::render(RenderLock&&) {
  if (showingDifficultySelect) {
    renderDifficultySelect();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CARO));

  const int gridW = CELL * SIZE;
  const int gridH = CELL * SIZE;
  const int gridX = (pageWidth - gridW) / 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight + 4;
  const int contentBot = pageHeight - metrics.buttonHintsHeight - 4;
  const int gridY = contentTop + (contentBot - contentTop - gridH - 20) / 2;

  // Draw grid lines
  for (int i = 0; i <= SIZE; i++) {
    renderer.drawLine(gridX, gridY + i * CELL, gridX + gridW, gridY + i * CELL);
    renderer.drawLine(gridX + i * CELL, gridY, gridX + i * CELL, gridY + gridH);
  }

  // Draw stones and cursor
  for (int r = 0; r < SIZE; r++) {
    for (int c = 0; c < SIZE; c++) {
      int cx = gridX + c * CELL;
      int cy = gridY + r * CELL;
      bool isCursor = (r == cursorRow && c == cursorCol);

      // Cursor: use rounded border on empty cell, black fill on occupied cells
      if (isCursor && grid[r][c] == 0) {
        renderer.drawRoundedRect(cx + 2, cy + 2, CELL - 4, CELL - 4, 2, 4, true);
        renderer.drawRoundedRect(cx + 3, cy + 3, CELL - 6, CELL - 6, 1, 3, true);
      } else if (isCursor) {
        renderer.fillRect(cx + 1, cy + 1, CELL - 2, CELL - 2);
      }

      if (grid[r][c] == 1) {
        // X: two diagonal lines
        int m = 5;
        bool color = isCursor ? false : true;
        renderer.drawLine(cx + m, cy + m, cx + CELL - m, cy + CELL - m, color);
        renderer.drawLine(cx + m + 1, cy + m, cx + CELL - m + 1, cy + CELL - m, color);
        renderer.drawLine(cx + CELL - m, cy + m, cx + m, cy + CELL - m, color);
        renderer.drawLine(cx + CELL - m - 1, cy + m, cx + m - 1, cy + CELL - m, color);
      } else if (grid[r][c] == 2) {
        // O: text character centered
        const char* ch = "O";
        int tw = renderer.getTextWidth(SMALL_FONT_ID, ch);
        int th = renderer.getLineHeight(SMALL_FONT_ID);
        renderer.drawText(SMALL_FONT_ID, cx + (CELL - tw) / 2, cy + (CELL - th) / 2, ch, !isCursor,
                          EpdFontFamily::BOLD);
      }
    }
  }

  // Status line below grid
  int statusY = gridY + gridH + 4;
  if (gameOver) {
    const char* msg = (winner == 1) ? tr(STR_CARO_WIN_X) : (winner == 2) ? tr(STR_CARO_WIN_O) : tr(STR_CARO_DRAW);
    // Wrap game over message in a small card
    int msgW = renderer.getTextWidth(UI_10_FONT_ID, msg);
    int msgH = renderer.getLineHeight(UI_10_FONT_ID);
    int cardPadX = 16, cardPadY = 6;
    int cardX = (pageWidth - msgW - cardPadX * 2) / 2;
    renderer.fillRoundedRect(cardX, statusY - cardPadY, msgW + cardPadX * 2, msgH + cardPadY * 2, 8, Color::Black);
    renderer.drawCenteredText(UI_10_FONT_ID, statusY, msg, false, EpdFontFamily::BOLD);
  } else {
    renderer.drawCenteredText(SMALL_FONT_ID, statusY, difficultyLabel());
  }

  // Button hints
  const char* btn1 = tr(STR_BACK);
  const char* btn2 = gameOver ? tr(STR_NEW_GAME) : tr(STR_CARO_PLACE);
  const auto labels = mappedInput.mapLabels(btn1, btn2, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
