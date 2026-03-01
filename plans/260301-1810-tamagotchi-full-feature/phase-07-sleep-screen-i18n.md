# Phase 7: Sleep Screen Integration & i18n

## Context Links
- Sleep screen: `src/activities/boot_sleep/SleepActivity.cpp` (513 lines -- already large)
- i18n: `lib/I18n/translations/english.yaml` (380+ keys)
- Phase 4: [phase-04](phase-04-attention-notifications.md) (attention on sleep screen)

## Overview
- **Priority:** P3
- **Status:** pending
- **Description:** Polish sleep screen pet integration, add i18n strings for all new UI text, update Vietnamese translation file.

## Key Insights
- SleepActivity.cpp is 513 lines. Already large but it's mostly render functions for different sleep modes. Not worth splitting further since each function is self-contained.
- Sleep screen pet rendering already exists (mini sprite + speech bubble in clock mode). Phase 4 adds attention indicator.
- i18n uses YAML files with STR_ prefix keys. All user-facing text must have translation keys.

## Requirements

### Sleep Screen Updates
- All sleep screen modes that show pet (default, clock): show attention indicator from phase 4
- Add need-specific speech bubbles:
  - Hungry: "Feed me~", "So hungry..."
  - Sick: "Not feeling well...", "Need medicine..."
  - Dirty: "It's dirty...", "Clean up please~"
  - Attention (fake): "Hey!", "Look at me!"
  - Happy: "Zzz...", "Purr~" (existing)
  - Sad: "Lonely...", "Read more!" (existing)

### New i18n String Keys
```yaml
# Pet actions
STR_PET_FEED_MEAL: "Feed Meal"
STR_PET_FEED_SNACK: "Feed Snack"
STR_PET_MEDICINE: "Medicine"
STR_PET_EXERCISE: "Exercise"
STR_PET_CLEAN: "Clean"
STR_PET_SCOLD: "Scold"
STR_PET_IGNORE: "Ignore"
STR_PET_LIGHTS: "Lights"

# Pet stats
STR_PET_WEIGHT: "Weight"
STR_PET_DISCIPLINE: "Discipline"
STR_PET_CARE_MISTAKES: "Mistakes"

# Pet status
STR_PET_SLEEPING: "Sleeping"
STR_PET_SICK: "Sick"
STR_PET_DIRTY: "Dirty"
STR_PET_ATTENTION: "Needs attention!"

# Pet stage variants
STR_PET_STAGE_PUDGY_YOUNGSTER: "Pudgy Youngster"
STR_PET_STAGE_ROWDY_YOUNGSTER: "Rowdy Youngster"
STR_PET_STAGE_CHONKY_COMPANION: "Chonky Companion"
STR_PET_STAGE_WILD_COMPANION: "Wild Companion"

# Pet feedback
STR_PET_FED_MEAL: "Nom nom! +Hunger"
STR_PET_FED_SNACK: "Yummy! +Happy"
STR_PET_GAVE_MEDICINE: "Feeling better~"
STR_PET_EXERCISED: "Great workout!"
STR_PET_CLEANED: "All clean!"
STR_PET_SCOLDED_GOOD: "Good discipline!"
STR_PET_SCOLDED_BAD: "That was mean..."
STR_PET_IGNORED_GOOD: "Good training!"
STR_PET_IGNORED_BAD: "Feeling neglected..."
STR_PET_LIGHTS_OFF: "Goodnight~"
STR_PET_LIGHTS_ON: "Lights on!"
STR_PET_GOOD_DEATH: "Your pet lived a full life. Farewell~"
STR_PET_NEGLECT_DEATH: "Your pet has passed from neglect..."

# Action blocked
STR_PET_BLOCKED_SICK: "Too sick to eat..."
STR_PET_BLOCKED_SLEEPING: "Shh... sleeping"
STR_PET_BLOCKED_COOLDOWN: "Not ready yet..."
```

## Related Code Files
- **Modify:** `lib/I18n/translations/english.yaml` -- add ~30 new string keys
- **Modify:** `lib/I18n/translations/vietnamese.yaml` -- add Vietnamese translations
- **Modify:** `src/activities/boot_sleep/SleepActivity.cpp` -- update speech bubble messages
- **Modify:** `src/activities/tools/PetActionMenu.cpp` -- use i18n keys for action labels
- **Modify:** `src/activities/tools/PetStatsPanel.cpp` -- use i18n keys for stat labels

## Implementation Steps
1. Add all new STR_ keys to english.yaml
2. Add Vietnamese translations to vietnamese.yaml
3. Run i18n code generation (if the project has one) or add to `fontIds.h` / string ID header
4. Update SleepActivity speech bubble logic:
   - Check pet's currentNeed for need-specific messages
   - Rotate messages by hour (existing pattern)
5. Replace hardcoded strings in PetActionMenu and PetStatsPanel with `tr(STR_...)` calls
6. Update feedback strings in PetManager action methods to use i18n keys

## Todo List
- [ ] Add ~30 new STR_ keys to english.yaml
- [ ] Add Vietnamese translations
- [ ] Update sleep screen speech bubbles with need-specific messages
- [ ] Replace all hardcoded pet strings with tr() calls
- [ ] Verify i18n code gen / string ID registration
- [ ] Compile test

## Success Criteria
- All user-visible pet text uses i18n system
- English and Vietnamese translations complete
- Sleep screen shows contextual pet messages based on need state
- No hardcoded English strings in pet-related code

## Risk Assessment
- **Translation quality:** Vietnamese translations may need native review. Mark as draft.
- **String ID conflicts:** Verify no duplicate STR_ keys in YAML. Low risk with unique naming.
