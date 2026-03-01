# Phase 4: Attention & Notification System

## Context Links
- Data model: [phase-01](phase-01-data-model-expansion.md)
- Care tracker: [phase-02](phase-02-core-gameplay-logic.md) (PetCareTracker)
- Sleep screen: `src/activities/boot_sleep/SleepActivity.cpp`

## Overview
- **Priority:** P2
- **Status:** pending
- **Description:** Pet "calls" for attention when it needs something (or fakes it). Show indicator on sleep screen and in VirtualPetActivity.

## Key Insights
- E-ink can't blink/animate. "Attention" = static icon drawn on sleep screen refresh.
- Sleep screen is rendered once then device sleeps. So attention indicator is "baked in" at sleep time based on pet state.
- VirtualPetActivity can show attention icon during active use.
- No buzzer/LED on device (e-ink reader). Visual-only notification.

## Requirements

### Attention Call Generation
- During `tick()`, every ~4h of awake time, PetCareTracker rolls for attention call
- Call types (priority order): hungry (hunger<30), sick, dirty (waste>0), bored (happiness<30), fake (30% chance)
- Only 1 call active at a time. `attentionCall=true`, `isFakeCall=true/false`
- Call auto-expires after 2 hours if not addressed (counts as ignored)

### Need Detection (Non-Fake)
```cpp
enum class PetNeed : uint8_t {
  NONE = 0,
  HUNGRY = 1,
  SICK = 2,
  DIRTY = 3,
  BORED = 4,
  SLEEPY = 5,
};
```
- Add `PetNeed currentNeed` to PetState to track what the pet actually needs
- For fake calls, currentNeed = NONE but attentionCall = true

### Sleep Screen Indicator
- When pet has `attentionCall == true`, draw a small "!" icon near the pet mini-sprite
- Use existing speech bubble pattern from `renderClockSleepScreen()`:
  - Change message to need-specific: "Hungry!", "Sick!", "Dirty!", "Hey!", "Bored!"
- Also show on default sleep screen (renderDefaultSleepScreen)

### VirtualPetActivity Indicator
- Show flashing-style attention icon (alternating "[!]" text near pet sprite)
- Show need hint text below sprite: "Your pet needs attention!"
- Button hint changes: Confirm button shows relevant action (e.g., "Feed" if hungry)

## Related Code Files
- **Modify:** `src/pet/PetState.h` -- add PetNeed enum, currentNeed field
- **Modify:** `src/pet/PetCareTracker.cpp` -- generate attention calls
- **Modify:** `src/activities/boot_sleep/SleepActivity.cpp` -- show attention on sleep screens
- **Modify (phase 6):** `src/activities/tools/VirtualPetActivity.cpp` -- show attention in UI

## Implementation Steps
1. Add PetNeed enum to PetState.h
2. Add `currentNeed` and `lastCallTime` fields to PetState
3. In PetCareTracker: implement `generateAttentionCall()`:
   - Check if awake, no active call, enough time since last call
   - Scan needs in priority order; if none found, roll for fake call
   - Set attentionCall, isFakeCall, currentNeed
4. In PetCareTracker: implement `expireAttentionCall()`:
   - If call active > 2 hours: treat as ignored, clear call
5. Update SleepActivity::renderDefaultSleepScreen():
   - If `PET_MANAGER.getState().attentionCall`, draw "!" above mini pet sprite
   - Change speech bubble message to need-specific text
6. Update SleepActivity::renderClockSleepScreen():
   - Same attention indicator near pet sprite
7. Serialize/deserialize currentNeed, lastCallTime in load()/save()

## Todo List
- [ ] Add PetNeed enum to PetState.h
- [ ] Add currentNeed, lastCallTime fields
- [ ] Implement generateAttentionCall() in PetCareTracker
- [ ] Implement expireAttentionCall() in PetCareTracker
- [ ] Update renderDefaultSleepScreen() with attention indicator
- [ ] Update renderClockSleepScreen() with attention indicator
- [ ] Update load()/save() for new fields
- [ ] Compile test

## Success Criteria
- Pet generates attention calls every ~4h when awake
- Fake calls occur ~30% of the time
- Sleep screen shows "!" when pet needs attention
- Calls expire after 2h (counted as ignored = care mistake if real)

## Risk Assessment
- **Sleep screen complexity:** Adding to renderDefaultSleepScreen is simple (already has pet rendering). Low risk.
- **Over-notification:** 4h interval prevents spam. Capped at 1 active call.
