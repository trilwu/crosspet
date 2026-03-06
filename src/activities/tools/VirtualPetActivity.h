#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

#include "PetActionMenu.h"
#include "PetAnimationIcons.h"
#include "PetStatsPanel.h"

// Main virtual pet interaction screen.
// Sprite + status icons + scrollable action menu + extended stat bars.
// Up/Down = navigate menu | Confirm = execute action | Back = exit
//
// Animation: idle breathing (Y-offset bob) + egg wobble (X-offset) + action feedback icons.
// Hatch flow: keyboard entry for name → type selection screen → hatchNew().
class VirtualPetActivity final : public Activity {
 public:
  explicit VirtualPetActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Virtual Pet", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return animActive(); }

 private:
  enum class ScreenMode { NORMAL, TYPE_SELECT };

  void renderNoPet() const;
  void renderDead() const;
  void renderAlive() const;
  void renderTypeSelect() const;

  void executeSelectedAction();

  // Hatch/customize helpers
  void startHatchFlow();
  void startRenameFlow();
  void startTypeSelectForHatch();
  void startTypeSelectForChange();
  void confirmTypeSelect();

  // Animation helpers
  void updateAnimation();
  void triggerActionIcon(PetAnimIcon icon);
  bool animActive() const;

  ButtonNavigator buttonNavigator;
  PetActionMenu actionMenu;
  PetStatsPanel statsPanel;

  ScreenMode screenMode = ScreenMode::NORMAL;
  char pendingName[20] = {};
  int typeSelectIndex = 0;
  bool hatchAfterTypeSelect = false;

  // Animation state
  uint8_t animFrame = 0;             // 0, 1, or 2 — cycles through 3 animation frames
  unsigned long lastAnimMs = 0;      // millis() of last frame toggle
  PetAnimIcon actionIcon = PetAnimIcon::NONE;
  unsigned long actionIconEndMs = 0; // millis() when icon should disappear

  static constexpr unsigned long ANIM_INTERVAL_MS = 4000;   // idle breathing period
  static constexpr unsigned long ACTION_ICON_DURATION_MS = 1500;
};
