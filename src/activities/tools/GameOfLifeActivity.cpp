#include "GameOfLifeActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <cstring>
#include <esp_random.h>

#include "components/UITheme.h"
#include "fontIds.h"

// --- Grid helpers ---

bool GameOfLifeActivity::getCell(const uint8_t* grid, int x, int y) const {
  return (grid[y * BYTES_PER_ROW + x / 8] >> (x % 8)) & 1;
}

void GameOfLifeActivity::setCell(uint8_t* grid, int x, int y, bool alive) {
  uint8_t& byte = grid[y * BYTES_PER_ROW + x / 8];
  uint8_t bit = (uint8_t)(1 << (x % 8));
  if (alive) byte |= bit; else byte &= ~bit;
}

int GameOfLifeActivity::countNeighbors(int x, int y) const {
  int count = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = (x + dx + COLS) % COLS;
      int ny = (y + dy + ROWS) % ROWS;
      if (getCell(current, nx, ny)) count++;
    }
  }
  return count;
}

int GameOfLifeActivity::countAliveCells() const {
  int count = 0;
  for (int i = 0; i < GRID_BYTES; i++)
    count += __builtin_popcount(current[i]);
  return count;
}

void GameOfLifeActivity::randomize() {
  for (int i = 0; i < GRID_BYTES; i++) {
    uint8_t rnd = (uint8_t)(esp_random() & 0xFF);
    gridA[i] = rnd & (uint8_t)(esp_random() & 0xFF);  // AND reduces density ~25%
  }
  current = gridA;
  next = gridB;
  generation = 0;
  aliveCells = countAliveCells();
}

void GameOfLifeActivity::step() {
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      int n = countNeighbors(x, y);
      bool alive = getCell(current, x, y);
      bool nextAlive = alive ? (n == 2 || n == 3) : (n == 3);
      setCell(next, x, y, nextAlive);
    }
  }
  std::swap(current, next);
  generation++;
  aliveCells = countAliveCells();
}

// --- Pattern presets ---
// Each preset stamps a known pattern centered on the grid.

void GameOfLifeActivity::loadPreset(int idx) {
  memset(gridA, 0, sizeof(gridA));
  memset(gridB, 0, sizeof(gridB));
  current = gridA;
  next = gridB;
  generation = 0;

  // Helper: stamp a cell relative to grid center
  const int cx = COLS / 2;
  const int cy = ROWS / 2;
  auto set = [&](int dx, int dy) {
    int x = (cx + dx + COLS) % COLS;
    int y = (cy + dy + ROWS) % ROWS;
    setCell(current, x, y, true);
  };

  switch (idx) {
    case 0:  // Random
      randomize();
      return;

    case 1:  // Glider — 5 cells, travels diagonally
      set(0, -1); set(1, 0); set(-1, 1); set(0, 1); set(1, 1);
      break;

    case 2:  // Blinker — 3 cells, period-2 oscillator
      set(-1, 0); set(0, 0); set(1, 0);
      break;

    case 3:  // Toad — 6 cells, period-2
      set(0, 0); set(1, 0); set(2, 0);
      set(-1, 1); set(0, 1); set(1, 1);
      break;

    case 4:  // Beacon — 8 cells, period-2
      set(-2, -2); set(-1, -2); set(-2, -1);
      set(1, 0); set(2, 0); set(1, 1);
      set(-1, -1); set(2, 1);
      break;

    case 5:  // Pulsar — 48 cells, period-3 (a classic large oscillator)
      // Arms at ±2 and ±4 rows/cols from center
      for (int i : {-4, -3, -2, 2, 3, 4}) {
        set(i, -6); set(i, -1); set(i, 1); set(i, 6);
        set(-6, i); set(-1, i); set(1, i); set(6, i);
      }
      break;

    case 6:  // R-pentomino — 5 cells, chaotic long-lived pattern
      set(0, -1); set(1, -1); set(-1, 0); set(0, 0); set(0, 1);
      break;

    default:
      randomize();
      return;
  }

  aliveCells = countAliveCells();
}

// --- Lifecycle ---

void GameOfLifeActivity::onEnter() {
  Activity::onEnter();
  presetIdx = 0;
  randomize();
  lastStepMs = millis();
  requestUpdate();
}

void GameOfLifeActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finish(); return; }

  bool changed = false;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    paused = !paused;
    lastStepMs = millis();
    changed = true;
  }
  // Left = cycle to next preset
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    presetIdx = (presetIdx + 1) % PRESET_COUNT;
    loadPreset(presetIdx);
    paused = false;
    lastStepMs = millis();
    changed = true;
  }
  // Right = randomize (reset to random, same as preset 0)
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    presetIdx = 0;
    randomize();
    lastStepMs = millis();
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (speedIdx > 0) { speedIdx--; changed = true; }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (speedIdx < SPEED_COUNT - 1) { speedIdx++; changed = true; }
  }

  // Auto-advance when not paused
  if (!paused && (millis() - lastStepMs) >= SPEED_MS[speedIdx]) {
    step();
    lastStepMs = millis();
    changed = true;
  }

  if (changed) requestUpdate();
}

// --- Render ---

void GameOfLifeActivity::render(RenderLock&&) {
  renderer.clearScreen();

  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (getCell(current, x, y)) {
        renderer.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
      }
    }
  }

  // HUD bar at bottom (grid uses top 720px, bottom 80px free)
  const int hudY = ROWS * CELL_SIZE;  // 720
  renderer.fillRect(0, hudY, renderer.getScreenWidth(), 1);  // separator

  const int textY = hudY + 10;
  char genStr[24];
  snprintf(genStr, sizeof(genStr), "Gen %lu", (unsigned long)generation);
  renderer.drawText(SMALL_FONT_ID, 8, textY, genStr);

  // Center: preset name + alive count
  char centerStr[32];
  snprintf(centerStr, sizeof(centerStr), "%s | %d alive", PRESET_NAMES[presetIdx], aliveCells);
  renderer.drawCenteredText(SMALL_FONT_ID, textY, centerStr);

  const char* speedStr = speedIdx == 0 ? "Fast" : speedIdx == 1 ? "Med" :
                         speedIdx == 2 ? "Slow" : "Slowest";
  const char* pauseStr = paused ? "Resume" : "Pause";

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), pauseStr, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
