#pragma once

#include "activities/Activity.h"
#include "MappedInputManager.h"
#include "tasks/Task.h"
#include "util/ButtonNavigator.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <vector>

class TasksActivity : public Activity {
 public:
  TasksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tasks", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State {
    Setup,         // tasks.json missing or todoistApiToken empty
    Loading,       // fetch in progress / awaiting WiFi
    ShowingTasks,
    ShowingError,
  };

  void startFetch();
  void proceedWithFetch();  // after WiFi is up

  StrId fetchResultToStrId() const;

  void renderSetup();
  void renderLoading();
  void renderError();
  void renderTaskList();
  void renderMinimal();
  void renderDaily();
  void drawTaskRows(int contentTop, int contentHeight, int tileX, int tileWidth);

  std::vector<tasks::Task> _tasks;
  State _state = State::Setup;
  int _scrollOffset = 0;
  int _selectedIndex = 0;
  int _lastVisibleIndex = -1;
  StrId _errorStrId = StrId::STR_TASKS_FETCH_FAILED;

  // Capture time at fetch for the header timestamp + per-row "today" comparison.
  // _today is empty if the clock hadn't synced — the Daily renderer falls back
  // to suppressing the date header in that case.
  uint8_t _capturedHour = 0;
  uint8_t _capturedMin = 0;
  uint8_t _capturedDay = 0;
  uint8_t _capturedMonth = 0;
  uint16_t _capturedYear = 0;
  uint8_t _capturedDow = 0;
  char _today[11] = "";

  // Snapshot of renderer orientation at onEnter() so we can restore it on exit.
  GfxRenderer::Orientation _entryOrientation = GfxRenderer::Orientation::Portrait;
  ButtonNavigator _navigator;
};
