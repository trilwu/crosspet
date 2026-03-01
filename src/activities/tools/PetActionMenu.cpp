#include "PetActionMenu.h"

#include "fontIds.h"

// ---- Navigation ---------------------------------------------------------

void PetActionMenu::moveUp() {
  selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : ACTION_TOTAL - 1;
}

void PetActionMenu::moveDown() {
  selectedIndex = (selectedIndex < ACTION_TOTAL - 1) ? selectedIndex + 1 : 0;
}

PetAction PetActionMenu::getSelected() const {
  return static_cast<PetAction>(selectedIndex);
}

// ---- Action availability guards -----------------------------------------

bool PetActionMenu::isActionAvailable(PetAction action, const PetState& state) const {
  if (!state.isAlive()) return false;

  switch (action) {
    case PetAction::FEED_MEAL:
    case PetAction::FEED_SNACK:
      return !state.isSleeping && !state.isSick;
    case PetAction::MEDICINE:
      return !state.isSleeping && state.isSick;
    case PetAction::EXERCISE:
      return !state.isSleeping && !state.isSick;
    case PetAction::CLEAN:
      return state.wasteCount > 0;
    case PetAction::SCOLD:
    case PetAction::IGNORE_CRY:
      return state.attentionCall;
    case PetAction::TOGGLE_LIGHTS:
      return true;
    case PetAction::PET_PET:
      return !state.isSleeping;
    case PetAction::RENAME:
    case PetAction::CHANGE_TYPE:
      return true;
    default:
      return false;
  }
}

// ---- Action labels ------------------------------------------------------

const char* PetActionMenu::actionLabel(PetAction action) {
  switch (action) {
    case PetAction::FEED_MEAL:     return "Feed Meal";
    case PetAction::FEED_SNACK:    return "Feed Snack";
    case PetAction::MEDICINE:      return "Medicine";
    case PetAction::EXERCISE:      return "Exercise";
    case PetAction::CLEAN:         return "Clean";
    case PetAction::SCOLD:         return "Scold";
    case PetAction::IGNORE_CRY:    return "Ignore";
    case PetAction::TOGGLE_LIGHTS: return "Lights";
    case PetAction::PET_PET:       return "Pet";
    case PetAction::RENAME:        return "Rename";
    case PetAction::CHANGE_TYPE:   return "Change Type";
    default:                       return "???";
  }
}

// ---- Rendering ----------------------------------------------------------

void PetActionMenu::render(GfxRenderer& renderer, const PetState& state,
                           int x, int y, int w, int h) const {
  const int lh = renderer.getLineHeight(SMALL_FONT_ID);
  const int rowH = lh + 6;
  const int visibleRows = h / rowH;

  // Scroll offset: keep selected item visible
  int scrollOffset = 0;
  if (selectedIndex >= visibleRows) scrollOffset = selectedIndex - visibleRows + 1;

  for (int i = 0; i < ACTION_TOTAL && (i - scrollOffset) < visibleRows; i++) {
    if (i < scrollOffset) continue;
    const int rowY = y + (i - scrollOffset) * rowH;
    const PetAction action = static_cast<PetAction>(i);
    const bool available = isActionAvailable(action, state);
    const bool selected = (i == selectedIndex);
    const char* label = actionLabel(action);

    if (selected) {
      // Highlight selected row with an inverted rect
      renderer.fillRect(x, rowY, w, rowH);
      renderer.drawText(SMALL_FONT_ID, x + 4, rowY + 3, label, /*invert=*/false);
    } else if (!available) {
      // Grayed out: show label with brackets to indicate unavailable
      char buf[32];
      snprintf(buf, sizeof(buf), "(%s)", label);
      renderer.drawText(SMALL_FONT_ID, x + 4, rowY + 3, buf);
    } else {
      renderer.drawText(SMALL_FONT_ID, x + 4, rowY + 3, label);
    }
  }
}
