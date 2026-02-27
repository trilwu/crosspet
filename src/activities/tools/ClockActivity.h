#pragma once
#include "../Activity.h"

class ClockActivity final : public Activity {
  unsigned long lastUpdateMs = 0;
  bool timeAvailable = false;

 public:
  explicit ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Clock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  // No preventAutoSleep — static display, device should sleep normally
};
