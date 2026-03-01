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
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++) {
      walls[y][x] = 0x0F;   // all 4 walls present
      visited[y][x] = false;
    }

  // Iterative DFS backtracker
  std::vector<std::pair<int, int>> stack;
  visited[0][0] = true;
  stack.push_back({0, 0});

  while (!stack.empty()) {
    auto [cx, cy] = stack.back();

    // Collect and shuffle valid unvisited neighbours
    int dirs[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
      int j = (int)(esp_random() % (uint32_t)(i + 1));
      std::swap(dirs[i], dirs[j]);
    }

    bool moved = false;
    for (int i = 0; i < 4; i++) {
      int d = dirs[i];
      int nx = cx + DX[d], ny = cy + DY[d];
      if (nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS && !visited[ny][nx]) {
        walls[cy][cx] &= ~(1 << d);         // remove wall on this side
        walls[ny][nx] &= ~(1 << OPP[d]);    // remove wall on opposite side
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
  if (victory || hasWall(playerX, playerY, dir)) return false;
  playerX += dx;
  playerY += dy;
  moveCount++;
  if (playerX == COLS - 1 && playerY == ROWS - 1) victory = true;
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

  bool moved = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) moved |= tryMove(1, 0, 1);
  if (mappedInput.wasReleased(MappedInputManager::Button::Left))  moved |= tryMove(-1, 0, 3);
  if (mappedInput.wasReleased(MappedInputManager::Button::Down))  moved |= tryMove(0, 1, 2);
  if (mappedInput.wasReleased(MappedInputManager::Button::Up))    moved |= tryMove(0, -1, 0);
  if (moved) requestUpdate();
}

void MazeGameActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int mazeY = metrics.topPadding + metrics.headerHeight;
  const int pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_MAZE_GAME));

  // Draw maze walls (N + W for every cell, then close S/E border)
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      const int px = x * CELL_SIZE;
      const int py = mazeY + y * CELL_SIZE;
      if (hasWall(x, y, 0)) renderer.fillRect(px, py, CELL_SIZE + 1, 2);          // N
      if (hasWall(x, y, 3)) renderer.fillRect(px, py, 2, CELL_SIZE + 1);          // W
    }
  }
  // Bottom border (S wall of last row) and right border (E wall of last col)
  for (int x = 0; x < COLS; x++) {
    if (hasWall(x, ROWS - 1, 2))
      renderer.fillRect(x * CELL_SIZE, mazeY + ROWS * CELL_SIZE, CELL_SIZE + 1, 2);
  }
  for (int y = 0; y < ROWS; y++) {
    if (hasWall(COLS - 1, y, 1))
      renderer.fillRect(COLS * CELL_SIZE, mazeY + y * CELL_SIZE, 2, CELL_SIZE + 1);
  }

  // Goal: double square at bottom-right
  const int gx = (COLS - 1) * CELL_SIZE + CELL_SIZE / 2;
  const int gy = mazeY + (ROWS - 1) * CELL_SIZE + CELL_SIZE / 2;
  renderer.drawRect(gx - 5, gy - 5, 10, 10);
  renderer.drawRect(gx - 8, gy - 8, 16, 16);

  // Player: filled square
  const int px = playerX * CELL_SIZE + CELL_SIZE / 2;
  const int py = mazeY + playerY * CELL_SIZE + CELL_SIZE / 2;
  renderer.fillRect(px - 5, py - 5, 10, 10);

  // Status line in button hints
  char status[40];
  if (victory) snprintf(status, sizeof(status), "Done! %d moves", moveCount);
  else         snprintf(status, sizeof(status), "Moves: %d", moveCount);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NEW_MAZE), status, "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
