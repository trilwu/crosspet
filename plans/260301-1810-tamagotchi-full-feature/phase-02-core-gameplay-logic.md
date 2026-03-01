# Phase 2: Core Gameplay Logic

## Context Links
- Current: `src/pet/PetManager.cpp` (335 lines -- needs modularization)
- Current: `src/pet/PetManager.h` (67 lines)
- Phase 1 data model: [phase-01](phase-01-data-model-expansion.md)

## Overview
- **Priority:** P1 (blocks phases 3-7)
- **Status:** pending
- **Description:** Implement sleep/wake cycle, sickness mechanics, weight system, bathroom/waste, discipline, aging/lifespan, care-mistake tracking. Modularize PetManager.

## Key Insights
- PetManager.cpp is already 335 lines. Adding all new logic will push it well over 200. Must split.
- `applyDecay()` loops hour-by-hour. New mechanics plug into this loop.
- `tick()` is called on boot + periodically. All time-based logic lives here.
- Sleep cycle uses system clock (already available via `getLocalTime`).

## Architecture: Modularization Plan

Split PetManager.cpp into focused modules:

```
src/pet/
  PetState.h              -- data model (phase 1)
  PetManager.h            -- public API (slim)
  PetManager.cpp          -- tick(), load(), save(), hatchNew() (~150 lines)
  PetDecayEngine.h/cpp    -- applyDecay(), sleep/wake, hourly stat changes (~120 lines)
  PetCareTracker.h/cpp    -- care mistakes, discipline, attention calls (~100 lines)
  PetEvolution.h/cpp      -- evolution checks, branching logic (~80 lines)
  PetSpriteRenderer.h/cpp -- unchanged
```

PetManager delegates to PetDecayEngine and PetCareTracker. They operate on PetState& (no ownership).

## Requirements

### Sleep/Wake Cycle
- Pet sleeps between SLEEP_HOUR (22) and WAKE_HOUR (7)
- During sleep: no hunger/happiness decay, no attention calls
- `isSleeping` flag updated each tick based on current hour
- If user interacts during sleep (without turning lights off), pet wakes upset: -5 happiness, +1 care mistake
- "Lights off" action: user confirms pet is sleeping. If lights left on during sleep hours, happiness decays faster

### Sickness Mechanics
- Triggers: (a) overfeeding (weight > OVERWEIGHT_THRESHOLD), (b) waste left uncleaned > 4h
- While sick: happiness decays 2x faster, pet refuses food, can't play
- Cure: medicine action (instant cure)
- Auto-recovery: after SICK_RECOVERY_HOURS (24h) if untreated
- Untreated > CARE_MISTAKE_SICK_HOURS = +1 care mistake

### Weight System
- Meals add WEIGHT_PER_MEAL (5), snacks add WEIGHT_PER_SNACK (3)
- Exercise removes WEIGHT_PER_EXERCISE (8)
- Natural decay: weight tends toward NORMAL_WEIGHT (50) at 1 point per 12 hours
- Overweight (>80): triggers sickness chance
- Underweight (<20): health decays slowly

### Bathroom/Waste
- Every MEALS_UNTIL_WASTE (3) meals, wasteCount++ (max MAX_WASTE=4)
- Each waste pile: -1 happiness per hour
- Waste uncleaned > CARE_MISTAKE_DIRTY_HOURS (4h) = +1 care mistake
- "Clean" action resets wasteCount to 0

### Discipline
- Random "attention calls" every ~4 hours when awake
- 30% chance call is fake (pet doesn't actually need anything)
- If fake call + user feeds/pets: discipline -5 (indulged bad behavior)
- If fake call + user ignores: discipline +5 (good training)
- If real call + user responds: discipline +2
- If real call + user ignores: care mistake +1, happiness -10
- High discipline (>70): fewer fake calls, better evolution
- Low discipline (<30): more fake calls, worse evolution

### Aging & Lifespan
- Elder stage: lifespan = ELDER_LIFESPAN_DAYS - (careMistakes * CARE_MISTAKE_LIFESPAN_PENALTY)
- When elder totalAge exceeds lifespan: "good death" (farewell screen)
- If health reaches 0 at any stage: "neglect death" (sad screen)
- totalAge incremented in tick() daily

### Care Mistake Tracking
- Tracked in `careMistakes` (uint8_t, capped at 255)
- Sources: hungry > 6h, sick > 12h untreated, dirty > 4h, disturbed sleep, ignored real call
- Affects: elder lifespan, evolution quality

## Related Code Files
- **Modify:** `src/pet/PetManager.h` (add method declarations, forward-declare new classes)
- **Modify:** `src/pet/PetManager.cpp` (slim down, delegate to new modules)
- **Create:** `src/pet/PetDecayEngine.h` (free functions or static class for decay logic)
- **Create:** `src/pet/PetDecayEngine.cpp`
- **Create:** `src/pet/PetCareTracker.h` (care mistakes, discipline, attention)
- **Create:** `src/pet/PetCareTracker.cpp`

## Implementation Steps

### Step 1: Create PetDecayEngine
1. Extract `applyDecay()` from PetManager into `PetDecayEngine::applyDecay(PetState&, uint32_t hours)`
2. Add sleep check: skip decay during sleep hours
3. Add sickness effects: double happiness decay when sick
4. Add waste happiness penalty
5. Add weight natural tendency toward normal
6. Add underweight health drain
7. Add waste generation after meals (via counter)

### Step 2: Create PetCareTracker
1. `checkCareMistakes(PetState&, uint32_t elapsedHours)` -- increment careMistakes for overdue needs
2. `generateAttentionCall(PetState&)` -- randomly trigger attention calls during awake hours
3. `resolveAttentionCall(PetState&, bool userResponded)` -- handle discipline changes
4. `updateCareScore(PetState&)` -- rolling average of care quality

### Step 3: Update PetManager
1. `tick()`: call `PetDecayEngine::applyDecay()`, `PetCareTracker::checkCareMistakes()`, update isSleeping, update totalAge
2. `hatchNew()`: reset all new fields
3. Keep `load()`/`save()` in PetManager (already handles JSON)
4. Add public methods: `giveMedicine()`, `feedSnack()`, `exercise()`, `cleanBathroom()`, `toggleLights()`, `discipline()` (scold), `ignoreCry()`

### Step 4: Update getMood()
```cpp
PetMood PetManager::getMood() const {
  if (!state.isAlive()) return PetMood::DEAD;
  if (state.isSleeping) return PetMood::SLEEPING;
  if (state.isSick) return PetMood::SICK;
  if (state.attentionCall) return PetMood::NEEDY;
  if (state.discipline < 30 && random(100) < 20) return PetMood::REFUSING;
  if (state.hunger > 70 && state.health > 70) return PetMood::HAPPY;
  if (state.hunger > 30 && state.health > 30) return PetMood::NEUTRAL;
  return PetMood::SAD;
}
```

## Todo List
- [ ] Create PetDecayEngine.h/cpp with extracted + new decay logic
- [ ] Create PetCareTracker.h/cpp with care mistake + discipline logic
- [ ] Add sleep/wake detection in tick()
- [ ] Add sickness trigger + auto-recovery in decay
- [ ] Add weight natural tendency in decay
- [ ] Add waste generation in feeding path
- [ ] Add care-mistake checking per tick
- [ ] Add attention call generation
- [ ] Update getMood() with new states
- [ ] Add new public methods to PetManager (medicine, snack, exercise, clean, lights, scold, ignore)
- [ ] Update hatchNew() to reset all new fields
- [ ] Compile test

## Success Criteria
- PetManager.cpp stays under 200 lines
- PetDecayEngine.cpp under 150 lines
- PetCareTracker.cpp under 120 lines
- All new mechanics tick correctly with time simulation
- Backward compatible: old saves work (new fields get defaults)

## Risk Assessment
- **Complexity creep in applyDecay:** hour-by-hour loop with many checks. Keep each check as a separate inline helper for readability.
- **Random number generation:** ESP32 has `esp_random()`. Use `random()` (Arduino wrapper). Already used in codebase.
- **Clock dependency:** Sleep cycle needs valid time. Already guarded by `isTimeValid()`. If no clock, skip sleep logic.
