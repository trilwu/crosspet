#pragma once

#include <cstdint>

#include "../Activity.h"

// Random maze game with three difficulty levels (Small / Medium / Large).
// Navigate from top-left to bottom-right using D-pad.
// Up/Down changes difficulty and regenerates maze. Confirm = new maze. Back = exit.
// Uses iterative DFS backtracker — all mazes are fully connected.
class MazeGameActivity final : public Activity {
 public:
  enum class Difficulty { SMALL = 0, MEDIUM = 1, LARGE = 2 };

  struct DiffConfig { int cols, rows, cellSize; const char* label; };
  static constexpr DiffConfig CONFIGS[] = {
    {15, 20, 24, "Small"},   // 15*24=360px (centered)
    {20, 27, 24, "Medium"},  // 20*24=480px (current default, fills screen)
    {30, 40, 16, "Large"},   // 30*16=480px (tight cells, challenging)
  };

 private:
  // Arrays sized to the largest difficulty (Large = 30×40)
  static constexpr int MAX_COLS = 30;
  static constexpr int MAX_ROWS = 40;

  // Wall bits per cell: bit0=N, bit1=E, bit2=S, bit3=W
  uint8_t walls[MAX_ROWS][MAX_COLS];
  bool visited[MAX_ROWS][MAX_COLS];

  Difficulty difficulty = Difficulty::MEDIUM;
  int playerX = 0;
  int playerY = 0;
  int moveCount = 0;
  bool victory = false;

  void generateMaze();
  bool hasWall(int x, int y, int dir) const;
  bool tryMove(int dx, int dy, int dir);

 public:
  explicit MazeGameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Maze", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
