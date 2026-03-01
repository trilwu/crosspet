# Phase 4: Tools Menu Integration

## Overview
- **Priority:** High (blocks shipping)
- **Status:** Pending
- **Effort:** 30min
- **Depends on:** Phases 1-3

## Description
Add all 3 new activities to the Tools menu. Update MENU_COUNT, add i18n strings, add switch cases.

## Related Code Files
- **Modify:** `src/activities/tools/ToolsActivity.h` — MENU_COUNT from 4 → 7
- **Modify:** `src/activities/tools/ToolsActivity.cpp` — includes, menu labels, switch cases
- **Modify:** `lib/I18n/translations/english.yaml` — add STR_PHOTO_FRAME, STR_MAZE_GAME, STR_GAME_OF_LIFE
- **Modify:** `lib/I18n/translations/vietnamese.yaml` — Vietnamese translations

## Implementation Steps
1. Add i18n string IDs in translation YAML files
2. Update `ToolsActivity.h`: `MENU_COUNT = 7`
3. Update `ToolsActivity.cpp`:
   - Add `#include` for all 3 new activities
   - Add menu labels array entries
   - Add switch cases 4, 5, 6 pushing new activities
4. Compile and verify full build

## Todo
- [ ] Add i18n strings
- [ ] Update ToolsActivity.h MENU_COUNT
- [ ] Update ToolsActivity.cpp menu entries
- [ ] Compile full build
