#pragma once

#include <cstdint>

#include "../Activity.h"

// Conway's Game of Life on e-ink. Bit-packed grid — 1 bit per cell.
// Confirm = pause/resume. Left = cycle pattern preset. Right = randomize.
// Up/Down = speed. Back = exit.
class GameOfLifeActivity final : public Activity {
  static constexpr int COLS = 80;
  static constexpr int ROWS = 120;
  static constexpr int CELL_SIZE = 6;
  static constexpr int BYTES_PER_ROW = (COLS + 7) / 8;   // 10
  static constexpr int GRID_BYTES = BYTES_PER_ROW * ROWS; // 1200 per buffer

  uint8_t gridA[GRID_BYTES];
  uint8_t gridB[GRID_BYTES];
  uint8_t* current = gridA;
  uint8_t* next = gridB;

  uint32_t generation = 0;
  int aliveCells = 0;   // updated after each step / preset load
  int speedIdx = 1;
  bool paused = false;
  unsigned long lastStepMs = 0;

  // Pattern presets (0 = random, 1-6 = named classics)
  static constexpr int PRESET_COUNT = 7;
  static constexpr const char* PRESET_NAMES[] = {
    "Random", "Glider", "Blinker", "Toad", "Beacon", "Pulsar", "R-pento"
  };
  int presetIdx = 0;

  static constexpr int SPEED_COUNT = 4;
  static constexpr unsigned long SPEED_MS[] = {500, 1000, 2000, 4000};

  void randomize();
  void step();
  void loadPreset(int idx);  // 0=random; 1-6=named patterns
  int countAliveCells() const;
  int countNeighbors(int x, int y) const;
  bool getCell(const uint8_t* grid, int x, int y) const;
  void setCell(uint8_t* grid, int x, int y, bool alive);

 public:
  explicit GameOfLifeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Game of Life", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
