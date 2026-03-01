---
status: done
created: 2026-03-01
completed: 2026-03-01
---

# Snake + 2048 Games

Add two games to the Tools menu: Snake (turn-based) and 2048 (tile merge).

## Phases

| # | Phase | Status | Files |
|---|-------|--------|-------|
| 1 | [Snake game](phase-01-snake-game.md) | done | `SnakeActivity.h/.cpp` |
| 2 | [2048 game + integration](phase-02-2048-game.md) | done | `TwentyFortyEightActivity.h/.cpp`, `ToolsActivity.h/.cpp`, `english.yaml`, `vietnamese.yaml` |

## Key Constraints
- ESP32-C3, 480×800 e-ink, ~380KB RAM
- All rendering: `renderer.fillRect / drawCenteredText / drawRect / displayBuffer`
- Input: `mappedInput.wasReleased(Button::*)` — no polling
- `esp_random()` for all RNG
- Follow existing Activity pattern exactly (see `GameOfLifeActivity`, `MazeGameActivity`)
- ToolsActivity: MENU_COUNT 8 → 10, add cases 8 and 9

## Dependencies
- Phase 2 blocked by Phase 1 (integration test needs both)
