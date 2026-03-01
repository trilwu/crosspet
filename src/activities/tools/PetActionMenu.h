#pragma once

#include <GfxRenderer.h>

#include "pet/PetState.h"

// Available user-facing pet actions shown in the action menu
enum class PetAction : uint8_t {
  FEED_MEAL = 0,
  FEED_SNACK,
  MEDICINE,
  EXERCISE,
  CLEAN,
  SCOLD,
  IGNORE_CRY,
  TOGGLE_LIGHTS,
  PET_PET,
  RENAME,
  CHANGE_TYPE,
  ACTION_COUNT
};

// Scrollable action menu for VirtualPetActivity.
// Tracks the selected index; renders a list of actions with availability guards.
class PetActionMenu {
 public:
  void moveUp();
  void moveDown();

  PetAction getSelected() const;

  // Returns true if the action is usable given current pet state
  bool isActionAvailable(PetAction action, const PetState& state) const;

  // Render the action list at (x, y) within (w x h) pixels
  void render(GfxRenderer& renderer, const PetState& state, int x, int y, int w, int h) const;

  // Label string for a given action
  static const char* actionLabel(PetAction action);

 private:
  int selectedIndex = 0;

  static constexpr int ACTION_TOTAL = static_cast<int>(PetAction::ACTION_COUNT);
};
