#pragma once
#include "../Activity.h"

// Full-screen reading statistics viewer.
// Mirrors the Reading Stats sleep screen but is accessible from the Tools menu.
// Back button exits to Tools menu.
class ReadingStatsActivity final : public Activity {
 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
