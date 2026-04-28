#pragma once

#include <Arduino.h>
#include <deque>
#include <utility>

#include "../Activity.h"

// Snake — directional arcade game.
// D-pad changes direction. Confirm = new game. Back = exit.
// Difficulty controls snake speed.
class SnakeActivity final : public Activity {
  static constexpr int GRID_SIZE = 15;
  static constexpr int CELL = 28;

  enum class Difficulty : uint8_t { EASY = 0, MEDIUM, HARD };
  Difficulty difficulty = Difficulty::EASY;
  static constexpr int DIFFICULTY_COUNT = 3;

  enum class Direction : int8_t { NONE = -1, RIGHT = 0, LEFT, DOWN, UP };
  static constexpr int DIR_COUNT = 4;

  bool showingDifficultySelect = true;
  bool gameOver = false;

  std::deque<std::pair<int16_t, int16_t>> snake;  // head at front
  Direction direction = Direction::RIGHT;
  Direction nextDirection = Direction::RIGHT;
  std::pair<int16_t, int16_t> food{0, 0};
  int score = 0;

  unsigned long lastMoveTime = 0;
  int speedMs = 200;

  void initGame();
  void spawnFood();
  void update();
  bool isValidCell(int16_t r, int16_t c) const;
  int delayMs() const;
  void renderDifficultySelect();

 public:
  explicit SnakeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Snake", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  const char* difficultyLabel() const;
};
