# Phase 3: Pet Actions & Items System

## Context Links
- Data model: [phase-01](phase-01-data-model-expansion.md)
- Core logic: [phase-02](phase-02-core-gameplay-logic.md)
- Current: `src/pet/PetManager.h` -- existing `pet()`, `onPageTurn()`

## Overview
- **Priority:** P2
- **Status:** pending
- **Description:** Implement user-facing pet actions: feed meal, feed snack, give medicine, exercise/play, clean bathroom, discipline (scold), ignore cry, toggle lights.

## Key Insights
- Current system has only 2 user actions: `pet()` (petting) and implicit `onPageTurn()` (reading feeds).
- New actions triggered from UI (phase 6). PetManager exposes simple methods; UI calls them.
- Each action returns a feedback string for display (like current `petFeedback`).
- All actions save state after modification.
- Actions are guarded: can't feed when sick, can't exercise when sleeping, etc.

## Requirements

### Action: Feed Meal
- Fills hunger by HUNGER_PER_MEAL (25, existing)
- Adds WEIGHT_PER_MEAL (5) to weight
- Increments mealsSinceClean (triggers waste at threshold)
- Blocked if: sick, sleeping, dead
- Feedback: "Nom nom! +Hunger +Weight"

### Action: Feed Snack
- Adds HAPPINESS_PER_SNACK (15) to happiness
- Adds WEIGHT_PER_SNACK (3) to weight
- Does NOT fill hunger
- Blocked if: sick, sleeping, dead
- Feedback: "Yummy treat! +Happy +Weight"

### Action: Give Medicine
- Clears isSick, resets sicknessTimer
- Blocked if: not sick, sleeping, dead
- Feedback: "Medicine given! Feeling better~"

### Action: Exercise/Play
- Removes WEIGHT_PER_EXERCISE (8) from weight (clamped to 0)
- Adds 10 happiness
- Cooldown: 1 hour between exercises (reuse cooldown pattern from pet())
- Blocked if: sick, sleeping, dead
- Feedback: "Great workout! -Weight +Happy"

### Action: Clean Bathroom
- Resets wasteCount to 0
- Blocked if: wasteCount == 0
- Feedback: "All clean! (x piles removed)"

### Action: Discipline (Scold)
- If attentionCall && isFakeCall: discipline +DISCIPLINE_PER_SCOLD (good)
- If attentionCall && !isFakeCall: discipline -DISCIPLINE_PER_SCOLD (punished for real need)
- If no call active: discipline -3 (random scolding = bad)
- Clears attentionCall
- Feedback varies by outcome

### Action: Ignore Cry
- If attentionCall && isFakeCall: discipline +DISCIPLINE_PER_IGNORE_FAKE (good training)
- If attentionCall && !isFakeCall: careMistakes++, happiness -10 (neglect)
- Clears attentionCall
- Feedback varies

### Action: Toggle Lights
- Toggles lightsOff flag
- If sleep hours + lights off: pet sleeps peacefully
- If sleep hours + lights on: happiness -1/hour
- Feedback: "Lights off. Goodnight~" / "Lights on!"

## Related Code Files
- **Modify:** `src/pet/PetManager.h` -- add method declarations
- **Modify:** `src/pet/PetManager.cpp` -- implement new action methods (~80 lines of action handlers)

## Implementation Steps
1. Add `PetConfig::HAPPINESS_PER_SNACK = 15` to PetState.h
2. Add method signatures to PetManager.h:
   ```cpp
   bool feedMeal();
   bool feedSnack();
   bool giveMedicine();
   bool exercise();
   bool cleanBathroom();
   bool disciplinePet();
   bool ignoreCry();
   bool toggleLights();
   ```
3. Implement each in PetManager.cpp with guard checks, state mutations, save()
4. Each returns true if action succeeded (for UI feedback)
5. Add `const char* lastActionFeedback` member to PetManager for UI to read
6. Add `const char* getLastFeedback() const` accessor

## Todo List
- [ ] Add HAPPINESS_PER_SNACK constant
- [ ] Add 8 new method signatures to PetManager.h
- [ ] Implement feedMeal() with weight + waste tracking
- [ ] Implement feedSnack() with happiness + weight
- [ ] Implement giveMedicine() with sick check
- [ ] Implement exercise() with weight reduction + cooldown
- [ ] Implement cleanBathroom() with waste reset
- [ ] Implement disciplinePet() with fake/real call logic
- [ ] Implement ignoreCry() with fake/real call logic
- [ ] Implement toggleLights() with sleep flag
- [ ] Add lastActionFeedback + getter
- [ ] Compile test

## Success Criteria
- All 8 actions implemented with proper guards
- Actions correctly mutate PetState fields
- State saved after each action
- Feedback strings set for UI display
- Blocked actions return false without state change

## Risk Assessment
- **Action spam:** Cooldowns prevent rapid-fire exploitation (already have pattern from `pet()`)
- **State corruption:** Each action calls save(). ArduinoJson serialization is atomic to SD.
