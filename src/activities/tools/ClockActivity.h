#pragma once
#include "../Activity.h"

class ClockActivity final : public Activity {
  unsigned long lastUpdateMs = 0;
  bool timeAvailable = false;

  // Manual time-set mode
  bool editing = false;
  int editField = 0;  // 0=hour, 1=minute, 2=day, 3=month, 4=year
  struct tm editTime = {};

  // Month navigation (0 = current month, +1 = next, -1 = prev)
  int monthOffset = 0;

  void applyEditedTime();
  void renderCalendar(int startY, const struct tm& t, bool isCurrentMonth) const;

 public:
  explicit ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Clock", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
