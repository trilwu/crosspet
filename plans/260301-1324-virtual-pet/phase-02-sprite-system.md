# Phase 2: Sprite System

## Overview
- **Priority:** Critical (needed by Activity UI + sleep screen)
- **Status:** Pending
- **Effort:** 3h
- **RAM:** ~300 bytes (one 48×48 frame in memory at a time)

## Description
Load and render 1-bit pixel art sprites from SD card. Supports two sizes: 48×48 (main view) and 24×24 (sleep screen mini). Only one frame loaded at a time to minimize RAM.

## Sprite Storage Format
- Raw 1-bit bitmap: 1 bit per pixel, MSB first, row-major
- 48×48 = 288 bytes/frame, 24×24 = 72 bytes/frame
- Files on SD: `.crosspoint/pet/sprites/{stage}_{mood}.bin`
- Example: `hatchling_happy.bin`, `elder_sad.bin`, `egg_idle.bin`
- Mini sprites: `.crosspoint/pet/sprites/mini/{stage}_{mood}.bin`

## Sprite States per Stage
| Stage | Sprites Needed |
|-------|---------------|
| Egg | idle, hatching |
| Hatchling | idle, happy, sad, eating, sleeping |
| Youngster | idle, happy, sad, eating, sleeping |
| Companion | idle, happy, sad, eating, sleeping |
| Elder | idle, happy, sad, eating, sleeping |
| Dead | grave |

Total: 2 + 5×4 + 1 = 23 full sprites + 11 mini sprites

## Related Code Files
- **Create:** `src/pet/PetSpriteRenderer.h`
- **Create:** `src/pet/PetSpriteRenderer.cpp`
- **Reference:** `lib/GfxRenderer/GfxRenderer.h` — `drawImage()` API
- **Reference:** `lib/hal/HalStorage.h` — SD file reading

## Implementation Steps

1. Create `PetSpriteRenderer.h/cpp`:
   - `drawPet(renderer, x, y, stage, mood)` — load + render 48×48 sprite
   - `drawMini(renderer, x, y, stage, mood)` — load + render 24×24 sprite
   - Internal: `loadSprite(path, buffer, size)` — read from SD into temp buffer
   - Internal: `renderBitmap(renderer, buffer, x, y, w, h)` — draw 1-bit bitmap using `drawImage()`
2. Build sprite filename from stage + mood enum values
3. Fallback: if sprite file missing, draw placeholder rectangle with stage initial
4. Sprite buffer: single static `uint8_t[288]` (reused per draw call)

## Placeholder Sprites
For initial development before real pixel art:
- Draw filled rectangle with letter (E/H/Y/C/A/D) for each stage
- Replace with real sprites in Phase 6

## Todo
- [ ] Create PetSpriteRenderer.h/.cpp
- [ ] Implement sprite loading from SD
- [ ] Implement 1-bit bitmap rendering via GfxRenderer
- [ ] Implement fallback placeholder for missing sprites
- [ ] Create directory structure on SD if missing
- [ ] Compile and verify

## Success Criteria
- Sprites load from SD and render correctly
- Fallback works when sprite files missing
- Only 288 bytes RAM used (single frame buffer)
- Clean API for both Activity and sleep screen integration
