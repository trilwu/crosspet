# Phase 3: Pet Activity UI

## Overview
- **Priority:** High
- **Status:** Pending
- **Effort:** 3h
- **Depends on:** Phase 1 (data model), Phase 2 (sprites)

## Description
Main interaction screen for the virtual pet. Shows pet sprite centered, stat bars, stage name, and action buttons. User can pet (Confirm), play (Left/Right cycle actions), view stats.

## Screen Layout (480×800 portrait)
```
┌──────────────────────────┐
│       CrossPet           │  ← header
├──────────────────────────┤
│   Stage: Hatchling       │  ← stage + name
│   Day 5 · Streak: 3🔥   │  ← age + reading streak
│                          │
│                          │
│      ┌──────────┐        │
│      │  48×48   │        │  ← pet sprite (scaled 3x = 144×144)
│      │  sprite  │        │
│      └──────────┘        │
│                          │
│   ♥ Hunger  ████████░░   │  ← stat bars
│   ☺ Happy   ██████░░░░   │
│   ✚ Health  ██████████   │
│                          │
│  "I'm feeling great!"    │  ← mood text bubble
│                          │
├──────────────────────────┤
│ [Back] [Pet] [◄ ►]      │  ← button hints
└──────────────────────────┘
```

## Interactions
| Button | Action |
|--------|--------|
| Confirm | Pet the creature (+10 happiness, 5min cooldown). Animation: happy sprite briefly. |
| Left/Right | Cycle info pages: Stats → History → About |
| Back | Exit to Tools menu (auto-saves state) |

## Info Pages
1. **Stats** (default): sprite + stat bars + mood text
2. **History**: total pages fed, days alive, evolution log
3. **About**: pet species name, birth date, stage progress bar

## Dead State
When pet is dead: show grave sprite, "Your pet has passed..." message, Confirm = "Hatch New Egg?"

## Related Code Files
- **Create:** `src/activities/tools/VirtualPetActivity.h`
- **Create:** `src/activities/tools/VirtualPetActivity.cpp`
- **Modify:** `src/activities/tools/ToolsActivity.h` (MENU_COUNT++)
- **Modify:** `src/activities/tools/ToolsActivity.cpp` (add menu entry)
- **Reference:** `src/pet/PetManager.h` (Phase 1)
- **Reference:** `src/pet/PetSpriteRenderer.h` (Phase 2)

## Implementation Steps

1. Create `VirtualPetActivity.h` — members: current page index, last pet time, animation state
2. Create `VirtualPetActivity.cpp`:
   - `onEnter()`: call `PetManager::tick()` to update stats, request render
   - `loop()`: handle buttons (pet, page cycle, back), check pet cooldown
   - `render()`: draw header, pet sprite (scaled), stat bars, mood text, button hints
   - `renderStats()`: stat bars using `renderer.fillRect()` for bar segments
   - `renderHistory()`: text-based stats page
   - `renderDead()`: grave sprite + restart prompt
3. Stat bars: filled rect (green portion) + empty rect (gray portion), proportional to stat value
4. Mood text: map mood enum to i18n string (happy/neutral/sad/sick)
5. Scale sprite 3× for display (48→144px) using nearest-neighbor in render code
6. Add to ToolsActivity menu

## Todo
- [ ] Create VirtualPetActivity.h
- [ ] Create VirtualPetActivity.cpp with all render states
- [ ] Implement stat bar rendering
- [ ] Implement page cycling (Stats/History/About)
- [ ] Implement petting interaction with cooldown
- [ ] Implement dead state + restart flow
- [ ] Add i18n strings for pet UI
- [ ] Integrate into ToolsActivity menu
- [ ] Compile and verify

## Success Criteria
- Pet sprite renders centered and scaled
- Stat bars reflect actual pet state
- Petting interaction works with cooldown
- Dead state shows restart option
- Page cycling works smoothly
- All text is i18n-ready
