# Project Changelog

**Format:** Semantic Versioning (MAJOR.MINOR.PATCH)
**Last Updated:** March 4, 2026

---

## [1.4.0] - Unreleased (In Development)

### Major Features

#### New Games & Tools
- **Chess:** Full chess game with move validation and AI (ChessActivity)
- **Caro/Gomoku:** Tile-matching strategy game with AI opponent (CaroActivity)
- **Weather Display:** Real-time weather information (WeatherActivity)
- **News Reader:** RSS feed reader for current events (NewsReaderActivity)
- **Daily Quote:** Rotating literary quotes with daily tracking (DailyQuoteActivity)

#### UI Enhancements
- **Home Screen Clock:** Display HH:MM in header when `statusBarClock` enabled
- **ToolsActivity Expansion:** Now 11 items (Clock, Pomodoro, VirtualPet, 2048, Sudoku, Minesweeper, Presenter, Caro, Chess, Weather, News)
- **Daily Quotes on Sleep Screen:** 28 rotating literary quotes on CLOCK sleep screen (bottom aligned, left-shifted for pet sprite)

#### Reading Integration Features
- **Reading Stats Sleep Screen:** New sleep mode (READING_STATS=7) showing today's reading time, all-time total, last book title + progress bar
- **Clock Refresh on Brief Wake:** Brief power button press re-renders dynamic sleep screens (CLOCK, READING_STATS) before re-entering sleep
- **ReadingStats Persistence:** Binary persistence at `/.crosspoint/reading_stats.bin` with auto-load on boot

### Removed Activities
- **PhotoFrameActivity** - Removed due to feature consolidation
- **GameOfLifeActivity** - Removed due to maintenance burden
- **MazeGameActivity** - Removed due to space constraints
- **SnakeActivity** - Removed due to redundancy with other games

### Code Changes

**New Files:**
```
src/activities/tools/
├── ChessActivity.h/cpp
├── CaroActivity.h/cpp
├── WeatherActivity.h/cpp
├── NewsReaderActivity.h/cpp
└── DailyQuoteActivity.h/cpp

src/
├── ReadingStats.h/cpp (singleton, binary persistence)
└── ReadingStatsManager.cpp (if split)
```

**Modified Files:**
- `src/activities/home/HomeActivity.cpp` - Added renderHeaderClock() method
- `src/activities/tools/ToolsActivity.cpp/h` - Expanded menu to 11 items
- `src/activities/boot_sleep/SleepActivity.cpp` - Added daily quotes rendering, reading stats screen
- `src/main.cpp` - Added ReadingStats load on boot, verifyPowerButtonDuration() for clock refresh
- `src/CrossPointSettings.h` - Added `statusBarClock` boolean setting

**Deleted Files:**
- `src/activities/tools/PhotoFrameActivity.h/cpp`
- `src/activities/tools/GameOfLifeActivity.h/cpp`
- `src/activities/tools/MazeGameActivity.h/cpp`
- `src/activities/tools/SnakeActivity.h/cpp`

### Testing

- Manual testing of all 5 new activities on hardware
- ReadingStats persistence across power cycles verified
- Clock refresh on brief wake tested
- Daily quotes rotation validated
- Home screen clock display alignment checked

### Documentation

- Updated `docs/code-standards.md` with new activity patterns
- Updated `docs/system-architecture.md` with ReadingStats subsystem
- Updated `docs/codebase-summary.md` with complete tools list
- Updated `docs/project-overview-pdr.md` with v1.3 release status

### Known Issues

- Weather data source not yet implemented (placeholder)
- News feeds require WiFi connectivity
- ReadingStats binary format not yet documented (see file-formats.md TODO)

---

## [1.3.0] - 2026-03-02

### Major Features

#### Pet-Reading Integration System
- **Reading streaks drive gameplay:** 0/7/14/30-day thresholds unlock meal cost reductions (20→16→13→10 pages per meal)
- **Book completion rewards:** +40 happiness, +20 hunger, increments `booksFinished` counter
- **Daily reading goal bonus:** 20+ pages/day grants +10 health, +5 happiness (once per calendar day)
- **Pomodoro focus completion:** +15 happiness per session
- **Enhanced evolution system:**
  - Three evolution variants now rewritten: 0=Scholar (reading-focused), 1=Balanced, 2=Wild
  - COMPANION→ELDER evolution gates on streak >= 7 AND booksFinished >= 1
  - New stage names: "Scholarly Young", "Wild Youngster", "Scholar", "Wild Companion"
  - Variants determined by reading metrics (pages read, completion count)

#### Game Balance
- **Streak tiers reduce feeding cost:** Encourages daily reading habit
- **Goal-based rewards:** Multi-path progression (reading, petting, caring)
- **Evolution gating:** Ensures meaningful book engagement before elder transformation

### Code Changes

**Modified Files:**
- `src/pet/PetState.h` - Added `streakDays`, `booksFinished`, `lastGoalBonusDate` fields
- `src/pet/PetManager.h/cpp` - Streak tracking, goal checking, evolution prerequisite logic
- `src/pet/PetPersistence.cpp` - Persistence for new fields
- `src/pet/PetEvolution.cpp` - Variant calculation based on reading metrics
- `src/activities/EpubReaderActivity.h/cpp` - Book completion integration, goal bonus check, session tracking
- `src/activities/tools/PomodoroActivity.cpp` - Happiness grant on completion

**New Files:** None

**Deleted Files:** None

### Bug Fixes

- Fixed meal cost calculation to use streak-based page formula
- Ensured COMPANION→ELDER requires reading engagement (books completed)

### Testing

- Manual gameplay testing: streak progression, meal cost reduction verification
- Goal bonus timing tested across device calendar boundaries
- Evolution variant assignment validated against reading metrics
- Pomodoro completion reward integration verified

### Documentation

- Updated `docs/code-standards.md` with streak and goal bonus mechanics
- Updated `docs/system-architecture.md` with reading-pet feedback loop
- Added integration notes to `docs/codebase-summary.md`

### Breaking Changes

None. All new fields have safe defaults; existing pet states gracefully migrate.

### Known Issues

None at time of release.

---

## [1.2.0] - 2026-03-01

### Major Features

#### Virtual Pet - Full Tamagotchi Implementation
- **New subsystem:** `src/pet/` with 10+ files
- **Complete gameplay loop:** EGG → HATCHLING → YOUNGSTER → COMPANION → ELDER evolution
- **Advanced mechanics:**
  - Weight system (overweight/underweight tracking)
  - Sickness mechanic (overfeeding/neglect → illness → medicine cure)
  - Waste/bathroom system (every 3 meals, requires cleaning)
  - Discipline system (rewards ignoring fake calls)
  - Attention calls every ~4 hours (30% fake for discipline testing)
  - Daily missions (read 20 pages, pet 3x)
  - Care mistake tracking (hungry >6h, sick >12h, dirty >4h)

#### Evolution Variants
- **3-branch evolution system:** Good / Chubby / Misbehaved
- Variants determined at YOUNGSTER & COMPANION transitions
- Care quality metrics tracked per stage (avgCareScore)
- Variant-aware sprite rendering with pixel-art fallbacks

#### User Interactions
- **8 pet actions:** Feed meal, feed snack, medicine, exercise, clean bathroom, scold, ignore cry, toggle lights
- **Action menu:** Scrollable list with availability guards (cooldowns, locked actions)
- **Stats panel:** 5 stat bars (hunger, happiness, health, weight, discipline) + status icons
- **Mood system:** 8 visual moods (HAPPY, NEUTRAL, SAD, SICK, SLEEPING, DEAD, NEEDY, REFUSING)
- **Speech bubbles:** Need-specific messages (Feed me~, Need medicine..., It's dirty..., Hey!)

#### Persistence
- **33-field JSON state:** Complete pet serialization to `/.crosspoint/pet/state.json`
- **Hourly decay engine:** Automatic stat decay with sleep cycle skip (10 PM - 7 AM)
- **Reliable save/load:** Async persistence with corruption recovery

#### UI Components
- **PetActionMenu:** Scrollable action list with availability checks
- **PetStatsPanel:** Visual stat bars, status icons, mistake counter
- **VirtualPetActivity:** Refactored main interaction screen
- **SleepActivity:** Enhanced with attention indicator (!) and need-specific speech bubbles

#### Internationalization
- **37 new STR_PET_* keys** in all 18 languages
- **419 total keys** (updated from 382)
- **Languages:** English, Spanish, French, German, Italian, Portuguese, Russian, Ukrainian, Polish, Swedish, Norwegian, Chinese, Japanese, Korean, Vietnamese, Turkish, Hebrew, Arabic

### Code Changes

**New Files (10):**
```
src/pet/
├── PetState.h              (Data model: 33 fields, enums, constants)
├── PetManager.h/cpp        (Core lifecycle controller)
├── PetActions.cpp          (8 user action methods)
├── PetPersistence.cpp      (JSON save/load)
├── PetDecayEngine.h/cpp    (Hourly stat decay logic)
├── PetCareTracker.h/cpp    (Attention calls, care mistakes)
├── PetEvolution.h/cpp      (Evolution variants)
└── PetSpriteRenderer.h/cpp (Mood-based rendering)

src/activities/tools/
├── PetActionMenu.h/cpp     (Scrollable action menu)
└── PetStatsPanel.h/cpp     (Stats visualization)
```

**Modified Files:**
- `src/activities/tools/VirtualPetActivity.h/cpp` - Complete refactor with new UI
- `src/activities/tools/SleepActivity.cpp` - Added attention indicator and speech bubbles
- `src/SettingsList.h` - Added pet-related settings
- `src/I18nKeys.h` - 37 new pet keys (419 total)
- `src/I18nStrings.h/cpp` - Regenerated with pet strings
- `src/i18n/english.yaml` - +37 STR_PET_* entries
- `src/i18n/vietnamese.yaml` - +37 STR_PET_* entries (+ all other langs)

**Deleted Files:** None

### Game Balance Constants

**Saved in PetConfig (src/pet/PetState.h):**
```cpp
HUNGER_DECAY_PER_HOUR = 1
HAPPINESS_DECAY_PER_HOUR = 1
HEALTH_DECAY_PER_HOUR = 2 (only when starving)
WEIGHT_PER_MEAL = 5
WEIGHT_PER_EXERCISE = 8
FAKE_CALL_CHANCE_PERCENT = 30
ATTENTION_CALL_INTERVAL_SEC = 14400 (~4 hours)
ATTENTION_CALL_EXPIRE_SEC = 7200 (2 hours)
SICK_RECOVERY_HOURS = 24
CARE_MISTAKE_HUNGRY_HOURS = 6
CARE_MISTAKE_SICK_HOURS = 12
CARE_MISTAKE_DIRTY_HOURS = 4

Evolution thresholds (stage → days, pages, avg hunger):
  EGG → HATCHLING: 1 day, 20 pages, 0 avg
  HATCHLING → YOUNGSTER: 3 days, 100 pages, 40 avg
  YOUNGSTER → COMPANION: 7 days, 500 pages, 50 avg
  COMPANION → ELDER: 14 days, 1500 pages, 60 avg
```

### Bug Fixes

- **Virtual Pet Egg Creation:** Fixed failure when no RTC clock set (use hardcoded default)
- **Clock Sleep Screen:** Removed invertScreen bug causing white background
- **Display Refresh:** Improved partial update handling for pet sprites

### Testing

- Manual testing on physical Xteink X4 device
- EPUB parsing with edge cases
- Pet state persistence across power cycles
- Evolution threshold validation
- All 18 languages verified for completeness

### Documentation

- Created `docs/codebase-summary.md` (complete codebase overview)
- Created `docs/system-architecture.md` (data flows, component interactions)
- Created `docs/code-standards.md` (C++ style, patterns, game balance)
- Created `docs/project-overview-pdr.md` (PDR with functional requirements)
- Created `docs/project-changelog.md` (this file)

### Commits

```
36606a2 feat: virtual pet improvements and modern clock sleep screen
b865e58 feat: cute pet sprites, daily missions, clock sleep screen calendar
b845ad0 feat: add monthly calendar grid to Clock activity
b011569 fix: virtual pet egg creation fails when no clock is set
8023eaa feat: add manual time setting to Clock activity
```

### Performance Impact

- **Memory:** +200 bytes for PetState struct (total ~300KB peak)
- **Display:** No impact (pet already rendered in v1.1)
- **Storage:** +200 bytes per pet state file (negligible)
- **Startup:** No impact (pet loaded async)

### Breaking Changes

None. All changes backward-compatible with v1.1 saves.

### Known Issues

- None at time of release

### Migration Guide

No migration needed. Existing settings and reading progress preserved.

---

## [1.1.0] - 2025-11-15

### Features

#### Tools Menu System
- **New activity:** `ToolsActivity` launcher
- **Clock activity:** Time display with manual setting and NTP sync
- **Pomodoro timer:** 25/5 minute work/break cycles
- **Games:**
  - Snake: Classic game with grid-based movement
  - 2048: Tile-merging puzzle
  - Maze game: Procedural maze generation
  - Game of Life: Conway's cellular automaton
- **Photo frame:** Slideshow from `/photos` SD directory
- **Daily quote:** Inspirational quotes with date tracking
- **Conference badge:** Name & event display
- **Virtual Pet:** Initial implementation (basic stats only)

#### Sleep Screen
- Time and date display
- Auto-activation at 10 PM (configurable)
- Auto-deactivation at 7 AM
- Monthly calendar grid
- Pet sleep indicator

#### Bluetooth
- HID remote control support
- Button remapping per activity
- Consumer Control commands (play, pause, volume)

#### UX Improvements
- Button hint customization (hide/show per activity)
- Settings screen reorganization
- Better error messages
- Input stability improvements

### Code Changes

**New Files:**
```
src/activities/tools/
├── ClockActivity.h/cpp
├── PomodoroActivity.h/cpp
├── SnakeActivity.h/cpp
├── TwentyFortyEightActivity.h/cpp
├── MazeGameActivity.h/cpp
├── GameOfLifeActivity.h/cpp
├── PhotoFrameActivity.h/cpp
├── DailyQuoteActivity.h/cpp
├── ConferenceBadgeActivity.h/cpp
├── ToolsActivity.h/cpp
├── VirtualPetActivity.h/cpp (basic version)
└── ...
```

**Modified:**
- Activity system for input customization
- Settings UI expanded

### Bug Fixes

- **BLE loop guard:** Prevent infinite BLE scan
- **WiFi sync:** Force NTP on WiFi connect
- **Photo frame:** Now uses `/photos` folder (not `/sleep`)

### Testing

- All 10 tools tested on hardware
- BLE stability verified
- Performance under load (games)

---

## [1.0.0] - 2025-06-15

### Initial Release

#### EPUB Reader
- Full EPUB 2 & 3 parsing
- Chapter text rendering with typography
- JPEG/PNG image support
- Word wrap and language-specific hyphenation (16 languages)
- Table of contents navigation
- Reading progress persistence

#### File Management
- File browser with folder navigation
- Drag-and-drop file upload via WebDAV
- Recent books list

#### Settings
- Font selection (Bookerly, Noto Sans, OpenDyslexic, Ubuntu)
- Font size (12pt - 18pt)
- Display orientation (4 rotations)
- Margins and line height adjustment
- Language selection (18 languages)
- Theme selection (Lyra)

#### Connectivity
- WiFi connection (SSID/password entry)
- OTA firmware updates
- Web-based file browser
- WebDAV file management
- Web settings interface
- KOReader Sync integration

#### Internationalization
- 382 UI strings
- 18 languages (82+ pages translated)
- Dynamic language switching

#### Hardware Support
- Xteink X4 (540x960 e-paper display)
- Button input (up, down, confirm, back)
- SD card storage
- WiFi connectivity
- Battery management (estimated 1+ week)

#### Development Infrastructure
- PlatformIO build system
- Clang-format code style
- Comprehensive test suite (EPUB edge cases, hyphenation)
- Debug serial monitor script

### Documentation

- User guide (USER_GUIDE.md)
- Development guide (docs/contributing/)
- File format documentation (docs/file-formats.md)
- i18n guide (docs/i18n.md)
- Hyphenation format (docs/hyphenation-trie-format.md)

### Known Limitations

- No user-provided fonts (built-in only)
- Limited UTF-8 (2-byte sequences)
- EPUB3 fixed-layout not supported
- Cache not auto-cleared when book deleted
- WiFi credentials stored in plaintext

---

## Version Numbering

**MAJOR.MINOR.PATCH**
- **MAJOR:** Significant architectural changes (v1→v2)
- **MINOR:** New features, subsystems (v1.0→v1.1)
- **PATCH:** Bug fixes, minor improvements (v1.0.0→v1.0.1)

---

## Release Process

1. Create release branch: `release/v1.2.0`
2. Update version in `platformio.ini`
3. Update changelog (this file)
4. Commit: `chore: bump version to 1.2.0`
5. Build firmware: `pio run`
6. Test on hardware
7. Create git tag: `v1.2.0`
8. Push to GitHub
9. Upload firmware.bin to GitHub Releases
10. Announce in discussions

---

## Future Roadmap

### v1.3 (Q2 2026)
- Pet networking (trade between devices)
- Cloud backup option
- Enhanced pet sprites (more variants)
- Creature interactions

### v2.0 (Q4 2026)
- User-provided fonts
- Full UTF-8 support
- EPUB3 fixed-layout
- Plugin/extension system
- Advanced pet AI

---

**Maintainers:** CrossPoint Development Team
**Repository:** https://github.com/crosspoint-reader/crosspoint-reader
**Issues:** https://github.com/crosspoint-reader/crosspoint-reader/issues
**Discussions:** https://github.com/crosspoint-reader/crosspoint-reader/discussions
