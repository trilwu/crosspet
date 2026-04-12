#pragma once
#include <I18n.h>

#include <functional>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class ToolsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;

  // Dynamic menu entries built from enabled apps
  struct AppEntry {
    StrId labelId;
    std::function<void()> launch;
  };
  std::vector<AppEntry> menuEntries;

  void buildMenu();

 public:
  explicit ToolsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tools", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
