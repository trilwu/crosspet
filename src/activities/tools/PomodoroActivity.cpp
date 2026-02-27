#include "PomodoroActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

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
        startTimer(State::FOCUS, focusDurationMs);
        break;
      case State::FOCUS:
      case State::SHORT_BREAK:
      case State::LONG_BREAK:
        // Pause
        pausedFrom = state;
        pausedElapsedMs += (millis() - timerStartMs);
        state = State::PAUSED;
        requestUpdate();
        break;
      case State::PAUSED:
        // Resume
        state = pausedFrom;
        timerStartMs = millis();
        lastRenderMs = millis();
        requestUpdate();
        break;
    }
  }

  // Up/Down adjust duration in IDLE state
  if (state == State::IDLE) {
    buttonNavigator.onNext([this] {
      if (focusDurationMs > 5 * 60 * 1000) {
        focusDurationMs -= 5 * 60 * 1000;
        totalDurationMs = focusDurationMs;
        requestUpdate();
      }
    });
    buttonNavigator.onPrevious([this] {
      if (focusDurationMs < 60 * 60 * 1000) {
        focusDurationMs += 5 * 60 * 1000;
        totalDurationMs = focusDurationMs;
        requestUpdate();
      }
    });
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

    // Refresh display: every 60s normally, every 1s in last 60 seconds
    uint32_t remaining = getRemainingMs();
    unsigned long refreshInterval = (remaining <= 60000) ? 1000 : 60000;
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

  renderer.drawCenteredText(UI_10_FONT_ID, blockTop, getStateLabel(), true, EpdFontFamily::BOLD);

  // Large countdown MM:SS
  uint32_t remaining = getRemainingMs();
  int minutes = remaining / 60000;
  int seconds = (remaining % 60000) / 1000;
  char timeBuf[6];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", minutes, seconds);
  renderer.drawCenteredText(UI_12_FONT_ID, blockTop + labelHeight + 10, timeBuf, true, EpdFontFamily::BOLD);

  // Session dots: 4 small squares
  const int dotsY = blockTop + labelHeight + 10 + timeHeight + 20;
  const int totalDotsWidth = SESSIONS_BEFORE_LONG_BREAK * dotSize + (SESSIONS_BEFORE_LONG_BREAK - 1) * dotSpacing;
  const int dotsX = (pageWidth - totalDotsWidth) / 2;

  for (int i = 0; i < SESSIONS_BEFORE_LONG_BREAK; i++) {
    int x = dotsX + i * (dotSize + dotSpacing);
    if (i < completedSessions) {
      renderer.fillRect(x, dotsY, dotSize, dotSize, true);
    } else {
      renderer.drawRect(x, dotsY, dotSize, dotSize, true);
    }
  }

  // Button hints vary by state
  const char* btn1 = tr(STR_BACK);
  const char* btn2 = "";
  const char* btn3 = "";
  const char* btn4 = "";

  switch (state) {
    case State::IDLE:
      btn2 = tr(STR_START);
      btn3 = tr(STR_DIR_UP);
      btn4 = tr(STR_DIR_DOWN);
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
