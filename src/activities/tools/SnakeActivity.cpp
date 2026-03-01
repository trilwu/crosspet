#include "SnakeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <cstdio>
#include <cstring>
#include <esp_random.h>

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

  // Reset state
  body.clear();
  memset(occupied, 0, sizeof(occupied));
  gameOver = false;
  score = 0;
  dirX = 1;
  dirY = 0;

  // Initial snake: length 3, centered at row 20, heading right
  // head = (15,20), then (14,20), (13,20)
  body.push_back({15, 20});
  body.push_back({14, 20});
  body.push_back({13, 20});
  occupied[20][15] = true;
  occupied[20][14] = true;
  occupied[20][13] = true;

  spawnFood();
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

  // Accept direction input even when game over (so player sees they pressed)
  // but only move if not game over
  bool moved = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) && dirY != 1) {
    dirX = 0; dirY = -1; moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) && dirY != -1) {
    dirX = 0; dirY = 1; moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) && dirX != 1) {
    dirX = -1; dirY = 0; moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) && dirX != -1) {
    dirX = 1; dirY = 0; moved = true;
  }

  if (moved && !gameOver) {
    doMove();
    requestUpdate();
  }
}

void SnakeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int gameTop = metrics.topPadding + metrics.headerHeight;

  renderer.clearScreen();

  // Header with title
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_SNAKE));

  // Score shown in button hints area (btn3 slot) — avoids header overlap, matches Maze pattern
  char scoreStr[24];
  snprintf(scoreStr, sizeof(scoreStr), "Score: %d", score);

  // Draw food as outline square
  const int fx = foodX * CELL_SIZE;
  const int fy = gameTop + foodY * CELL_SIZE;
  renderer.drawRect(fx, fy, CELL_SIZE, CELL_SIZE);

  // Draw snake body as filled squares (1px gap on right/bottom for grid visibility)
  for (auto& seg : body) {
    const int sx = seg.first * CELL_SIZE;
    const int sy = gameTop + seg.second * CELL_SIZE;
    renderer.fillRect(sx, sy, CELL_SIZE - 1, CELL_SIZE - 1);
  }

  // Game over overlay
  if (gameOver) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Game Over!");
  }

  // Button hints: Back=Exit, Confirm=New Game, btn3=Score
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NEW_GAME), scoreStr, "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
