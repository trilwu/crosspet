#pragma once

#include <cstdint>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Classic 9x9 Sudoku puzzle game for e-ink.
// D-pad moves cursor. Confirm cycles number (0-9). Back = exit.
class SudokuActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  static constexpr int SIZE = 9;
  static constexpr int BOX = 3;

  // Puzzle state
  uint8_t puzzle[SIZE][SIZE];   // current grid (0 = empty)
  uint8_t solution[SIZE][SIZE]; // full solution
  bool fixed[SIZE][SIZE];       // true = given clue (not editable)

  int cursorRow = 0;
  int cursorCol = 0;
  bool completed = false;
  bool showConflicts = true;

  // Difficulty: number of cells to remove from the solution
  enum class Difficulty : uint8_t { EASY = 0, MEDIUM, HARD };
  Difficulty difficulty = Difficulty::EASY;
  static constexpr int REMOVE_COUNT[] = {35, 45, 52};
  static constexpr int DIFFICULTY_COUNT = 3;

  // Puzzle generation
  void generatePuzzle();
  bool fillGrid(int pos);
  bool isValidPlacement(const uint8_t grid[][SIZE], int row, int col, uint8_t num) const;
  bool checkCompleted() const;
  const char* difficultyLabel() const;

 public:
  explicit SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sudoku", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
