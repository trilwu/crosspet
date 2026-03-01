#pragma once

#include "../Activity.h"

#include "PetActionMenu.h"
#include "PetStatsPanel.h"

// Main virtual pet interaction screen.
// Sprite + status icons + scrollable action menu + extended stat bars.
// Up/Down = navigate menu | Confirm = execute action | Back = exit
//
// Hatch flow: keyboard entry for name → type selection screen → hatchNew().
// RENAME action: keyboard entry → renamePet().
// CHANGE_TYPE action: type selection screen → changeType().
class VirtualPetActivity final : public Activity {
 public:
  explicit VirtualPetActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Virtual Pet", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Inline type selection mode (shown after keyboard entry or from action menu)
  enum class ScreenMode { NORMAL, TYPE_SELECT };

  void renderNoPet() const;
  void renderDead() const;
  void renderAlive() const;
  void renderTypeSelect() const;

  // Execute the currently selected action via PetManager
  void executeSelectedAction();

  // Hatch/customize helpers
  void startHatchFlow();           // opens keyboard, then type select
  void startRenameFlow();          // opens keyboard for rename only
  void startTypeSelectForHatch();  // shows type select, then hatches
  void startTypeSelectForChange(); // shows type select, then calls changeType()
  void confirmTypeSelect();        // called when user confirms a type

  PetActionMenu actionMenu;
  PetStatsPanel statsPanel;

  ScreenMode screenMode = ScreenMode::NORMAL;
  char pendingName[20] = {};  // name entered before hatching
  int typeSelectIndex = 0;
  bool hatchAfterTypeSelect = false;  // true if confirming type should hatch
};
