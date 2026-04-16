#pragma once

#include <GfxRenderer.h>
#include <I18n.h>

#include "../Activity.h"
#include "components/UITheme.h"
#include "util/ButtonNavigator.h"

class MappedInputManager;

/**
 * Activity for selecting external font from SD card /fonts/ directory.
 * Supports two modes: Reader font (for book content) and UI font (for menus).
 * For reader fonts, selecting an external font shows a sub-screen to choose
 * render priority: Primary (external first) or Supplement (fills built-in gaps).
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
  enum class Phase { FontList, RenderMode };

  SelectMode mode;
  Phase phase = Phase::FontList;
  int selectedIndex = 0;
  int totalItems = 1;
  int pendingExternalIndex = -1;
  int renderModeIndex = 0;  // 0 = Primary, 1 = Supplement
  ButtonNavigator buttonNavigator;

  void handleSelection();
  void handleRenderModeSelection();
  void renderFontList(int pageWidth, int pageHeight);
  void renderModeScreen(int pageWidth, int pageHeight);
};
