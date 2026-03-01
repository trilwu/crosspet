# Phase 4: Reading Integration

## Overview
- **Priority:** High (core mechanic — reading feeds pet)
- **Status:** Pending
- **Effort:** 2h
- **Depends on:** Phase 1 (data model)

## Description
Hook into the reader to track pages read and feed the pet. Every 20 pages = 1 meal. Batched updates to avoid SD write on every page turn.

## Integration Point
The reader already tracks page turns. Need to find where page navigation happens and add a lightweight counter.

## Design
- **Page counter:** increment on each page turn in ReaderActivity
- **Batch feed:** every 20 pages, call `PetManager::feed(20)`
- **Save frequency:** feed triggers a save (max ~5 saves per 100 pages)
- **Streak tracking:** on first page turn of the day, record "read today" flag
- **Daily check:** on boot, if yesterday had no reading, reset streak

## Related Code Files
- **Modify:** `src/activities/reader/ReaderActivity.cpp` — add page counter hook
- **Reference:** `src/pet/PetManager.h` (Phase 1)
- **Reference:** `src/CrossPointState.h` — daily state tracking pattern

## Implementation Steps

1. Find page turn handler in ReaderActivity (look for page navigation / chapter advance)
2. Add `PetManager::onPageTurn()` call — lightweight, just increments counter
3. In PetManager: accumulate pages, every 20 → feed() + save
4. On `PetManager::tick()` (boot): check if new day, update streak
5. Keep reading integration minimal — single function call, no UI changes in reader

## Key Constraint
- MUST NOT slow down page turns. `onPageTurn()` must be O(1) with no SD I/O.
- SD write only on feed threshold (every 20 pages) — acceptable ~50ms pause.

## Todo
- [ ] Locate page turn handler in ReaderActivity
- [ ] Add PetManager::onPageTurn() call
- [ ] Implement page accumulator with batch feeding
- [ ] Implement streak tracking (daily read flag)
- [ ] Compile and verify no page-turn latency impact

## Success Criteria
- Pages read correctly counted and fed to pet
- No perceptible delay on page turns
- Reading streak tracks correctly across days
- SD writes batched (not per-page)
