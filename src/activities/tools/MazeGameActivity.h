#pragma once

#include <cstdint>

#include "../Activity.h"

// Random maze game. Navigate from top-left to bottom-right using D-pad.
// Confirm = generate new maze. Back = exit.
// Uses iterative DFS backtracker — all mazes are fully connected.
class MazeGameActivity final : public Activity {
  static constexpr int COLS = 20;
  static constexpr int ROWS = 27;
  static constexpr int CELL_SIZE = 24;  // px per cell (20*24=480 fits screen width)

  // Wall bits per cell: bit0=N, bit1=E, bit2=S, bit3=W
  uint8_t walls[ROWS][COLS];
  bool visited[ROWS][COLS];

  int playerX = 0;
  int playerY = 0;
  int moveCount = 0;
  bool victory = false;

  void generateMaze();
  bool hasWall(int x, int y, int dir) const;
  bool tryMove(int dx, int dy, int dir);  // returns true if player actually moved

 public:
  explicit MazeGameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Maze", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
