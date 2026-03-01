#pragma once

#include <cstdint>
#include <deque>
#include <utility>

#include "../Activity.h"

// Turn-based Snake game. Each D-pad press = 1 cell move. Perfect for e-ink.
// Confirm = restart. Back = exit to Tools menu.
class SnakeActivity final : public Activity {
  static constexpr int COLS = 30;
  static constexpr int ROWS = 40;
  static constexpr int CELL_SIZE = 16;  // 30*16=480 fits screen width exactly

  // Snake body: head at front(), tail at back()
  std::deque<std::pair<int8_t, int8_t>> body;
  // O(1) collision check grid
  bool occupied[ROWS][COLS];

  int8_t foodX = 0;
  int8_t foodY = 0;
  int8_t dirX = 1;  // initial direction: right
  int8_t dirY = 0;
  bool gameOver = false;
  int score = 0;

  void doMove();
  void spawnFood();

 public:
  explicit SnakeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Snake", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
