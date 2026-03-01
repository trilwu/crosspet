# Phase 2: Maze Game Activity

## Overview
- **Priority:** Medium
- **Status:** Pending
- **Effort:** 2h
- **RAM:** ~3KB (maze grid stored as bit array)

## Description
Random maze generation using recursive backtracker algorithm. Player navigates from top-left to bottom-right using d-pad. Maze rendered as pixel grid on e-ink. New maze generated each game.

## Key Insights
- Recursive backtracker is simple, produces good mazes, and uses minimal RAM
- Maze grid: each cell needs 4 wall bits → 4 bits/cell → 20x30 grid = 300 bytes
- E-ink partial refresh for player movement (only redraw player position area)
- 480x800 display fits ~20x33 cells at 24px/cell with margins
- Single e-ink refresh on maze generation, partial updates for movement

## Requirements
- Generate random maze using recursive backtracker (iterative stack version to avoid stack overflow)
- Display maze fullscreen with walls as black lines, paths as white
- Player marker (filled circle) at start position (top-left)
- Goal marker (hollow circle or flag) at end (bottom-right)
- D-pad moves player through open passages
- Confirm button = generate new maze
- Back button = exit to Tools menu
- Show move count on screen
- Victory message when reaching goal, then auto-generate new maze

## Architecture
```
MazeGameActivity
├── MazeGenerator (iterative backtracker)
│   ├── generate(width, height, seed)
│   └── hasWall(x, y, direction) → bool
├── Player position (x, y)
├── Move count
└── Render (walls + player + goal)
```

## Related Code Files
- **Create:** `src/activities/tools/MazeGameActivity.h`
- **Create:** `src/activities/tools/MazeGameActivity.cpp`
- **Modify:** `src/activities/tools/ToolsActivity.h` (MENU_COUNT++)
- **Modify:** `src/activities/tools/ToolsActivity.cpp` (add menu entry + case)
- **Reference:** `lib/GfxRenderer/GfxRenderer.h` (drawRect, fillRect, drawCircle)

## Implementation Steps

1. Create `MazeGameActivity.h` — members: maze wall data (bitfield array), player pos, goal pos, move count, maze dimensions
2. Implement maze generation:
   - Iterative backtracker using explicit stack (avoid recursion on ESP32 limited stack)
   - Use `esp_random()` for seeding
   - Store walls as compact bit array: 4 bits per cell (N, S, E, W)
3. Implement rendering:
   - Calculate cell size from screen dimensions and maze size
   - Draw walls as black rectangles
   - Draw player as filled circle
   - Draw goal as concentric circles
   - Draw move count at bottom
4. Implement movement:
   - D-pad checks wall presence before moving
   - On move: request partial update (player area only)
   - On reaching goal: show victory, delay, regenerate
5. Add to ToolsActivity menu

## Todo
- [ ] Create MazeGameActivity.h with maze data structures
- [ ] Implement iterative backtracker maze generation
- [ ] Implement maze rendering (walls, player, goal)
- [ ] Implement player movement with wall collision
- [ ] Victory detection and new maze generation
- [ ] Integrate into ToolsActivity menu
- [ ] Compile and verify

## Success Criteria
- Random maze generated each game
- Player navigates correctly (can't walk through walls)
- Victory detected when reaching goal
- RAM under 5KB additional
- Responsive movement (partial e-ink refresh)
