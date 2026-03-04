#include "PomodoroActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "pet/PetManager.h"

#include "components/UITheme.h"
#include "fontIds.h"

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

  // In IDLE: Up/Down adjusts the selected duration field; Right = Start
  if (state == State::IDLE) {
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
    buttonNavigator.onNext([&] { adjustField(-1); });    // Up = decrease
    buttonNavigator.onPrevious([&] { adjustField(+1); }); // Down = increase

    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      startTimer(State::FOCUS, focusDurationMs);
      return;
    }
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
      {"Focus:      ", focusDurationMs,      IdleField::FOCUS},
      {"Short break:", shortBreakDurationMs, IdleField::SHORT_BREAK},
      {"Long break: ", longBreakDurationMs,  IdleField::LONG_BREAK},
    };

    for (int i = 0; i < 3; i++) {
      const int y = listTop + i * rowH;
      const bool selected = (rows[i].field == selectedField);
      char buf[32];
      snprintf(buf, sizeof(buf), "%s %2lu min", rows[i].name, (unsigned long)(rows[i].ms / 60000));
      // Draw selection arrow + bold text for selected row
      if (selected) {
        renderer.drawText(SMALL_FONT_ID, labelX - 14, y, ">");
        renderer.drawText(UI_10_FONT_ID, labelX, y, buf);
      } else {
        renderer.drawText(SMALL_FONT_ID, labelX, y, buf);
      }
    }
  } else {
    // Running / paused: show state label + large countdown + session dots
    renderer.drawCenteredText(UI_10_FONT_ID, blockTop, getStateLabel(), true, EpdFontFamily::BOLD);

    uint32_t remaining = getRemainingMs();
    int minutes = remaining / 60000;
    int seconds = (remaining % 60000) / 1000;
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", minutes, seconds);
    renderer.drawCenteredText(UI_12_FONT_ID, blockTop + labelHeight + 10, timeBuf, true, EpdFontFamily::BOLD);

    const int dotsY = blockTop + labelHeight + 10 + timeHeight + 20;
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
      btn2 = "Select";
      btn3 = tr(STR_DIR_UP);
      btn4 = "Start >";
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
