# Phase 1: Pet Data Model & Persistence

## Overview
- **Priority:** Critical (foundation for all other phases)
- **Status:** Pending
- **Effort:** 2h
- **RAM:** ~500 bytes (PetState struct + PetManager logic)

## Description
Define the pet's data model, stat decay logic, evolution rules, and JSON persistence on SD card. This is the "brain" of the pet — all other phases depend on it.

## Data Model

```cpp
struct PetState {
  uint8_t stage;         // 0=egg, 1=hatchling, 2=youngster, 3=companion, 4=elder, 5=dead
  uint8_t hunger;        // 0-100 (0=starving, 100=full)
  uint8_t happiness;     // 0-100
  uint8_t health;        // 0-100 (derived: decreases when hunger=0)
  uint32_t birthTime;    // unix timestamp of egg creation
  uint32_t lastTickTime; // unix timestamp of last stat update
  uint32_t totalPagesRead; // lifetime pages fed to pet
  uint16_t currentStreak;  // consecutive days with reading
  uint8_t daysAtStage;     // days spent in current evolution stage
};
```

## Stat Decay Rules
- **Hunger:** -1 per hour (pet needs ~4 meals/day to stay above 50%)
- **Happiness:** -1 per 2 hours
- **Health:** stable when hunger > 0. When hunger = 0: -2 per hour
- **Death:** health reaches 0 → stage = DEAD (permanent)
- **Time delta:** on boot, calculate hours elapsed since lastTickTime, apply decay in bulk

## Evolution Thresholds
| Stage | Min Days | Min Total Pages | Min Avg Hunger |
|-------|----------|-----------------|----------------|
| Egg → Hatchling | 1 | 20 | — |
| Hatchling → Youngster | 3 | 100 | 40+ |
| Youngster → Companion | 7 | 500 | 50+ |
| Companion → Elder | 14 | 1500 | 60+ |

Evolution check runs once per day (on first boot of the day).

## Feeding
- 20 pages read = 1 meal = +25 hunger (capped at 100)
- Pages tracked via accumulator, not per-page callback (batch efficiency)
- Reading streak: if pages > 0 today, streak++. If 0 pages yesterday, streak resets.
- Streak bonus: +5 happiness per streak day (max +25 at 5-day streak)

## Related Code Files
- **Create:** `src/pet/PetState.h` — struct definition
- **Create:** `src/pet/PetManager.h` — game logic class header
- **Create:** `src/pet/PetManager.cpp` — stat decay, evolution, feeding, persistence
- **Reference:** `src/JsonSettingsIO.cpp` — JSON save/load pattern
- **Reference:** `src/CrossPointState.h` — state persistence pattern

## Implementation Steps

1. Create `PetState.h` with struct and constants (evolution thresholds, decay rates)
2. Create `PetManager.h/cpp`:
   - `load()` / `save()` — JSON to/from `.crosspoint/pet/state.json`
   - `tick()` — calculate time delta, apply decay, check death, check evolution
   - `feed(uint32_t pagesRead)` — convert pages to meals, update hunger
   - `pet()` — user interaction, +10 happiness (cooldown: once per 5 min)
   - `hatchNew()` — reset to egg state
   - `isAlive()`, `getStage()`, `getMood()` — state queries
   - `getMood()` returns: HAPPY (h>70,hp>70), NEUTRAL (h>30,hp>30), SAD (h>0,hp>0), SICK (h=0), DEAD
3. Singleton access via `PET_MANAGER` macro (follows SETTINGS pattern)
4. Initialize in `main.cpp setup()` after settings load

## Todo
- [ ] Create PetState.h
- [ ] Create PetManager.h/.cpp with all game logic
- [ ] Implement JSON persistence
- [ ] Implement time-delta stat decay
- [ ] Implement evolution check
- [ ] Implement feeding from page count
- [ ] Initialize in main.cpp
- [ ] Compile and verify

## Success Criteria
- Pet state persists across reboots
- Stat decay works correctly with time delta
- Evolution triggers at correct thresholds
- Death is permanent until new egg hatched
- RAM under 1KB
