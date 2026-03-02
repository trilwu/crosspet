#include "MazeGameActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <esp_random.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "components/UITheme.h"
#include "fontIds.h"

// Direction constants: N=0, E=1, S=2, W=3
static constexpr int DX[] = {0, 1, 0, -1};
static constexpr int DY[] = {-1, 0, 1, 0};
static constexpr int OPP[] = {2, 3, 0, 1};  // opposite direction index

void MazeGameActivity::generateMaze() {
  const auto& cfg = CONFIGS[(int)difficulty];

  for (int y = 0; y < cfg.rows; y++)
    for (int x = 0; x < cfg.cols; x++) {
      walls[y][x] = 0x0F;   // all 4 walls present
      visited[y][x] = false;
    }

  // Iterative DFS backtracker
  std::vector<std::pair<int, int>> stack;
  visited[0][0] = true;
  stack.push_back({0, 0});

  while (!stack.empty()) {
    auto [cx, cy] = stack.back();

    // Shuffle valid unvisited neighbours
    int dirs[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
      int j = (int)(esp_random() % (uint32_t)(i + 1));
      std::swap(dirs[i], dirs[j]);
    }

    bool moved = false;
    for (int i = 0; i < 4; i++) {
      int d = dirs[i];
      int nx = cx + DX[d], ny = cy + DY[d];
      if (nx >= 0 && nx < cfg.cols && ny >= 0 && ny < cfg.rows && !visited[ny][nx]) {
        walls[cy][cx] &= ~(1 << d);
        walls[ny][nx] &= ~(1 << OPP[d]);
        visited[ny][nx] = true;
        stack.push_back({nx, ny});
        moved = true;
        break;
      }
    }
    if (!moved) stack.pop_back();
  }
}

bool MazeGameActivity::hasWall(int x, int y, int dir) const {
  return (walls[y][x] >> dir) & 1;
}

bool MazeGameActivity::tryMove(int dx, int dy, int dir) {
  const auto& cfg = CONFIGS[(int)difficulty];
  if (victory || hasWall(playerX, playerY, dir)) return false;
  playerX += dx;
  playerY += dy;
  moveCount++;
  if (playerX == cfg.cols - 1 && playerY == cfg.rows - 1) victory = true;
  return true;
}

void MazeGameActivity::onEnter() {
  Activity::onEnter();
  generateMaze();
  playerX = 0; playerY = 0;
  moveCount = 0; victory = false;
  requestUpdate();
}

void MazeGameActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finish(); return; }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    generateMaze();
    playerX = 0; playerY = 0;
    moveCount = 0; victory = false;
    requestUpdate();
    return;
  }

  // Read directional inputs once — wasReleased is one-shot per frame
  const bool up    = mappedInput.wasReleased(MappedInputManager::Button::Up);
  const bool down  = mappedInput.wasReleased(MappedInputManager::Button::Down);
  const bool left  = mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool right = mappedInput.wasReleased(MappedInputManager::Button::Right);

  // Up/Down changes difficulty only when not actively playing (before first move or after victory)
  if (moveCount == 0 || victory) {
    bool diffChanged = false;
    if (up   && (int)difficulty > 0) { difficulty = (Difficulty)((int)difficulty - 1); diffChanged = true; }
    if (down && (int)difficulty < 2) { difficulty = (Difficulty)((int)difficulty + 1); diffChanged = true; }
    if (diffChanged) {
      generateMaze();
      playerX = 0; playerY = 0;
      moveCount = 0; victory = false;
      requestUpdate();
      return;
    }
  }

  // Player movement
  bool moved = false;
  if (right) moved |= tryMove(1, 0, 1);
  if (left)  moved |= tryMove(-1, 0, 3);
  if (down)  moved |= tryMove(0, 1, 2);
  if (up)    moved |= tryMove(0, -1, 0);
  if (moved) requestUpdate();
}

void MazeGameActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& cfg = CONFIGS[(int)difficulty];
  const int pageWidth = renderer.getScreenWidth();
  const int mazeY = metrics.topPadding + metrics.headerHeight;

  // Center Small maze; Medium/Large fill 480px naturally
  const int mazeOffsetX = (pageWidth - cfg.cols * cfg.cellSize) / 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_MAZE_GAME));

  // Draw N + W wall for each cell, then close the S/E border
  for (int y = 0; y < cfg.rows; y++) {
    for (int x = 0; x < cfg.cols; x++) {
      const int px = mazeOffsetX + x * cfg.cellSize;
      const int py = mazeY + y * cfg.cellSize;
      if (hasWall(x, y, 0)) renderer.fillRect(px, py, cfg.cellSize + 1, 2);
      if (hasWall(x, y, 3)) renderer.fillRect(px, py, 2, cfg.cellSize + 1);
    }
  }
  for (int x = 0; x < cfg.cols; x++) {
    if (hasWall(x, cfg.rows - 1, 2))
      renderer.fillRect(mazeOffsetX + x * cfg.cellSize, mazeY + cfg.rows * cfg.cellSize, cfg.cellSize + 1, 2);
  }
  for (int y = 0; y < cfg.rows; y++) {
    if (hasWall(cfg.cols - 1, y, 1))
      renderer.fillRect(mazeOffsetX + cfg.cols * cfg.cellSize, mazeY + y * cfg.cellSize, 2, cfg.cellSize + 1);
  }

  // Goal: double square at bottom-right cell center
  const int gx = mazeOffsetX + (cfg.cols - 1) * cfg.cellSize + cfg.cellSize / 2;
  const int gy = mazeY + (cfg.rows - 1) * cfg.cellSize + cfg.cellSize / 2;
  renderer.drawRect(gx - 5, gy - 5, 10, 10);
  renderer.drawRect(gx - 8, gy - 8, 16, 16);

  // Player: filled square at current cell center
  const int px = mazeOffsetX + playerX * cfg.cellSize + cfg.cellSize / 2;
  const int py = mazeY + playerY * cfg.cellSize + cfg.cellSize / 2;
  renderer.fillRect(px - 5, py - 5, 10, 10);

  char status[40];
  if (victory) snprintf(status, sizeof(status), "Done! %d moves", moveCount);
  else         snprintf(status, sizeof(status), "Moves: %d", moveCount);

  char diffStr[16];
  snprintf(diffStr, sizeof(diffStr), "[%s]", cfg.label);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NEW_MAZE), status, diffStr);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
