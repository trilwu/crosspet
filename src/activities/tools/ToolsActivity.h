#pragma once
#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ToolsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;

  static constexpr int MENU_COUNT = 10;

 public:
  explicit ToolsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tools", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
