#pragma once

#include <cstdint>

#include "../Activity.h"

// Classic 2048 sliding tile game on e-ink.
// D-pad swipes tiles in 4 directions. Confirm = new game. Back = exit.
// Tile values stored as uint32_t to safely handle values beyond 65535.
class TwentyFortyEightActivity final : public Activity {
  static constexpr int SIZE = 4;
  static constexpr int TILE = 108;  // px per tile
  static constexpr int GAP = 8;     // px between tiles

  uint32_t grid[SIZE][SIZE];  // 0 = empty
  uint32_t score = 0;
  bool gameOver = false;
  bool won = false;  // reached 2048 tile (can continue playing)

  // Core slide logic: left is the canonical direction; others transform + use it
  bool slideLeft();
  bool slideRight();
  bool slideUp();
  bool slideDown();

  void spawnTile();
  bool hasMovesLeft() const;

  // Idle timeout: auto-exit after 5 min of no input
  unsigned long lastInputMs = 0;
  static constexpr unsigned long IDLE_TIMEOUT_MS = 5UL * 60UL * 1000UL;

 public:
  explicit TwentyFortyEightActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("2048", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
