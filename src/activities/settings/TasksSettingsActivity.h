#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

#include <GfxRenderer.h>

// Per-app settings for the Tasks (To-Do List) activity. Launched from
// within TasksActivity via the Left or Right front button — no entry in
// the main SettingsActivity, mirroring crosspet's WeatherActivity
// pattern of keeping app preferences inside the owning activity.
class TasksSettingsActivity final : public Activity {
 public:
  TasksSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TasksSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void handleSelection();

  ButtonNavigator buttonNavigator;
  size_t selectedIndex = 0;
  static constexpr int kItemCount = 7;
};
