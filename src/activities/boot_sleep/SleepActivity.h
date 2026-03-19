#pragma once
#include "../Activity.h"
#include "util/SleepScreenCache.h"

class Bitmap;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sleep", renderer, mappedInput) {}
  void onEnter() override;

 private:
  bool displayCachedSleepScreen(const std::string& sourcePath) const;
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap, const std::string& sourcePath) const;
  void renderBlankSleepScreen() const;
  void renderClockSleepScreen() const;
  void renderReadingStatsSleepScreen() const;
  void renderOverlaySleepScreen() const;
  // Shared pet + attention indicator helper (scale 1 = 48px, scale 2 = 96px)
  void renderSleepPet(int scale = 2) const;
};
