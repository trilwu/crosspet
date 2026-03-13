#pragma once

#include "../Activity.h"
#include "fontIds.h"
#include "util/SleepScreenCache.h"

/// Simple activity that clears the sleep screen cache and shows confirmation.
class SleepCacheClearActivity final : public Activity {
  bool rendered = false;

 public:
  explicit SleepCacheClearActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SleepCacheClear", renderer, mappedInput) {}

  void onEnter() override {
    Activity::onEnter();
    SleepScreenCache::invalidateAll();
    requestUpdate();
  }

  void loop() override {
    // Wait until the confirmation has been rendered at least once
    if (rendered && mappedInput.wasAnyReleased()) {
      finish();
    }
  }

  void render(RenderLock&&) override {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID,
                              renderer.getScreenHeight() / 2 - renderer.getLineHeight(UI_10_FONT_ID) / 2,
                              tr(STR_SLEEP_CACHE_CLEARED));
    rendered = true;
  }
};
