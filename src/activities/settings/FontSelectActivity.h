#pragma once

#include <GfxRenderer.h>
#include <I18n.h>

#include "../Activity.h"
#include "components/UITheme.h"
#include "util/ButtonNavigator.h"

class MappedInputManager;

/**
 * Activity for selecting external CJK font from SD card /fonts/ directory.
 * Supports two modes: Reader font (for book content) and UI font (for menus).
 */
class FontSelectActivity final : public Activity {
 public:
  enum class SelectMode { Reader, UI };

  explicit FontSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, SelectMode mode)
      : Activity("FontSelect", renderer, mappedInput), mode(mode) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  SelectMode mode;
  int selectedIndex = 0;
  int totalItems = 1;
  ButtonNavigator buttonNavigator;

  void handleSelection();
};
