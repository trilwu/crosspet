# Phase 1: Photo Frame Activity

## Overview
- **Priority:** High
- **Status:** Pending
- **Effort:** 3h
- **RAM:** ~10KB (BMP decoded line-by-line, no full image in RAM)

## Description
BMP slideshow from SD card. User places `.bmp` files in a `/photos/` folder on SD. Activity scans folder, displays images fullscreen with auto-cycle timer. Buttons: next/prev image, back to exit.

## Key Insights
- `GfxRenderer::drawBitmap()` already exists — handles Bitmap rendering with crop/fit
- Existing cover image rendering in EPUB uses similar BMP loading patterns
- E-ink full refresh per image is acceptable (photo changes are infrequent)
- 4-bit grayscale BMP (16 shades) matches e-ink capability perfectly
- Line-by-line BMP decoding avoids loading full image into RAM

## Requirements
- Scan `/photos/` directory on SD for `.bmp` files
- Display images fullscreen, centered, with fit-to-screen scaling
- Auto-cycle every 60s (configurable via Up/Down buttons: 30s, 60s, 2min, 5min)
- Manual next/prev with Left/Right buttons
- Show filename + index overlay briefly on image change
- Back button exits to Tools menu
- Handle empty folder gracefully (show "No photos found" message)

## Related Code Files
- **Create:** `src/activities/tools/PhotoFrameActivity.h`
- **Create:** `src/activities/tools/PhotoFrameActivity.cpp`
- **Modify:** `src/activities/tools/ToolsActivity.h` (MENU_COUNT++)
- **Modify:** `src/activities/tools/ToolsActivity.cpp` (add menu entry + case)
- **Reference:** `lib/GfxRenderer/GfxRenderer.h` (drawBitmap API)
- **Reference:** `lib/Epub/Epub/blocks/ImageBlock.cpp` (BMP loading pattern)

## Implementation Steps

1. Create `PhotoFrameActivity.h` — inherit Activity, members: file list, current index, timer interval, last advance time
2. Create `PhotoFrameActivity.cpp`:
   - `onEnter()`: scan `/photos/*.bmp` using HalStorage directory listing, populate filename vector
   - `loop()`: handle button input (Left=prev, Right=next, Up/Down=change interval, Back=exit), check auto-advance timer
   - `render()`: load current BMP from SD, draw fullscreen via `renderer.drawBitmap()`, show brief overlay text
3. Handle edge cases: empty folder, corrupt BMPs, files too large
4. Add to ToolsActivity menu

## Todo
- [ ] Create PhotoFrameActivity.h
- [ ] Create PhotoFrameActivity.cpp with scan, display, auto-cycle
- [ ] Handle empty folder / corrupt file gracefully
- [ ] Integrate into ToolsActivity menu
- [ ] Compile and verify

## Success Criteria
- Photos display fullscreen, properly scaled
- Auto-cycle works at configurable intervals
- Manual navigation works
- Empty folder shows message
- RAM stays under 10KB additional
