# Phase 6: UI Overhaul

## Context Links
- Current: `src/activities/tools/VirtualPetActivity.cpp` (215 lines -- at limit)
- Current: `src/activities/tools/VirtualPetActivity.h` (31 lines)
- Current: `src/activities/tools/ToolsActivity.cpp` (99 lines)
- 4 buttons: Back, Confirm, Up/Prev, Down/Next

## Overview
- **Priority:** P2
- **Status:** pending
- **Description:** Redesign VirtualPetActivity to support new actions menu, status icons, weight meter, care stats. Modularize rendering into sub-components.

## Key Insights
- VirtualPetActivity.cpp already at 215 lines. Must split rendering.
- Device has 4 buttons: Back (exit), Confirm (select), Up (prev), Down (next). Actions need a scrollable menu.
- E-ink: full redraws are slow. Use requestUpdate() sparingly. HALF_REFRESH for action feedback.
- Current UI: sprite + info line + 3 stat bars + missions + feedback. New UI: sprite + status icons + action menu + stats panel.

## Requirements

### Screen Layout (480x800 portrait)
```
[Header: "Virtual Pet"]
[Pet Sprite 96x96 centered]
[Status Icons Row: sleeping|sick|dirty|hungry|attention]
[Stage | Day X | Streak X]
[Stats: Hunger | Happy | Health | Weight | Discipline]
[Action Menu (scrollable): Feed Meal, Feed Snack, Medicine, Exercise, Clean, Scold, Ignore, Lights]
[Missions (if space)]
[Button Hints: Back | Select | Up | Down]
```

### Status Icons
- Row of small indicators below pet sprite
- Each icon: 16x16 pixel area with symbol
- Icons (drawn as text symbols for simplicity): ZZ (sleeping), + (sick), ** (dirty), ! (attention), heart (happy)
- Only shown when condition is active

### Action Menu
- Scrollable list of available actions
- Grayed out / hidden when not applicable (e.g., Medicine only when sick)
- Selected action highlighted with inverted rect (existing GUI.drawList pattern)
- Confirm button executes selected action
- Up/Down navigate menu

### Stats Panel
- Compact stat bars (reuse existing drawStatBar pattern)
- Add Weight bar and Discipline bar alongside existing Hunger/Happiness/Health
- Care Mistakes counter: small text "Mistakes: X"

### Modularization
Split into focused files:
```
src/activities/tools/
  VirtualPetActivity.h/cpp       -- lifecycle, loop, main render dispatch (~100 lines)
  PetStatsPanel.h/cpp            -- stat bars + status icons rendering (~100 lines)
  PetActionMenu.h/cpp            -- action menu logic + rendering (~120 lines)
```

## Related Code Files
- **Modify:** `src/activities/tools/VirtualPetActivity.h` -- slim down, add menu state
- **Modify:** `src/activities/tools/VirtualPetActivity.cpp` -- refactor, delegate to sub-renderers
- **Create:** `src/activities/tools/PetStatsPanel.h/cpp` -- stats + icons rendering
- **Create:** `src/activities/tools/PetActionMenu.h/cpp` -- action menu

## Implementation Steps

### Step 1: Create PetActionMenu
1. Define action enum:
   ```cpp
   enum class PetAction : uint8_t {
     FEED_MEAL, FEED_SNACK, MEDICINE, EXERCISE,
     CLEAN, SCOLD, IGNORE_CRY, TOGGLE_LIGHTS,
     PET_PET,  // existing petting action
     ACTION_COUNT
   };
   ```
2. `PetActionMenu` class:
   - `int selectedIndex = 0`
   - `void moveUp()`, `void moveDown()`
   - `PetAction getSelected() const`
   - `bool isActionAvailable(PetAction, const PetState&) const` -- guard checks
   - `void render(GfxRenderer&, const PetState&, int x, int y, int w, int h) const`
   - Render: list of action labels, highlight selected, gray out unavailable

### Step 2: Create PetStatsPanel
1. `void renderStats(GfxRenderer&, const PetState&, int x, int y, int w) const`
   - Draw 5 stat bars: Hunger, Happiness, Health, Weight, Discipline
   - Draw care mistakes counter
2. `void renderStatusIcons(GfxRenderer&, const PetState&, int x, int y, int w) const`
   - Draw icon row: sleeping, sick, dirty, attention indicators

### Step 3: Refactor VirtualPetActivity
1. Add `PetActionMenu actionMenu` member
2. Replace `renderAlive()` content:
   - Draw pet sprite (top)
   - Call `PetStatsPanel::renderStatusIcons()`
   - Call `PetStatsPanel::renderStats()`
   - Call `actionMenu.render()`
3. Update `loop()`:
   - Up/Down: navigate action menu (when pet alive)
   - Confirm: execute selected action via PetManager
   - Show feedback text from `PET_MANAGER.getLastFeedback()`
4. Keep renderNoPet() and renderDead() mostly unchanged

### Step 4: Update ToolsActivity
- No changes needed (VirtualPetActivity entry point unchanged)

## Todo List
- [ ] Define PetAction enum
- [ ] Create PetActionMenu.h/cpp with menu logic + rendering
- [ ] Create PetStatsPanel.h/cpp with stat bars + status icons
- [ ] Refactor VirtualPetActivity to use PetActionMenu and PetStatsPanel
- [ ] Update loop() for action menu navigation
- [ ] Implement action execution (Confirm -> PetManager method call)
- [ ] Add feedback text display after actions
- [ ] Test all action flows with guard checks
- [ ] Compile test

## Success Criteria
- VirtualPetActivity.cpp under 120 lines
- PetActionMenu.cpp under 130 lines
- PetStatsPanel.cpp under 110 lines
- All actions accessible via menu navigation
- Unavailable actions visually grayed or hidden
- Status icons correctly reflect pet state

## Risk Assessment
- **Screen space:** 800px height is generous. 5 stat bars + 8 action items + sprite fits if bars are compact (10px height each).
- **E-ink refresh:** Each action triggers requestUpdate() -> full render. Acceptable since user interactions are infrequent.
- **Button mapping:** Only 4 buttons. Up/Down for menu, Confirm for action, Back to exit. Clean mapping.
