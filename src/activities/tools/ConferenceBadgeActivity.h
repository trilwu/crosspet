#pragma once
#include <string>

#include "../Activity.h"

class ConferenceBadgeActivity final : public Activity {
  std::string badgeName;
  std::string badgeTitle;
  std::string qrData;
  bool fileLoaded = false;

 public:
  explicit ConferenceBadgeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Badge", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  // No preventAutoSleep — static display, device should sleep normally
};
