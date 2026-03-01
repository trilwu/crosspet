# Phase 5: Evolution Branching

## Context Links
- Current: `src/pet/PetManager.cpp` `checkEvolution()` (13 lines)
- Current: `src/pet/PetSpriteRenderer.cpp` -- sprite lookup by stage+mood
- Data model: [phase-01](phase-01-data-model-expansion.md) (avgCareScore, evolutionVariant)

## Overview
- **Priority:** P2
- **Status:** pending
- **Description:** Add evolution quality branching. Good care leads to "good" variant; poor care leads to "bad" variants (chubby from overweight, misbehaved from low discipline).

## Key Insights
- Current evolution is linear: EGG->HATCHLING->YOUNGSTER->COMPANION->ELDER
- Branching applies at YOUNGSTER and COMPANION stages (where appearance diverges)
- `evolutionVariant` stored in PetState determines sprite selection
- Sprite renderer already constructs paths like `{stage}_{mood}.bin`. Add variant prefix: `{stage}_{variant}_{mood}.bin` with fallback to `{stage}_{mood}.bin` for variant 0 (good/default).
- Pixel-art fallback sprites (12x12 grids) need minor variants for bad evolutions

## Requirements

### Evolution Variant Logic
At evolution check (HATCHLING->YOUNGSTER and YOUNGSTER->COMPANION):
```
if avgCareScore >= 70 AND weight in [30,70] AND discipline >= 50:
    variant = 0  (good)
elif weight > OVERWEIGHT_THRESHOLD:
    variant = 1  (chubby)
elif discipline < 30:
    variant = 2  (misbehaved)
else:
    variant = 0  (default to good)
```

### Care Score Calculation
- `avgCareScore` updated daily in tick():
  ```
  dailyScore = (hunger + happiness + health) / 3
  avgCareScore = (avgCareScore * 6 + dailyScore * 4) / 10  // weighted rolling average
  ```

### Sprite Variant Support
- PetSpriteRenderer::drawPet() accepts optional variant parameter
- Path construction: `{stage}_{variant}_{mood}.bin` (variant > 0) or `{stage}_{mood}.bin` (variant 0)
- Fallback 12x12 sprites: add 2 variant sprites for YOUNGSTER and COMPANION stages
  - Chubby variant: wider body silhouette
  - Misbehaved variant: different ear/eye pattern (angry look)

### Stage Names for Variants
- Variant 0: keep existing names (Youngster, Companion)
- Variant 1: "Pudgy Youngster", "Chonky Companion"
- Variant 2: "Rowdy Youngster", "Wild Companion"

## Related Code Files
- **Create:** `src/pet/PetEvolution.h` -- evolution check + variant logic
- **Create:** `src/pet/PetEvolution.cpp`
- **Modify:** `src/pet/PetManager.cpp` -- delegate checkEvolution() to PetEvolution
- **Modify:** `src/pet/PetSpriteRenderer.cpp` -- add variant to path construction
- **Modify:** `src/pet/PetSpriteRenderer.h` -- add variant param to drawPet/drawMini

## Implementation Steps
1. Create PetEvolution.h/cpp:
   - `static void checkEvolution(PetState& state)` -- existing logic + variant selection
   - `static void updateCareScore(PetState& state)` -- daily rolling average
   - `static uint8_t determineVariant(const PetState& state)` -- variant selection logic
   - `static const char* variantStageName(PetStage stage, uint8_t variant)` -- display names
2. Move existing `checkEvolution()` logic from PetManager to PetEvolution
3. Add variant determination at evolution transition points
4. Update PetSpriteRenderer:
   - `drawPet()` and `drawMini()` accept `uint8_t variant = 0`
   - Path: if variant > 0, try `{stage}_v{variant}_{mood}.bin` first, fallback to default
5. Add 2 fallback pixel-art variants for YOUNGSTER and COMPANION (12x12 bitmaps)
6. Update PetManager to call `PetEvolution::updateCareScore()` daily

## Todo List
- [ ] Create PetEvolution.h/cpp
- [ ] Move checkEvolution() logic to PetEvolution
- [ ] Implement variant determination at evolution points
- [ ] Implement updateCareScore() daily rolling average
- [ ] Add variant parameter to PetSpriteRenderer drawPet/drawMini
- [ ] Update sprite path construction with variant
- [ ] Add chubby + misbehaved fallback 12x12 sprites
- [ ] Add variant stage name strings
- [ ] Update PetManager to delegate evolution + care score
- [ ] Compile test

## Success Criteria
- Good care (score>70, normal weight, discipline>50) produces variant 0
- Overweight pets get chubby variant
- Undisciplined pets get misbehaved variant
- Sprite renderer loads variant sprites when available, falls back to default
- Existing sprite files continue to work (variant 0 = no change in path)

## Risk Assessment
- **Sprite file proliferation:** 2 extra variants * 2 stages * 6 moods = 24 extra .bin files. Optional -- fallback sprites handle missing files.
- **avgCareScore accuracy:** Rolling average smooths short-term neglect. Acceptable for gameplay.
