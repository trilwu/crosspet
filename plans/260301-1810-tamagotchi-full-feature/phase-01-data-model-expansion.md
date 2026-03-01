# Phase 1: Data Model Expansion

## Context Links
- Current: `src/pet/PetState.h` (47 lines, struct PetState + enums + PetConfig namespace)
- Current: `src/pet/PetManager.h` (67 lines)
- Persistence: `src/pet/PetManager.cpp` load()/save() use ArduinoJson

## Overview
- **Priority:** P1 (blocks all other phases)
- **Status:** pending
- **Description:** Add new fields to PetState, new enums, and new PetConfig constants for weight, sickness, bathroom, discipline, sleep cycle, care mistakes, and aging.

## Key Insights
- PetState is a flat POD struct serialized to JSON on SD card. Adding fields is backward-compatible via ArduinoJson `doc["field"] | default`.
- All stats are uint8_t (0-100). Keep new stats in same range for consistency.
- ESP32-C3 RAM: struct is tiny (~50 bytes now, will be ~70 bytes after). No concern.
- PetConfig is a constexpr namespace. New constants go here.

## Requirements

### New PetState Fields
```cpp
// Weight system (0-100, normal=50, overweight>80, underweight<20)
uint8_t weight = 50;

// Sickness
bool isSick = false;           // true when sick (from overfeeding or neglect)
uint8_t sicknessTimer = 0;     // hours remaining sick if untreated (auto-recovery after 24h)

// Bathroom
uint8_t wasteCount = 0;        // number of uncleared waste piles (0-4, max 4)
uint8_t mealsSinceClean = 0;   // meals eaten since last bathroom event

// Discipline (0-100, higher = better behaved)
uint8_t discipline = 50;
bool attentionCall = false;     // pet is calling for attention (may be real or fake need)
bool isFakeCall = false;        // true if current call is fake (discipline test)

// Sleep cycle
bool isSleeping = false;        // derived from clock, but cached for display
uint8_t lightsOff = 0;          // 1 = user turned lights off, 0 = lights on

// Aging & lifespan
uint16_t totalAge = 0;          // total days alive (redundant with birthTime but explicit for elder countdown)
uint8_t careMistakes = 0;       // lifetime care mistakes (hungry too long, sick too long, dirty too long)

// Evolution quality tracking
uint8_t avgCareScore = 50;      // rolling average care quality (0-100) for evolution branching
uint8_t evolutionVariant = 0;   // 0=good, 1=bad-chubby, 2=bad-misbehaved
```

### New PetMood Values
```cpp
enum class PetMood : uint8_t {
  HAPPY = 0,
  NEUTRAL = 1,
  SAD = 2,
  SICK = 3,
  SLEEPING = 4,
  DEAD = 5,
  // New:
  NEEDY = 6,     // calling for attention (icon blink)
  REFUSING = 7,  // misbehaving (won't eat/play)
};
```

### New PetConfig Constants
```cpp
// Weight
constexpr uint8_t WEIGHT_PER_MEAL = 5;       // meal adds weight
constexpr uint8_t WEIGHT_PER_SNACK = 3;      // snack adds weight
constexpr uint8_t WEIGHT_PER_EXERCISE = 8;   // exercise removes weight
constexpr uint8_t OVERWEIGHT_THRESHOLD = 80;
constexpr uint8_t UNDERWEIGHT_THRESHOLD = 20;
constexpr uint8_t NORMAL_WEIGHT = 50;

// Sickness
constexpr uint8_t SICK_RECOVERY_HOURS = 24;  // auto-recover if untreated
constexpr uint8_t SICK_HAPPINESS_PENALTY = 2; // per hour while sick

// Bathroom
constexpr uint8_t MEALS_UNTIL_WASTE = 3;     // every 3 meals = 1 waste
constexpr uint8_t MAX_WASTE = 4;
constexpr uint8_t WASTE_HAPPINESS_PENALTY = 1; // per waste pile per hour

// Discipline
constexpr uint8_t DISCIPLINE_PER_SCOLD = 10;
constexpr uint8_t DISCIPLINE_PER_IGNORE_FAKE = 5;  // ignoring fake call
constexpr uint8_t DISCIPLINE_PENALTY_INDULGE = 5;  // indulging fake call
constexpr uint8_t FAKE_CALL_CHANCE_PERCENT = 30;    // 30% chance call is fake

// Sleep cycle
constexpr uint8_t SLEEP_HOUR = 22;   // 10 PM
constexpr uint8_t WAKE_HOUR = 7;     // 7 AM

// Aging
constexpr uint16_t ELDER_LIFESPAN_DAYS = 20;    // max days in elder stage
constexpr uint8_t CARE_MISTAKE_LIFESPAN_PENALTY = 1; // each mistake reduces lifespan by 1 day
constexpr uint8_t CARE_MISTAKE_HUNGRY_HOURS = 6; // hungry >6h = 1 mistake
constexpr uint8_t CARE_MISTAKE_SICK_HOURS = 12;  // sick >12h untreated = 1 mistake
constexpr uint8_t CARE_MISTAKE_DIRTY_HOURS = 4;  // waste >4h uncleaned = 1 mistake
```

## Related Code Files
- **Modify:** `src/pet/PetState.h` (add fields, enums, config constants)
- **Modify:** `src/pet/PetManager.cpp` load()/save() (add new JSON fields with backward-compat defaults)

## Implementation Steps
1. Add new fields to `PetState` struct with sensible defaults
2. Add new PetMood enum values (NEEDY, REFUSING)
3. Add new PetConfig constants
4. Update `PetManager::load()` to deserialize new fields with `| default`
5. Update `PetManager::save()` to serialize new fields
6. Verify backward compat: old save files load without crash (missing fields get defaults)

## Todo List
- [ ] Add weight, sickness, bathroom, discipline, sleep, aging fields to PetState
- [ ] Add NEEDY, REFUSING to PetMood enum
- [ ] Add all new PetConfig constants
- [ ] Update load() with backward-compat deserialization
- [ ] Update save() with new field serialization
- [ ] Compile test

## Success Criteria
- Old pet save files load without error (backward compatible)
- New fields get sensible defaults on first load
- Struct size stays under 100 bytes
- Compiles cleanly

## Risk Assessment
- **JSON size growth:** ~200 bytes more JSON on SD. Trivial.
- **Backward compat:** ArduinoJson `doc["x"] | default` handles missing keys. Low risk.
