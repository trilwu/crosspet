#include "SnakeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <cstdio>
#include <cstring>
#include <esp_random.h>

#include "GameScores.h"
#include "components/UITheme.h"
#include "fontIds.h"

void SnakeActivity::spawnFood() {
  // Count empty cells then pick one at random — O(ROWS*COLS) but infrequent
  int emptyCount = COLS * ROWS - (int)body.size();
  if (emptyCount <= 0) return;
  int pick = (int)(esp_random() % (uint32_t)emptyCount);
  int idx = 0;
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (!occupied[y][x]) {
        if (idx++ == pick) {
          foodX = (int8_t)x;
          foodY = (int8_t)y;
          return;
        }
      }
    }
  }
}

void SnakeActivity::doMove() {
  const int8_t nx = body.front().first + dirX;
  const int8_t ny = body.front().second + dirY;

  // Wall collision
  if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) {
    gameOver = true;
    return;
  }
  // Self collision
  if (occupied[ny][nx]) {
    gameOver = true;
    return;
  }

  // Place new head
  body.push_front({nx, ny});
  occupied[ny][nx] = true;

  if (nx == foodX && ny == foodY) {
    // Ate food — grow (don't remove tail) and spawn new food
    score++;
    spawnFood();
  } else {
    // Normal move — remove tail
    auto tail = body.back();
    occupied[tail.second][tail.first] = false;
    body.pop_back();
  }
}

void SnakeActivity::onEnter() {
  Activity::onEnter();

  // Ensure session high reflects persisted all-time best
  if (GAME_SCORES.snakeHigh > (uint32_t)highScore) highScore = (int)GAME_SCORES.snakeHigh;

  // Persist all-time high score; update session high for display
  if (score > GAME_SCORES.snakeHigh) {
    GAME_SCORES.snakeHigh = score;
    GAME_SCORES.saveToFile();
  }
  if (score > highScore) highScore = score;

  // Reset state
  body.clear();
  memset(occupied, 0, sizeof(occupied));
  gameOver = false;
  score = 0;
  dirX = 1; dirY = 0;
  nextDirX = 1; nextDirY = 0;

  // Initial snake: length 3, centered at row 20, heading right
  body.push_back({15, 20});
  body.push_back({14, 20});
  body.push_back({13, 20});
  occupied[20][15] = true;
  occupied[20][14] = true;
  occupied[20][13] = true;

  spawnFood();
  lastMoveMs = millis();
  requestUpdate();
}

void SnakeActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onEnter();
    return;
  }

  if (!gameOver) {
    // Queue next direction — prevents 180° reversal into self
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)    && dirY != 1)  { nextDirX = 0;  nextDirY = -1; }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)  && dirY != -1) { nextDirX = 0;  nextDirY = 1;  }
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)  && dirX != 1)  { nextDirX = -1; nextDirY = 0;  }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right) && dirX != -1) { nextDirX = 1;  nextDirY = 0;  }
  } else {
    // When game over: Up/Down changes speed for next game
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) && speedIdx > 0) {
      speedIdx--;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down) && speedIdx < SPEED_COUNT - 1) {
      speedIdx++;
      requestUpdate();
    }
  }

  // Auto-advance: apply queued direction then step
  if (!gameOver && (millis() - lastMoveMs) >= SPEED_MS[speedIdx]) {
    dirX = nextDirX;
    dirY = nextDirY;
    doMove();
    lastMoveMs = millis();
    requestUpdate();
  }
}

void SnakeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int gameTop = metrics.topPadding + metrics.headerHeight;

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_SNAKE));

  // Draw food as outline square
  const int fx = foodX * CELL_SIZE;
  const int fy = gameTop + foodY * CELL_SIZE;
  renderer.drawRect(fx, fy, CELL_SIZE, CELL_SIZE);

  // Draw snake body as filled squares (1px gap for grid visibility)
  for (auto& seg : body) {
    const int sx = seg.first * CELL_SIZE;
    const int sy = gameTop + seg.second * CELL_SIZE;
    renderer.fillRect(sx, sy, CELL_SIZE - 1, CELL_SIZE - 1);
  }

  // Game over: show message + speed-change hint
  if (gameOver) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 16, "Game Over!");
    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 8, "Up/Down: speed");
  }

  // Button hints: score + high score in btn3, current speed in btn4
  char scoreStr[32];
  snprintf(scoreStr, sizeof(scoreStr), "Score:%d Hi:%d", score, highScore);
  char speedStr[16];
  snprintf(speedStr, sizeof(speedStr), "[%s]", SPEED_LABELS[speedIdx]);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NEW_GAME), scoreStr, speedStr);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
