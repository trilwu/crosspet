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

void GameOfLifeActivity::randomize() {
  for (int i = 0; i < GRID_BYTES; i++) {
    // ~30% alive: each bit alive if random < 77 (out of 256)
    uint8_t rnd = (uint8_t)(esp_random() & 0xFF);
    gridA[i] = rnd & (uint8_t)(esp_random() & 0xFF);  // AND reduces density ~25%
  }
  current = gridA;
  next = gridB;
  generation = 0;
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
}

// --- Lifecycle ---

void GameOfLifeActivity::onEnter() {
  Activity::onEnter();
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
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) && paused) {
    step();
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
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

  // Draw alive cells as filled CELL_SIZE squares
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (getCell(current, x, y)) {
        renderer.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
      }
    }
  }

  // HUD bar at bottom (grid uses top 720px, bottom 80px free)
  const int hudY = ROWS * CELL_SIZE;  // 720
  char genStr[24];
  snprintf(genStr, sizeof(genStr), "Gen %lu", (unsigned long)generation);

  const char* speedStr = speedIdx == 0 ? "Fast" : speedIdx == 1 ? "Med" :
                         speedIdx == 2 ? "Slow" : "Slowest";
  const char* pauseStr = paused ? "Resume" : "Pause";

  // Draw a thin separator line
  renderer.fillRect(0, hudY, renderer.getScreenWidth(), 1);

  // Text labels in hud area
  const int textY = hudY + 10;
  renderer.drawText(SMALL_FONT_ID, 8, textY, genStr);

  char speedLabel[16];
  snprintf(speedLabel, sizeof(speedLabel), "[%s]", speedStr);
  renderer.drawCenteredText(SMALL_FONT_ID, textY, speedLabel);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), pauseStr, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
