#pragma once

#include <cstdint>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Classic Minesweeper for e-ink. 10x14 grid.
// D-pad moves cursor. Confirm tap = reveal. Confirm long-press = toggle flag. Back = exit.
class MinesweeperActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  static constexpr int COLS = 10;
  static constexpr int ROWS = 14;
  static constexpr int CELL = 32;

  enum class CellState : uint8_t { HIDDEN, REVEALED, FLAGGED };

  // Each cell: adjacent mine count (0-8) or 9 = mine
  uint8_t grid[ROWS][COLS];
  CellState state[ROWS][COLS];

  int cursorRow = 0;
  int cursorCol = 0;
  int mineCount = 20;
  int flagCount = 0;
  int revealedCount = 0;
  bool gameOver = false;
  bool won = false;
  bool firstMove = true;  // mines placed after first reveal to avoid instant death

  enum class Difficulty : uint8_t { EASY = 0, MEDIUM, HARD };
  Difficulty difficulty = Difficulty::EASY;
  static constexpr int MINE_COUNTS[] = {15, 25, 35};
  static constexpr int DIFFICULTY_COUNT = 3;

  void placeMines(int safeRow, int safeCol);
  void reveal(int row, int col);
  void revealAll();
  int countAdjacentMines(int row, int col) const;
  int countAdjacentFlags(int row, int col) const;
  bool checkWin() const;
  const char* difficultyLabel() const;

 public:
  explicit MinesweeperActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Minesweeper", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
