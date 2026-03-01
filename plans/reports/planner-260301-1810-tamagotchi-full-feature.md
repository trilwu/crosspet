# Planner Report: Full Tamagotchi Feature

**Plan:** `/Users/trilm/DEV/crosspoint-reader/plans/260301-1810-tamagotchi-full-feature/plan.md`
**Date:** 2026-03-01

## Summary

Created 7-phase implementation plan to extend the existing virtual pet system into a full Tamagotchi experience. Total estimated effort: 16h.

## What Exists (read, not re-planned)
- PetState.h: stage enum (EGG-DEAD), hunger/happiness/health stats, missions
- PetManager.cpp: tick/decay/evolution/feeding/petting (335 lines)
- PetSpriteRenderer: 12x12 fallback sprites + SD card .bin loading
- VirtualPetActivity: sprite + stat bars + missions UI (215 lines)
- SleepActivity: mini pet sprite + speech bubbles on sleep screen

## What's New (planned)

| Phase | Scope | New Files | Effort |
|-------|-------|-----------|--------|
| 1. Data Model | 12 new PetState fields, 2 new PetMood values, ~20 config constants | None | 2h |
| 2. Core Logic | Sleep/wake, sickness, weight, bathroom, discipline, aging, care mistakes | PetDecayEngine.h/cpp, PetCareTracker.h/cpp | 4h |
| 3. Actions | 8 new user actions (feed meal/snack, medicine, exercise, clean, scold, ignore, lights) | None (methods in PetManager) | 2h |
| 4. Attention | Attention call generation, need detection, sleep screen indicators | None (logic in PetCareTracker, UI in SleepActivity) | 2h |
| 5. Evolution | Care score tracking, variant selection (good/chubby/misbehaved), sprite variants | PetEvolution.h/cpp | 2h |
| 6. UI | Action menu, status icons panel, stats panel, modularized rendering | PetActionMenu.h/cpp, PetStatsPanel.h/cpp | 3h |
| 7. i18n | ~30 new translation keys, Vietnamese translations, sleep screen speech bubbles | None | 1h |

## Key Architectural Decisions
- **Modularize PetManager** (335 lines) into PetDecayEngine + PetCareTracker + PetEvolution. PetManager stays as facade.
- **Modularize VirtualPetActivity** (215 lines) into PetActionMenu + PetStatsPanel. Activity stays as coordinator.
- All new PetState fields use backward-compatible ArduinoJson defaults.
- Action menu uses existing ButtonNavigator + GUI.drawList patterns (4-button navigation).
- No heap allocation for gameplay. All state in flat POD struct (~70 bytes).

## File Change Summary

### New Files (6)
- `src/pet/PetDecayEngine.h/cpp` -- hourly decay + sleep/sick/weight/waste logic
- `src/pet/PetCareTracker.h/cpp` -- care mistakes, discipline, attention calls
- `src/pet/PetEvolution.h/cpp` -- evolution checks, variant branching, care score
- `src/activities/tools/PetActionMenu.h/cpp` -- scrollable action menu
- `src/activities/tools/PetStatsPanel.h/cpp` -- stat bars + status icons

### Modified Files (7)
- `src/pet/PetState.h` -- new fields, enums, config constants
- `src/pet/PetManager.h` -- new method signatures
- `src/pet/PetManager.cpp` -- delegate to new modules, add action methods
- `src/pet/PetSpriteRenderer.h/cpp` -- variant parameter for drawPet/drawMini
- `src/activities/tools/VirtualPetActivity.h/cpp` -- refactored UI
- `src/activities/boot_sleep/SleepActivity.cpp` -- attention indicators
- `lib/I18n/translations/english.yaml` + `vietnamese.yaml` -- new strings

## Unresolved Questions
1. **i18n code generation:** Does the project auto-generate string IDs from YAML, or are they manually registered in a header? Need to check `fontIds.h` or similar.
2. **Sprite assets:** Who creates the .bin sprite files for variant evolutions? Plan assumes fallback pixel-art is sufficient initially.
3. **Sleep screen file size:** SleepActivity.cpp is 513 lines. Flagged but not split because each render function is self-contained. Confirm this is acceptable or if it should be refactored.
