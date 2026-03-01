---
title: "Full Tamagotchi Feature Implementation"
description: "Extend virtual pet with sleep cycle, sickness, weight, bathroom, discipline, aging, and care tracking"
status: completed
priority: P2
effort: 16h
branch: master
tags: [pet, tamagotchi, gameplay, e-ink]
created: 2026-03-01
completed: 2026-03-01
---

# Full Tamagotchi Feature Plan

## Summary
Extend existing virtual pet (PetState/PetManager/VirtualPetActivity) with classic Tamagotchi mechanics: sleep/wake cycle, sickness/medicine, weight, bathroom, discipline, aging/lifespan, care-mistake tracking, evolution branching, and attention alerts.

## Phases

| # | Phase | Status | Progress | File |
|---|-------|--------|----------|------|
| 1 | Data model expansion (PetState.h, PetConfig) | ✅ COMPLETED | 100% | [phase-01](phase-01-data-model-expansion.md) |
| 2 | Core gameplay logic (PetManager overhaul) | ✅ COMPLETED | 100% | [phase-02](phase-02-core-gameplay-logic.md) |
| 3 | Pet actions & items system | ✅ COMPLETED | 100% | [phase-03](phase-03-pet-actions-items.md) |
| 4 | Attention & notification system | ✅ COMPLETED | 100% | [phase-04](phase-04-attention-notifications.md) |
| 5 | Evolution branching | ✅ COMPLETED | 100% | [phase-05](phase-05-evolution-branching.md) |
| 6 | UI overhaul (VirtualPetActivity + status icons) | ✅ COMPLETED | 100% | [phase-06](phase-06-ui-overhaul.md) |
| 7 | Sleep screen integration & i18n | ✅ COMPLETED | 100% | [phase-07](phase-07-sleep-screen-i18n.md) |

## Key Dependencies
- Phase 1 (data model) blocks all other phases
- Phase 2 (logic) blocks phases 3-7
- Phases 3-5 can run in parallel after phase 2
- Phase 6 depends on phases 3, 4, 5
- Phase 7 depends on phase 6

## Constraints
- ESP32-C3: ~320KB RAM, no heap bloat
- E-ink: minimize redraws, use HALF_REFRESH
- Files under 200 lines; modularize PetManager into focused modules
- No network required for pet gameplay
- 4 physical buttons: Back, Confirm, Up/Prev, Down/Next
