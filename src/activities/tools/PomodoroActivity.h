#pragma once
#include "../Activity.h"
#include "util/ButtonNavigator.h"

class PomodoroActivity final : public Activity {
 public:
  enum class State { IDLE, FOCUS, SHORT_BREAK, LONG_BREAK, PAUSED };
  // Which duration is selected for editing in IDLE screen
  enum class IdleField { FOCUS = 0, SHORT_BREAK = 1, LONG_BREAK = 2 };

 private:
  ButtonNavigator buttonNavigator;

  State state = State::IDLE;
  State pausedFrom = State::FOCUS;

  uint32_t focusDurationMs = 25 * 60 * 1000;
  uint32_t shortBreakDurationMs = 5 * 60 * 1000;
  uint32_t longBreakDurationMs = 15 * 60 * 1000;
  uint32_t totalDurationMs = 0;

  unsigned long timerStartMs = 0;
  unsigned long pausedElapsedMs = 0;
  unsigned long lastRenderMs = 0;

  int completedSessions = 0;
  IdleField selectedField = IdleField::FOCUS;  // which row is highlighted in IDLE
  static constexpr int SESSIONS_BEFORE_LONG_BREAK = 4;

  uint32_t getRemainingMs() const;
  void startTimer(State newState, uint32_t durationMs);
  void advanceState();
  const char* getStateLabel() const;

 public:
  explicit PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Pomodoro", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state != State::IDLE; }
};
