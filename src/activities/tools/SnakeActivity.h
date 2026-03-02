#pragma once

#include <cstdint>
#include <deque>
#include <utility>

#include "../Activity.h"

// Auto-advancing Snake game. Snake moves automatically on a timer.
// D-pad queues the next direction (applied on next auto-step).
// Up/Down adjusts speed. Confirm = restart. Back = exit.
class SnakeActivity final : public Activity {
  static constexpr int COLS = 30;
  static constexpr int ROWS = 40;
  static constexpr int CELL_SIZE = 16;  // 30*16=480 fits screen width exactly

  // Speed tiers: ms between automatic steps (e-ink safe — min ~350ms)
  static constexpr unsigned long SPEED_MS[] = {2000, 1200, 700, 350};
  static constexpr int SPEED_COUNT = 4;
  static constexpr const char* SPEED_LABELS[] = {"Slow", "Med", "Fast", "Turbo"};

  // Snake body: head at front(), tail at back()
  std::deque<std::pair<int8_t, int8_t>> body;
  // O(1) collision check grid
  bool occupied[ROWS][COLS];

  int8_t foodX = 0;
  int8_t foodY = 0;
  int8_t dirX = 1;      // current move direction
  int8_t dirY = 0;
  int8_t nextDirX = 1;  // queued direction (applied on next auto-step)
  int8_t nextDirY = 0;
  bool gameOver = false;
  int score = 0;
  int highScore = 0;    // session high score

  int speedIdx = 0;             // index into SPEED_MS[] — default Slow (e-ink friendly)
  unsigned long lastMoveMs = 0;

  void doMove();
  void spawnFood();

 public:
  explicit SnakeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Snake", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
