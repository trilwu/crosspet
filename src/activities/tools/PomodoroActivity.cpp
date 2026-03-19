#include "PomodoroActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "pet/PetManager.h"

#include "components/UITheme.h"
#include "fontIds.h"

#include <cmath>

// Draw a circular progress ring. Progress 0.0–1.0, starts from 12 o'clock clockwise.
// Draws a full background ring (outline), then fills the progress arc.
static void drawProgressRing(GfxRenderer& renderer, int cx, int cy, int radius, int thickness, float progress) {
  const int outerR = radius;
  const int innerR = radius - thickness;
  const int outerSq = outerR * outerR;
  const int innerSq = innerR * innerR;

  for (int dy = -outerR; dy <= outerR; dy++) {
    for (int dx = -outerR; dx <= outerR; dx++) {
      const int distSq = dx * dx + dy * dy;
      if (distSq > outerSq || distSq < innerSq) continue;

      // Angle from 12 o'clock, clockwise: atan2(dx, -dy)
      float angle = atan2f((float)dx, (float)-dy);  // -pi to pi, 0 = top
      if (angle < 0) angle += 2.0f * M_PI;
      const float progressAngle = progress * 2.0f * M_PI;

      if (angle <= progressAngle) {
        // Filled progress portion
        renderer.drawPixel(cx + dx, cy + dy, true);
      } else {
        // Background ring: draw only the outline (outermost and innermost pixels)
        const int distSqInner1 = (innerR + 1) * (innerR + 1);
        const int distSqOuter1 = (outerR - 1) * (outerR - 1);
        if (distSq >= distSqOuter1 || distSq <= distSqInner1) {
          renderer.drawPixel(cx + dx, cy + dy, true);
        }
      }
    }
  }
}

uint32_t PomodoroActivity::getRemainingMs() const {
  if (state == State::IDLE) return totalDurationMs;
  if (state == State::PAUSED) return totalDurationMs - pausedElapsedMs;

  unsigned long elapsed = pausedElapsedMs + (millis() - timerStartMs);
  if (elapsed >= totalDurationMs) return 0;
  return totalDurationMs - elapsed;
}

void PomodoroActivity::startTimer(State newState, uint32_t durationMs) {
  state = newState;
  totalDurationMs = durationMs;
  pausedElapsedMs = 0;
  timerStartMs = millis();
  lastRenderMs = millis();
  requestUpdate();
}

void PomodoroActivity::advanceState() {
  if (state == State::FOCUS || (state == State::PAUSED && pausedFrom == State::FOCUS)) {
    completedSessions++;
    if (PET_MANAGER.exists() && PET_MANAGER.isAlive()) {
      PET_MANAGER.onPomodoroComplete();
    }
    if (completedSessions >= SESSIONS_BEFORE_LONG_BREAK) {
      completedSessions = 0;
      startTimer(State::LONG_BREAK, longBreakDurationMs);
    } else {
      startTimer(State::SHORT_BREAK, shortBreakDurationMs);
    }
  } else {
    // After any break, start focus
    startTimer(State::FOCUS, focusDurationMs);
  }
}

const char* PomodoroActivity::getStateLabel() const {
  switch (state) {
    case State::IDLE:
      return tr(STR_READY);
    case State::FOCUS:
      return tr(STR_FOCUS);
    case State::SHORT_BREAK:
      return tr(STR_SHORT_BREAK);
    case State::LONG_BREAK:
      return tr(STR_LONG_BREAK);
    case State::PAUSED:
      return tr(STR_PAUSED);
  }
  return "";
}

void PomodoroActivity::onEnter() {
  Activity::onEnter();
  state = State::IDLE;
  totalDurationMs = focusDurationMs;
  completedSessions = 0;
  pausedElapsedMs = 0;
  selectedField = IdleField::FOCUS;
  requestUpdate();
}

void PomodoroActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (state) {
      case State::IDLE:
        // Cycle which duration field is selected for editing
        selectedField = (IdleField)(((int)selectedField + 1) % 3);
        requestUpdate();
        break;
      case State::FOCUS:
      case State::SHORT_BREAK:
      case State::LONG_BREAK:
        pausedFrom = state;
        pausedElapsedMs += (millis() - timerStartMs);
        state = State::PAUSED;
        requestUpdate();
        break;
      case State::PAUSED:
        state = pausedFrom;
        timerStartMs = millis();
        lastRenderMs = millis();
        requestUpdate();
        break;
    }
  }

  // In IDLE: Right = Start (check FIRST to avoid button conflict with navigator);
  // Up/Down adjusts the selected duration field
  if (state == State::IDLE) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      startTimer(State::FOCUS, focusDurationMs);
      return;
    }

    // Use direct Up/Down checks — NOT buttonNavigator which also triggers on Right
    auto adjustField = [&](int delta) {
      constexpr uint32_t STEP = 5 * 60 * 1000;
      constexpr uint32_t MIN_MS = 5 * 60 * 1000;
      constexpr uint32_t MAX_MS = 60 * 60 * 1000;
      uint32_t* target = nullptr;
      if (selectedField == IdleField::FOCUS)       target = &focusDurationMs;
      else if (selectedField == IdleField::SHORT_BREAK) target = &shortBreakDurationMs;
      else                                          target = &longBreakDurationMs;
      uint32_t next = *target + (uint32_t)(delta * (int)STEP);
      if (next >= MIN_MS && next <= MAX_MS) {
        *target = next;
        totalDurationMs = focusDurationMs;
        requestUpdate();
      }
    };
    if (mappedInput.wasReleased(MappedInputManager::Button::Up))   adjustField(-1);
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) adjustField(+1);
  }

  // Skip button (Right) during running or paused states
  if (state != State::IDLE && mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    advanceState();
    return;
  }

  // Check if timer expired
  if (state == State::FOCUS || state == State::SHORT_BREAK || state == State::LONG_BREAK) {
    if (getRemainingMs() == 0) {
      advanceState();
      return;
    }

    // Refresh display: every 30s normally, every 1s in last 60 seconds
    uint32_t remaining = getRemainingMs();
    unsigned long refreshInterval = (remaining <= 60000) ? 1000 : 30000;
    if (millis() - lastRenderMs >= refreshInterval) {
      lastRenderMs = millis();
      requestUpdate();
    }
  }
}

void PomodoroActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_POMODORO));

  // State label centered
  const int contentTop = metrics.topPadding + metrics.headerHeight;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight;
  const int contentCenter = (contentTop + contentBottom) / 2;

  const int labelHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int timeHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int dotSize = 12;
  const int dotSpacing = 8;
  const int totalBlockHeight = labelHeight + 10 + timeHeight + 20 + dotSize;
  const int blockTop = contentCenter - totalBlockHeight / 2;

  if (state == State::IDLE) {
    // IDLE: show all three durations as an editable list
    // Confirm = select field, Up/Down = adjust, Right = Start
    const int rowH = labelHeight + 8;
    const int listTop = contentCenter - (3 * rowH) / 2;
    const int labelX = pageWidth / 2 - 80;  // left-align labels at ~center-80

    struct { const char* name; uint32_t ms; IdleField field; } rows[3] = {
      {tr(STR_POMO_FOCUS_LABEL), focusDurationMs,      IdleField::FOCUS},
      {tr(STR_POMO_SHORT_BREAK), shortBreakDurationMs, IdleField::SHORT_BREAK},
      {tr(STR_POMO_LONG_BREAK),  longBreakDurationMs,  IdleField::LONG_BREAK},
    };

    for (int i = 0; i < 3; i++) {
      const int y = listTop + i * rowH;
      const bool selected = (rows[i].field == selectedField);
      char buf[48];
      snprintf(buf, sizeof(buf), tr(STR_POMO_DURATION_FMT), rows[i].name, (unsigned long)(rows[i].ms / 60000));
      if (selected) {
        // Black fill selection — CrossPet theme style
        renderer.fillRect(0, y - 2, pageWidth, rowH, true);
        renderer.drawText(UI_10_FONT_ID, labelX, y, buf, false);
      } else {
        renderer.drawText(UI_10_FONT_ID, labelX, y, buf, true);
      }
    }
  } else {
    // Running / paused: circular progress ring with countdown inside + session dots

    // Calculate progress (0.0 = just started, 1.0 = done)
    uint32_t remaining = getRemainingMs();
    float elapsed = (float)(totalDurationMs - remaining);
    float progress = totalDurationMs > 0 ? elapsed / (float)totalDurationMs : 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    // Ring dimensions
    constexpr int RING_RADIUS = 100;
    constexpr int RING_THICKNESS = 8;
    const int ringCenterX = pageWidth / 2;
    const int ringCenterY = contentCenter - 20;

    // Draw progress ring
    drawProgressRing(renderer, ringCenterX, ringCenterY, RING_RADIUS, RING_THICKNESS, progress);

    // State label inside ring (above center)
    renderer.drawCenteredText(UI_10_FONT_ID, ringCenterY - timeHeight / 2 - labelHeight - 2, getStateLabel(), true, EpdFontFamily::BOLD);

    // Countdown inside ring (centered)
    int minutes = remaining / 60000;
    int seconds = (remaining % 60000) / 1000;
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", minutes, seconds);
    renderer.drawCenteredText(UI_12_FONT_ID, ringCenterY - timeHeight / 2, timeBuf, true, EpdFontFamily::BOLD);

    // Session dots below ring
    const int dotsY = ringCenterY + RING_RADIUS + 20;
    const int totalDotsWidth = SESSIONS_BEFORE_LONG_BREAK * dotSize + (SESSIONS_BEFORE_LONG_BREAK - 1) * dotSpacing;
    const int dotsX = (pageWidth - totalDotsWidth) / 2;
    for (int i = 0; i < SESSIONS_BEFORE_LONG_BREAK; i++) {
      int x = dotsX + i * (dotSize + dotSpacing);
      if (i < completedSessions) renderer.fillRect(x, dotsY, dotSize, dotSize, true);
      else                       renderer.drawRect(x, dotsY, dotSize, dotSize, true);
    }
  }

  const char* btn1 = tr(STR_BACK);
  const char* btn2 = "";
  const char* btn3 = "";
  const char* btn4 = "";

  switch (state) {
    case State::IDLE:
      btn2 = tr(STR_SELECT);
      btn3 = tr(STR_DIR_UP);
      btn4 = tr(STR_POMO_START);
      break;
    case State::FOCUS:
    case State::SHORT_BREAK:
    case State::LONG_BREAK:
      btn2 = tr(STR_PAUSE);
      btn4 = tr(STR_SKIP);
      break;
    case State::PAUSED:
      btn2 = tr(STR_RESUME);
      btn4 = tr(STR_SKIP);
      break;
  }

  const auto labels = mappedInput.mapLabels(btn1, btn2, btn3, btn4);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
