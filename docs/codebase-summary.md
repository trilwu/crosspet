# CrossPoint Reader - Codebase Summary

**Last Updated:** March 4, 2026
**Version:** 1.4.0 (In Development)
**Total Files:** 495 | **Total Tokens:** 19.2M | **Languages:** C++, Python, HTML

## Project Overview

CrossPoint Reader is an open-source firmware for the Xteink X4 e-paper device built on PlatformIO (ESP32-C3). It provides:
- Full EPUB 2/3 parsing and rendering with image support
- Custom sleep screens and interactive tools (clock, games, virtual pet, weather, news)
- WiFi-based OTA updates and file uploads
- KOReader Sync integration for cross-device reading progress
- Multi-language support (18 languages)
- Customizable UI themes and layouts
- Reading statistics tracking and sleep screen display

**Key Architecture Principle:** Aggressive SD card caching to minimize ESP32-C3 RAM usage (380KB available).

## Directory Structure

```
├── src/                                    # Main firmware source
│   ├── main.cpp                           # Program entry point
│   ├── activities/                        # UI activity system
│   │   ├── browser/                       # EPUB reader, file browser
│   │   ├── network/                       # WiFi server, OTA, WebDAV
│   │   ├── settings/                      # Settings screens
│   │   ├── tools/                         # Clock, games, virtual pet
│   │   ├── util/                          # Message dialogs, keyboards
│   │   └── Activity.h                     # Base activity class
│   ├── pet/                               # Virtual pet subsystem (NEW)
│   │   ├── PetState.h                     # Core data structures
│   │   ├── PetManager.h/cpp               # Main pet controller
│   │   ├── PetManager.cpp → split into:
│   │   │   ├── PetActions.cpp             # 8 pet interaction methods
│   │   │   ├── PetPersistence.cpp         # JSON save/load
│   │   │   └── PetManager.cpp             # Core lifecycle
│   │   ├── PetDecayEngine.h/cpp           # Hourly stat decay logic
│   │   ├── PetCareTracker.h/cpp           # Care mistakes & attention calls
│   │   ├── PetEvolution.h/cpp             # Evolution stage variants
│   │   └── PetSpriteRenderer.h/cpp        # Pixel-art rendering
│   ├── ble/                               # Bluetooth remote control
│   ├── components/                        # UI themes, icons
│   ├── network/                           # Web server, OTA, WebDAV
│   ├── util/                              # Utility functions
│   └── CrossPoint{Settings,State}.h/cpp   # Global config & state
├── lib/                                   # Third-party libraries
│   ├── Epub/                             # EPUB parsing
│   ├── EpdFont/                          # Built-in fonts
│   ├── expat/                            # XML parsing
│   └── hal/                              # Hardware abstraction
├── test/                                  # Unit tests & resources
├── tools/                                 # Build utilities
├── docs/                                  # User & developer documentation
└── platformio.ini                         # PlatformIO config

```

## Core Subsystems

### 1. Activity System (src/activities/)
**Pattern:** Hierarchical event-driven UI with transition management

- **Base:** `Activity.h` - virtual lifecycle: onEnter → loop → render → onExit
- **Manager:** `ActivityManager.cpp` - activity stack, event routing
- **Rendering:** Double-buffered GfxRenderer with partial display updates
- **Input:** `MappedInputManager` - button/D-pad/touchpad event mapping

**Key Activities:**
- `OpdsBookBrowserActivity` - EPUB reader (main)
- `VirtualPetActivity` - Interactive pet with action menu + stats panel
- `ToolsActivity` - Launcher for 11 tools (Clock, Pomodoro, VirtualPet, 2048, Sudoku, Minesweeper, Presenter, Caro, Chess, Weather, News)
- `SettingsActivity` - Device configuration screens
- `HomeActivity` - Home screen with cover panel, recent books, grid menu, optional header clock

### 2. Virtual Pet Subsystem (src/pet/) [NEW - v1.2.0]
**Purpose:** Tamagotchi-style interactive pet with full gameplay loop

#### Data Model (`PetState.h`)
- **Life Cycle:** EGG → HATCHLING → YOUNGSTER → COMPANION → ELDER → DEAD
- **Mood System:** 8 moods (HAPPY, NEUTRAL, SAD, SICK, SLEEPING, DEAD, NEEDY, REFUSING)
- **Needs System:** HUNGRY, SICK, DIRTY, BORED, SLEEPY (drives attention calls)
- **Stats:** hunger, happiness, health (0-100 scale)
- **Advanced:** weight system, sickness tracking, discipline, waste counter, care mistakes
- **Persistence:** 33 JSON fields saved to `.crosspoint/pet/state.json`

#### Core Engine
- **PetManager.h/cpp** - Lifecycle controller: hatchNew(), tick(), checkEvolution()
- **PetDecayEngine.h/cpp** - Hourly stat decay with sleep cycle skip
- **PetCareTracker.h/cpp** - Attention call generation (30% fake), care mistake tracking
- **PetPersistence.cpp** - JSON serialization (33 fields)
- **PetActions.cpp** - 8 user actions: feedMeal, feedSnack, giveMedicine, exercise, cleanBathroom, disciplinePet, ignoreCry, toggleLights
- **PetEvolution.cpp** - Multi-stage evolution with 3 variants (good/chubby/misbehaved)
- **PetSpriteRenderer.cpp** - Mood-based pixel-art sprite rendering with fallbacks

#### UI Components (`src/activities/tools/`)
- **PetActionMenu.h/cpp** - Scrollable action list with availability guards
- **PetStatsPanel.h/cpp** - Status icons (ZZ/+/**/!/♥), stat bars, mistake counter
- **VirtualPetActivity.cpp** - Main interaction screen (sprite + menu + stats)

#### Game Balance (PetConfig constants)
- Evolution requires 1-14 days + 20-1500 pages read at each stage
- Attention calls every ~4 hours (30% fake); expire in 2 hours
- Sickness from overfeeding/neglect; cured with medicine
- Weight tracking: overweight (>80) from overeating, underweight (<20) from undereating
- Discipline system: rewards ignoring fake calls, punishes indulgence
- Daily missions (reset each day): read 20 pages, pet 3x

### 3. Reading Stats System (src/ReadingStats.h/cpp) [NEW - v1.4.0]
**Purpose:** Track daily and lifetime reading metrics, display on sleep screen

**Features:**
- Session tracking: `READ_STATS.startSession()` (on EpubReaderActivity entry), `READ_STATS.endSession()` (on exit)
- Binary persistence at `/.crosspoint/reading_stats.bin`
- Daily reset at midnight boundary
- Sleep screen display mode: `READING_STATS=7` shows today's total, all-time total, last book + progress
- Auto-load on boot in `main.cpp`
- Clock refresh: Brief power button press re-renders dynamic sleep screens

**Fields:**
- `todaysReadingSeconds` - Reset daily
- `lifetimeReadingSeconds` - Cumulative across all sessions
- `currentStreak` - Consecutive days with reading activity
- `currentSession` - In-progress session with start/end times

### 4. Game & Tool Activities (src/activities/tools/) [v1.4.0]
**New Tools (5 activities):**
- **ChessActivity** - Full chess with move validation and AI
- **CaroActivity** - Caro/Gomoku tile-matching strategy game
- **WeatherActivity** - Real-time weather display (network-dependent)
- **NewsReaderActivity** - RSS feed reader for news
- **DailyQuoteActivity** - 28 rotating literary quotes, daily tracking
- Plus existing: Clock, Pomodoro, VirtualPet, 2048, Sudoku, Minesweeper, Presenter

**ToolsActivity UI:**
- Launcher menu with 11 items
- Sleep screen: Daily quotes (28 rotating) at bottom of CLOCK mode, shifted left for pet sprite

### 5. EPUB Processing (lib/Epub/)
**Flow:**
1. Parse EPUB archive & OPF metadata
2. Extract spine (reading order) & table of contents
3. Render chapters via libxml2 + custom HTML→text layout engine
4. Cache parsed sections to SD card (`.crosspoint/epub_<hash>/sections/`)

**Key Components:**
- `Epub.h/cpp` - Archive parser, spine management
- `EpubRenderer.h` - Chapter text layout & pagination
- `Hyphenation/` - Language-specific hyphenation rules (16 languages via .trie.h)

### 4. Web Server (src/network/)
**Features:**
- REST API: file listing, OTA upload, settings read/write
- WebDAV: drag-and-drop file management
- HTML5 web interface for settings & file browser

**Routes:**
- `/` - HomePage (device info, recent books)
- `/files` - File explorer with drag-drop upload
- `/settings` - Configuration dashboard

### 5. Settings & Persistence (src/CrossPointSettings.h)
**Strategy:** JSON-based settings stored on SD card

- Font/size selection (Bookerly, Noto Sans, OpenDyslexic, Ubuntu)
- Display orientation (0°, 90°, 180°, 270°)
- Layout (margins, line height, page alignment)
- WiFi credentials (encrypted storage planned)
- KOReader sync tokens
- Theme selection (Lyra, etc.)

### 6. Internationalization (i18n)
**System:** 419 total string keys, 18 languages

- **Base:** `src/I18nKeys.h` (enum), `src/I18nStrings.h/cpp` (lookup)
- **Data:** `src/i18n/` (language YAML files)
- **Tools:** `tools/regenerate_i18n.py` (code generation)
- **Pet Strings:** 37 new STR_PET_* keys in v1.2.0 (actions, moods, needs)

## Design Patterns

### 1. Activity-Based UI
```cpp
class MyActivity : public Activity {
  void onEnter() override;      // Initialize
  void loop() override;          // Handle input
  void render(RenderLock&&) override; // Draw
};
```
- Activities form a stack (push/pop for navigation)
- Mutual exclusion: only top activity receives input

### 2. Lazy Rendering & Caching
- Partial screen updates via dirty-rect marking
- EPUB chapters cached after first render
- Pet state/sprites precomputed and cached

### 3. Memory Management
- Aggressive SD card usage to minimize RAM
- String pools & constant data in PROGMEM
- Single-buffered rendering for tight RAM budgets

### 4. Event Routing
- `MappedInputManager`: hardware buttons → logical events (up, down, confirm, back)
- Button remapping via settings (customizable per activity)

## Recent Changes (v1.2.0)

**Major Additions:**
- Complete virtual pet subsystem with 10+ new files
- 8 pet actions with game balance tuning
- Evolution branching (3 variants per stage)
- Attention call system with fake call mechanic
- Weight/sickness/waste/discipline mechanics
- New UI components: PetActionMenu, PetStatsPanel
- 37 new i18n keys for pet-related strings

**Commits:**
- `36606a2` - feat: virtual pet improvements and modern clock sleep screen
- `b865e58` - feat: cute pet sprites, daily missions, clock sleep screen calendar

**File Changes:**
- Added: 10 files in `src/pet/`
- Added: 2 files in `src/activities/tools/` (PetActionMenu, PetStatsPanel)
- Modified: VirtualPetActivity (major refactor)
- Modified: SleepActivity (attention indicator, speech bubbles)
- Modified: I18nKeys.h/I18nStrings.h/cpp (419 keys total)
- Modified: english.yaml, vietnamese.yaml (+37 STR_PET_* keys)

## Code Quality Metrics

- **Compilation:** No warnings, C++17 standard
- **File Size:** Max 200 LOC per file (modular design)
- **Test Coverage:** Hyphenation evaluation suite; EPUB edge-case tests
- **Linting:** Clang-format (.clang-format included)

## Build & Deployment

**Build Command:**
```bash
pio run          # Compile for target
pio run --target upload  # Flash to device
```

**Binary Output:** `firmware.bin` (flashed via web interface at xteink.dve.al)

**Deployment Channels:**
- Latest: OTA auto-update via WebDAV
- Releases: GitHub releases with version tags

## Dependencies

- **PlatformIO** - Build system
- **esp-idf** - ESP32 SDK
- **libxml2/expat** - XML parsing
- **ArduinoJSON** - JSON handling
- **JPEGDEC** - JPEG decoding
- **ArduinoBLE** - Bluetooth LE support

## Entry Points for Developers

1. **Reading Features:** Start with `OpdsBookBrowserActivity` (src/activities/browser/)
2. **UI Development:** Study `Activity.h` and `ActivityManager.cpp`
3. **Pet Gameplay:** Read `src/pet/PetState.h` (data model) → `PetManager.h` (controller)
4. **Settings:** Examine `CrossPointSettings.h` and `JsonSettingsIO.cpp`
5. **Internationalization:** Add keys to `src/i18n/english.yaml`, regenerate with `tools/regenerate_i18n.py`

## Known Limitations

- Single-threaded event loop (no concurrent file I/O)
- Limited font support (built-in fonts only; user fonts TBD)
- Cache not auto-cleared when EPUB deleted
- No UTF-8 beyond 2-byte sequences (limitation of current font system)

## Future Work

- [ ] Persistent WiFi credentials
- [ ] User-provided fonts
- [ ] Full UTF-8 support
- [ ] EPUB cover art in file picker
- [ ] Pet networking/trading (multiplayer)
- [ ] Cloud backup of pet state
