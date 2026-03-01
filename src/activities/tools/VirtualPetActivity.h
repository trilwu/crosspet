#pragma once

#include "../Activity.h"

#include "PetActionMenu.h"
#include "PetStatsPanel.h"

// Main virtual pet interaction screen.
// Sprite + status icons + scrollable action menu + extended stat bars.
// Up/Down = navigate menu | Confirm = execute action | Back = exit
class VirtualPetActivity final : public Activity {
 public:
  explicit VirtualPetActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Virtual Pet", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void renderNoPet() const;
  void renderDead() const;
  void renderAlive() const;

  // Execute the currently selected action via PetManager
  void executeSelectedAction();

  PetActionMenu actionMenu;
  PetStatsPanel statsPanel;
};
