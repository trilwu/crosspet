#include "SnakeActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdlib>
#include <cstring>

#include "components/UITheme.h"
#include "fontIds.h"

void SnakeActivity::initGame() {
  snake.clear();
  int mid = GRID_SIZE / 2;
  snake.emplace_back(mid, mid);
  snake.emplace_back(mid, mid - 1);
  snake.emplace_back(mid, mid - 2);
  direction = Direction::RIGHT;
  nextDirection = Direction::RIGHT;
  score = 0;
  gameOver = false;
  lastMoveTime = millis();
  randomSeed(millis());
  spawnFood();
}

void SnakeActivity::spawnFood() {
  int attempts = 0;
  while (attempts < 500) {
    int16_t r = random(0, GRID_SIZE);
    int16_t c = random(0, GRID_SIZE);
    bool occupied = false;
    for (const auto& seg : snake) {
      if (seg.first == r && seg.second == c) {
        occupied = true;
        break;
      }
    }
    if (!occupied) {
      food = {r, c};
      return;
    }
    attempts++;
  }
  // Fallback: linear scan
  for (int r = 0; r < GRID_SIZE && food.first >= 0; r++) {
    for (int c = 0; c < GRID_SIZE && food.first < 0; c++) {
      bool occupied = false;
      for (const auto& seg : snake) {
        if (seg.first == r && seg.second == c) {
          occupied = true;
          break;
        }
      }
      if (!occupied) food = {r, c};
    }
  }
}

bool SnakeActivity::isValidCell(int16_t r, int16_t c) const {
  return r >= 0 && r < GRID_SIZE && c >= 0 && c < GRID_SIZE;
}

void SnakeActivity::update() {
  if (gameOver) return;

  direction = nextDirection;

  auto [headR, headC] = snake.front();

  int dr = 0, dc = 0;
  switch (direction) {
    case Direction::UP:    dr = -1; dc = 0;  break;
    case Direction::DOWN:  dr = 1;  dc = 0;  break;
    case Direction::LEFT:  dr = 0;  dc = -1; break;
    case Direction::RIGHT: dr = 0;  dc = 1;  break;
    default: return;
  }

  int16_t newR = static_cast<int16_t>(headR + dr);
  int16_t newC = static_cast<int16_t>(headC + dc);

  // Teleport (wrap) at borders
  if (newR < 0)       newR = GRID_SIZE - 1;
  if (newR >= GRID_SIZE) newR = 0;
  if (newC < 0)       newC = GRID_SIZE - 1;
  if (newC >= GRID_SIZE) newC = 0;

  // Check self collision (skip tail if not eating food — tail will move)
  bool eating = (newR == food.first && newC == food.second);
  int tailSize = eating ? (int)snake.size() : (int)snake.size() - 1;
  for (int i = 0; i < tailSize; i++) {
    if (snake[i].first == newR && snake[i].second == newC) {
      gameOver = true;
      return;
    }
  }

  snake.insert(snake.begin(), {newR, newC});

  if (eating) {
    score++;
    spawnFood();
  } else {
    snake.pop_back();
  }
}

int SnakeActivity::delayMs() const {
  switch (difficulty) {
    case Difficulty::EASY:   return 800;
    case Difficulty::MEDIUM: return 600;
    case Difficulty::HARD:   return 450;
  }
  return 600;
}

const char* SnakeActivity::difficultyLabel() const {
  switch (difficulty) {
    case Difficulty::EASY:   return tr(STR_SNAKE_EASY);
    case Difficulty::MEDIUM: return tr(STR_SNAKE_MEDIUM);
    case Difficulty::HARD:   return tr(STR_SNAKE_HARD);
  }
  return "";
}

void SnakeActivity::onEnter() {
  Activity::onEnter();
  showingDifficultySelect = true;
  requestUpdate();
}

void SnakeActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // Difficulty selection screen
  if (showingDifficultySelect) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      difficulty = static_cast<Difficulty>(((int)difficulty - 1 + DIFFICULTY_COUNT) % DIFFICULTY_COUNT);
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      difficulty = static_cast<Difficulty>(((int)difficulty + 1) % DIFFICULTY_COUNT);
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      showingDifficultySelect = false;
      initGame();
      requestUpdate();
    }
    return;
  }

  // Game over: Confirm = difficulty select, Back = exit
  if (gameOver) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      showingDifficultySelect = true;
      requestUpdate();
    }
    return;
  }

  // Direction changes via D-pad
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) && direction != Direction::DOWN) {
    nextDirection = Direction::UP;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) && direction != Direction::UP) {
    nextDirection = Direction::DOWN;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) && direction != Direction::RIGHT) {
    nextDirection = Direction::LEFT;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) && direction != Direction::LEFT) {
    nextDirection = Direction::RIGHT;
  }

  // Time-based game tick
  unsigned long now = millis();
  int delay = delayMs();
  if (now - lastMoveTime >= delay) {
    lastMoveTime = now;
    update();
    requestUpdate();
  }
}

void SnakeActivity::renderDifficultySelect() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SNAKE));

  const int centerY = (metrics.topPadding + metrics.headerHeight + pageHeight - metrics.buttonHintsHeight) / 2;
  renderer.drawCenteredText(UI_12_FONT_ID, centerY - 40, tr(STR_SELECT_DIFFICULTY));

  char label[32];
  snprintf(label, sizeof(label), "< %s >", difficultyLabel());
  renderer.drawCenteredText(UI_10_FONT_ID, centerY, label, true, EpdFontFamily::BOLD);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void SnakeActivity::render(RenderLock&&) {
  if (showingDifficultySelect) {
    renderDifficultySelect();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SNAKE));

  const int gridW = CELL * GRID_SIZE;
  const int gridH = CELL * GRID_SIZE;
  const int gridX = (pageWidth - gridW) / 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight + 4;
  const int contentBot = pageHeight - metrics.buttonHintsHeight - 4;
  const int gridY = contentTop + (contentBot - contentTop - gridH) / 2;

  // Draw grid outline
  renderer.drawRect(gridX, gridY, gridW, gridH);

  // Draw food (small filled rect at center of cell)
  int foodPad = 6;
  renderer.fillRect(
    gridX + food.second * CELL + foodPad,
    gridY + food.first * CELL + foodPad,
    CELL - foodPad * 2,
    CELL - foodPad * 2
  );

  // Draw snake
  for (int i = 0; i < (int)snake.size(); i++) {
    auto [r, c] = snake[i];
    int x = gridX + c * CELL;
    int y = gridY + r * CELL;
    int pad = (i == 0) ? 2 : 3;  // head slightly larger

    if (i == 0) {
      // Head: solid black
      renderer.fillRect(x + pad, y + pad, CELL - pad * 2, CELL - pad * 2);
    } else {
      // Body: outlined with small gap
      renderer.drawRect(x + pad, y + pad, CELL - pad * 2, CELL - pad * 2);
      renderer.fillRect(x + pad, y + pad, CELL - pad * 2, 1);
      renderer.fillRect(x + pad, y + pad, 1, CELL - pad * 2);
    }
  }

  // Draw score above the grid
  char scoreBuf[8];
  snprintf(scoreBuf, sizeof(scoreBuf), "%d", score);
  int sw = renderer.getTextWidth(UI_10_FONT_ID, scoreBuf);
  renderer.drawText(UI_10_FONT_ID, gridX + (gridW - sw) / 2,
                    metrics.topPadding + metrics.headerHeight + 4, scoreBuf);

  // Draw game over overlay
  if (gameOver) {
    const char* msg = tr(STR_GAME_OVER);
    int msgW = renderer.getTextWidth(UI_12_FONT_ID, msg);
    int msgH = renderer.getLineHeight(UI_12_FONT_ID);
    int cardPadX = 20, cardPadY = 8;
    int cardW = msgW + cardPadX * 2;
    int cardH = msgH + cardPadY * 2;
    int cardX = (pageWidth - cardW) / 2;
    int cardY = gridY + gridH / 2 - cardH / 2 - 10;

    renderer.fillRoundedRect(cardX, cardY, cardW, cardH, 8, Color::Black);
    renderer.drawCenteredText(UI_12_FONT_ID, cardY + cardH / 2, msg, false, EpdFontFamily::BOLD);

    char scoreMsg[40];
    snprintf(scoreMsg, sizeof(scoreMsg), "%d", score);
    int smW = renderer.getTextWidth(SMALL_FONT_ID, scoreMsg);
    renderer.drawText(SMALL_FONT_ID, cardX + cardW / 2 - smW / 2, cardY + cardH + 8, scoreMsg);
  }

  // Button hints
  const char* btn1 = tr(STR_BACK);
  const char* btn2 = gameOver ? tr(STR_NEW_GAME) : "";
  const char* btn3 = tr(STR_DIR_UP);
  const char* btn4 = tr(STR_DIR_DOWN);
  const auto labels = mappedInput.mapLabels(btn1, btn2, btn3, btn4);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
