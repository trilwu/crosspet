#pragma once

#include "activities/Activity.h"

// Shows device status info (RAM, flash, version, uptime) with reboot option
class DeviceInfoActivity : public Activity {
 public:
  explicit DeviceInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DeviceInfo", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  bool needsRefresh = true;
  bool waitForRelease = true;  // Wait for Confirm release before accepting input
};
