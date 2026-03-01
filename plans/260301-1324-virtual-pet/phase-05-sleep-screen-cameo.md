# Phase 5: Sleep Screen Cameo

## Overview
- **Priority:** Medium (delight feature)
- **Status:** Pending
- **Effort:** 1.5h
- **Depends on:** Phase 1 (data model), Phase 2 (sprites)

## Description
Show a mini pet sprite on the sleep screen. Pet appears in corner of existing sleep screen modes (DARK, LIGHT, CLOCK, etc). Uses 24×24 mini sprites. Pet mood visible at a glance — motivates reading.

## Design
- Render 24×24 mini sprite in bottom-right corner of sleep screen
- Show alongside whatever sleep screen mode is active (non-intrusive)
- Pet mood determines which mini sprite: sleeping (if healthy), sad (if hungry), grave (if dead)
- On sleep screen, pet is always in "sleeping" pose if healthy — cute visual

## Sleep Screen States
| Pet State | Mini Sprite | Position |
|-----------|------------|----------|
| Healthy (hunger > 30) | sleeping | Bottom-right corner |
| Hungry (hunger ≤ 30) | sad | Bottom-right corner |
| Sick (health < 30) | sick | Bottom-right corner |
| Dead | grave | Bottom-right corner |
| No pet (never hatched) | — | Nothing shown |

## Related Code Files
- **Modify:** `src/activities/boot_sleep/SleepActivity.cpp` — add mini pet rendering
- **Reference:** `src/pet/PetSpriteRenderer.h` (Phase 2)
- **Reference:** `src/pet/PetManager.h` (Phase 1)

## Implementation Steps

1. In `SleepActivity::render()`: after existing sleep screen rendering, call `PetSpriteRenderer::drawMini()`
2. Position: `(screenWidth - 24 - margin, screenHeight - 24 - margin)`
3. Only render if pet exists (state file present) — skip if never hatched
4. Use sleeping sprite for healthy pets, sad for hungry, grave for dead
5. Minimal code addition — ~10 lines in SleepActivity

## Todo
- [ ] Add mini pet rendering call in SleepActivity::render()
- [ ] Determine sprite selection logic based on pet state
- [ ] Handle "no pet" case gracefully (skip rendering)
- [ ] Compile and verify all sleep screen modes still work

## Success Criteria
- Mini pet visible on all sleep screen modes
- Correct mood sprite shown
- No visual interference with existing sleep screen content
- Graceful when no pet exists
