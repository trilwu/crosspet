# Phase 3: Game of Life Activity

## Overview
- **Priority:** Medium
- **Status:** Pending
- **Effort:** 2h
- **RAM:** ~5KB (two grid buffers for double-buffering)

## Description
Conway's Game of Life cellular automaton. Black/white grid evolves on e-ink display. User can pause/resume, step forward, randomize, and clear the grid. Mesmerizing on e-ink's high-contrast display.

## Key Insights
- Double-buffer technique: current + next grid, swap each generation
- Grid size: 480/4 × 800/4 = 120×200 cells at 4px/cell = 3000 bytes per buffer × 2 = 6KB
- Or 480/6 × 800/6 = 80×133 cells at 6px/cell = 1330 bytes per buffer × 2 = ~2.7KB
- E-ink full refresh each generation (entire screen changes)
- Auto-advance with configurable speed (200ms–2s between generations)
- Use bit-packing: 1 bit per cell → 80×133 = 1330 bytes per buffer

## Requirements
- Display grid of cells (alive=black, dead=white)
- Auto-evolve with configurable speed (Up/Down to adjust)
- Confirm = pause/resume
- Left = step one generation (when paused)
- Right = randomize grid
- Back = exit to Tools menu
- Show generation count and speed indicator
- Start with random seed pattern

## Rules (Conway's)
1. Live cell with <2 neighbors → dies (underpopulation)
2. Live cell with 2-3 neighbors → lives
3. Live cell with >3 neighbors → dies (overpopulation)
4. Dead cell with exactly 3 neighbors → becomes alive (reproduction)

## Related Code Files
- **Create:** `src/activities/tools/GameOfLifeActivity.h`
- **Create:** `src/activities/tools/GameOfLifeActivity.cpp`
- **Modify:** `src/activities/tools/ToolsActivity.h` (MENU_COUNT++)
- **Modify:** `src/activities/tools/ToolsActivity.cpp` (add menu entry + case)

## Implementation Steps

1. Create `GameOfLifeActivity.h` — members: two bit-packed grid buffers, generation count, speed, paused flag, grid dimensions
2. Implement grid operations:
   - `randomize()`: fill grid with ~30% alive density using `esp_random()`
   - `step()`: apply Conway's rules, write to next buffer, swap
   - `countNeighbors(x, y)`: count 8 surrounding cells (wrap edges for toroidal grid)
   - `getCell(x, y)` / `setCell(x, y, alive)`: bit-level access
3. Implement rendering:
   - Draw grid as filled rectangles (alive cells only, dead = background)
   - Show generation count and speed at bottom
   - Show "PAUSED" indicator when paused
4. Implement controls:
   - Auto-advance timer based on speed setting
   - Pause/resume toggle
   - Single-step when paused
   - Randomize resets generation count
5. Add to ToolsActivity menu

## Todo
- [ ] Create GameOfLifeActivity.h with bit-packed grid
- [ ] Implement Conway's rules with double-buffer
- [ ] Implement rendering (cells + HUD)
- [ ] Implement controls (pause, step, speed, randomize)
- [ ] Integrate into ToolsActivity menu
- [ ] Compile and verify

## Success Criteria
- Automaton evolves correctly per Conway's rules
- Smooth auto-advance at configurable speed
- Pause/step/randomize controls work
- RAM under 5KB additional
- Visually compelling on e-ink display
