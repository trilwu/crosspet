#pragma once

#include "../Activity.h"
#include "components/CtosRenderer.h"
#include "util/ButtonNavigator.h"

// ctOS-style security toolkit home screen shown when Ghost Mode is enabled.
// Displays a 2x2 tool grid; navigates with next/prev, launches tools on select.
class GhostHomeActivity final : public Activity {
  CtosRenderer ctos;
  ButtonNavigator buttonNavigator;
  int selectedIdx = 0;

  static constexpr int TOOL_COUNT = 4;
  static constexpr const char* TOOL_LABELS[TOOL_COUNT] = {
      "WIFI\nSCANNER",
      "BLE\nSCANNER",
      "TRACKER\nDETECT",
      "COMING\nSOON",
  };

  void launchSelected();

 public:
  explicit GhostHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GhostHome", renderer, mappedInput), ctos(renderer) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
