---
status: done
created: 2026-03-01
branch: master
updated: 2026-03-01
---

# E-Ink Fun Features — Photo Frame, Maze Game, Game of Life

## Overview
Add 3 new Activities to the Tools menu: Photo Frame (BMP slideshow), Maze Game (random generation + navigation), and Conway's Game of Life (cellular automaton).

All follow existing Activity pattern. Total estimated RAM: ~15KB combined. Implementation effort: ~6-8 hours total.

## Phases

| # | Phase | Status | Effort | Files |
|---|-------|--------|--------|-------|
| 1 | [Photo Frame Activity](phase-01-photo-frame.md) | **Done** | 3h | 2 new + 2 modified |
| 2 | [Maze Game Activity](phase-02-maze-game.md) | **Done** | 2h | 2 new + 2 modified |
| 3 | [Game of Life Activity](phase-03-game-of-life.md) | **Done** | 2h | 2 new + 2 modified |
| 4 | [Tools Menu Integration](phase-04-tools-menu-integration.md) | **Done** | 30m | 3 modified |

## Dependencies
- Phases 1-3 are independent (can be implemented in parallel)
- Phase 4 depends on all previous phases

## Architecture
All activities extend `Activity` base class, live in `src/activities/tools/`. Registered in `ToolsActivity` menu. Use existing `GfxRenderer`, `ButtonNavigator`, `MappedInputManager`.

## Key Constraints
- E-ink: minimize full refreshes, use partial updates where possible
- RAM: each activity must stay under 10KB heap allocation
- Display: 480x800 portrait (or 800x480 landscape depending on orientation setting)
- Input: 6 buttons (Back, Confirm, Left, Right, Up, Down)
