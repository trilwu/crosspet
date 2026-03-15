#pragma once
#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ToolsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;

  int getMenuCount() const;

 public:
  explicit ToolsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tools", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
