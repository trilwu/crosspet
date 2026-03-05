#pragma once

#include <cstdint>

#include "../Activity.h"

// Caro (Gomoku) — human (X) vs AI (O). First to 5-in-a-row wins.
// D-pad moves cursor. Confirm = place stone. Back = exit.
// AI difficulty: Easy=random, Medium=block 3-in-a-row, Hard=block 4 & build 4.
class CaroActivity final : public Activity {
  static constexpr int SIZE = 15;
  static constexpr int CELL = 28;

  uint8_t grid[SIZE][SIZE];  // 0=empty, 1=X(human), 2=O(AI)
  int cursorRow = 0;
  int cursorCol = 0;
  bool gameOver = false;
  uint8_t winner = 0;  // 0=draw, 1=X won, 2=O won
  int moveCount = 0;

  enum class Difficulty : uint8_t { EASY = 0, MEDIUM, HARD };
  Difficulty difficulty = Difficulty::EASY;
  static constexpr int DIFFICULTY_COUNT = 3;

  bool showingDifficultySelect = true;  // pre-game difficulty selection screen

  bool checkWin(int row, int col, uint8_t player) const;
  int countDirection(int row, int col, int dr, int dc, uint8_t player) const;
  void doAiMove();
  int scorePosition(int row, int col, uint8_t player) const;
  const char* difficultyLabel() const;
  void initBoard();
  void renderDifficultySelect();

 public:
  explicit CaroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Caro", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
