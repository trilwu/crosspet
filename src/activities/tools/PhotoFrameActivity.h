#pragma once

#include <string>
#include <vector>

#include "../Activity.h"

// BMP slideshow from /photos/ directory on SD card.
// Left/Right = prev/next photo. Up/Down = change auto-advance interval. Back = exit.
class PhotoFrameActivity final : public Activity {
  static constexpr uint32_t INTERVALS_MS[] = {30000, 60000, 120000, 300000};
  static constexpr int INTERVAL_COUNT = 4;

  std::vector<std::string> photos;
  int currentIndex = 0;
  int intervalIdx = 1;           // default: 60s
  unsigned long lastAdvanceMs = 0;
  bool showOverlay = false;
  unsigned long overlayEndMs = 0;

  void scanPhotos();

 public:
  explicit PhotoFrameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Photo Frame", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
