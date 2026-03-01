---
status: in-progress
created: 2026-03-01
branch: master
updated: 2026-03-01
---

# CrossPet — Virtual Pet for E-Ink Reader

## Concept
Tamagotchi-style pixel art virtual pet that lives on the e-reader. Reading books feeds the pet and helps it evolve through 5 stages. Neglecting reading causes hunger → sickness → death. Pet appears in its own Activity and as a cameo on the sleep screen.

## Core Mechanics
- **Hunger:** Decreases over real time (~1 unit/hour). Fed by reading pages (20 pages = 1 meal).
- **Happiness:** Decreases slowly. Boosted by visiting pet, petting (Confirm button), reading streaks.
- **Health:** Derived stat. Decreases when hunger hits 0. Pet dies when health hits 0.
- **Evolution:** Egg(1d) → Hatchling(3d) → Youngster(7d) → Companion(14d) → Elder. Requires minimum reading + care thresholds to evolve. Can devolve one stage if severely neglected.
- **Death:** Permanent. Pet grave shown. Must hatch new egg to restart.

## Phases

| # | Phase | Status | Effort | Depends |
|---|-------|--------|--------|---------|
| 1 | [Pet Data Model & Persistence](phase-01-pet-data-model.md) | **Done** | 2h | — |
| 2 | [Sprite System](phase-02-sprite-system.md) | **Done** | 3h | — |
| 3 | [Pet Activity UI](phase-03-pet-activity-ui.md) | **Done** | 3h | 1, 2 |
| 4 | [Reading Integration](phase-04-reading-integration.md) | **Done** | 2h | 1 |
| 5 | [Sleep Screen Cameo](phase-05-sleep-screen-cameo.md) | **Done** | 1.5h | 1, 2 |
| 6 | [Sprite Assets & Polish](phase-06-sprite-assets.md) | **In Progress** | 2h | 2 |

Total: ~13.5h

## Architecture
```
PetState (JSON on SD)          ← persistence layer
  ├─ PetManager                ← game logic (stat decay, evolution, death)
  │   ├─ feedFromReading()     ← called by ReaderActivity on page turns
  │   ├─ tick()                ← time-based stat decay
  │   └─ interact()            ← user petting/playing
  ├─ PetSpriteRenderer         ← loads & draws sprites from SD
  │   ├─ drawPet()             ← render current state sprite
  │   └─ drawMini()            ← small sprite for sleep screen
  ├─ VirtualPetActivity        ← main interaction screen
  └─ SleepActivity integration ← cameo on sleep screen
```

## Key Constraints
- RAM: < 10KB total (sprite loaded one frame at a time from SD)
- Storage: sprites on SD at `.crosspoint/pet/sprites/`
- Pet state: JSON at `.crosspoint/pet/state.json`
- Time tracking: use RTC or millis delta from last save timestamp
- E-ink: minimize refreshes, use partial updates for animations

## Sprite Design
- 48×48 px, 1-bit (black/white), perfect for e-ink contrast
- 5 stages × 6 states (idle, happy, sad, eating, sleeping, dead) = 30 frames
- Mini sprite: 24×24 px for sleep screen cameo (5 stages × 2 states = 10 frames)
- Raw bitmap format: 48×48/8 = 288 bytes/frame, 30 frames = ~8.6KB on SD
- Only 1 frame loaded in RAM at a time = 288 bytes

## Risk Assessment
- **RTC accuracy:** ESP32-C3 has no battery-backed RTC. Time delta calculated from last save timestamp. If device is off for weeks, pet will die on next boot — this is intentional (stakes!).
- **Flash wear:** Pet state saves on every significant event (feeding, evolution, death). Not on every page turn — batch updates.
- **Sprite creation:** Need actual pixel art. Can use placeholder rectangles initially, replace with art later.
