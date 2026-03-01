# System Architecture

**Last Updated:** March 1, 2026
**Version:** 1.2.0

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  User Interface Layer                    │
│  (Activities: Reader, Settings, Pet, Games, Tools)      │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│              Activity Manager & Input System             │
│  (Event routing, activity stack, button mapping)        │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┼────────────┐
        │            │            │
┌───────▼──┐  ┌─────▼────┐  ┌───▼──────┐
│   Pet    │  │   EPUB   │  │  Network │
│ Subsystem│  │ Processor│  │ & WiFi   │
└───────┬──┘  └─────┬────┘  └───┬──────┘
        │           │            │
┌───────▼───────────▼────────────▼──────────┐
│         Settings & Persistence Layer       │
│  (JSON config, SD card caching, I18n)     │
└───────┬────────────────────────────────────┘
        │
┌───────▼────────────────────────────────────┐
│      Hardware Abstraction Layer (HAL)      │
│  (GPIO, display, clock, SD card, BLE)     │
└────────────────────────────────────────────┘
        │
┌───────▼────────────────────────────────────┐
│          ESP32-C3 Hardware                  │
│  (SoC, e-paper display, SD card, WiFi)    │
└────────────────────────────────────────────┘
```

## Component Architecture

### 1. Activity System (Core UI Framework)

**File:** `src/activities/Activity.h`, `src/activities/ActivityManager.h`

**Design Pattern:** State machine with stack-based navigation

```
┌─────────────────────────┐
│   ActivityManager       │
│  (Singleton)            │
│ ┌───────────────────┐   │
│ │ Stack of Activities   │
│ │  [Bottom] → [Top]    │
│ └───────────────────┘   │
└──────────┬──────────────┘
           │
     ┌─────▼──────┐
     │ MappedInput│
     │ Manager    │
     └─────┬──────┘
           │
    ┌──────▼────────┐
    │ Active Activity
    │ (Top of stack) │
    └───────────────┘
        │     ▲
        │ loop()
        │     │
        ▼     │
    ┌────────────┐
    │ Input: up, │
    │ down,      │
    │ confirm,   │
    │ back       │
    └────────────┘
```

**Activity Lifecycle:**
```cpp
// Virtual methods override these
void onEnter();         // Initialize (called when pushed)
void loop();           // Handle input & update state
void render(lock);     // Draw to screen
void onExit();         // Cleanup (called when popped)
```

**Navigation:**
- `pushActivity()` - Push new activity, pause current
- `popActivity()` - Return to previous activity
- `replaceActivity()` - Replace top activity

### 2. Virtual Pet Subsystem

**Location:** `src/pet/`

**Data Flow:**

```
┌──────────────────┐
│   PetState       │ ← Persistent state (33 fields)
│  (in memory)     │
└────────┬─────────┘
         │
    ┌────▼────────────────────────────────┐
    │ Main Update Loop (every hour)        │
    │  1. PetDecayEngine::decay()          │
    │  2. PetCareTracker::checkAttention() │
    │  3. PetManager::checkEvolution()     │
    │  4. PetManager::checkSick()          │
    └────┬─────────────────────────────────┘
         │
    ┌────▼─────────────────────────┐
    │ User Action (immediate)       │
    │ feedMeal() / giveMedicine()   │
    │ exercise() / cleanBathroom()  │
    │ disciplinePet() / ignoreCry() │
    └────┬──────────────────────────┘
         │
    ┌────▼─────────────────────┐
    │ Persistence               │
    │ PetPersistence::save()    │
    │ (async to SD card)        │
    └───────────────────────────┘
```

#### Core Components

**PetManager (Controller)**
- Owns PetState instance
- Orchestrates decay, evolution, sickness checks
- Delegates actions to PetActions.cpp
- Saves/loads via PetPersistence

**PetDecayEngine**
- Hourly stat decay: hunger-1, happiness-1, health-2 (if starving)
- Sleep cycle skip: no decay 10 PM → 7 AM
- Weight normalization: creeping toward 50

**PetCareTracker**
- Attention call generation: every ~4 hours
- 30% chance call is "fake" (discipline test)
- Tracks care mistakes (hungry >6h, sick >12h, dirty >4h)
- Rolling average care score for evolution quality

**PetActions**
- `feedMeal()` - Hunger-25, weight+5, 3 meals until waste
- `feedSnack()` - Hunger-10, happiness+15, weight+3
- `giveMedicine()` - Cures sickness, happiness-20 (bad taste)
- `exercise()` - Happiness+20, weight-8 (1h cooldown)
- `cleanBathroom()` - Clears waste, happiness+10
- `disciplinePet()` - discipline+10 if misbehaving
- `ignoreCry()` - discipline+5 if fake call, discipline-5 if real
- `toggleLights()` - Affects sleep state on sleep screen

**PetEvolution**
- 5 stages: EGG → HATCHLING → YOUNGSTER → COMPANION → ELDER
- Stage transitions triggered by: days alive + pages read + hunger average
- 3 variants per stage (after YOUNGSTER):
  - Variant 0 (Good): high avg care score
  - Variant 1 (Chubby): weight >80 at evolution
  - Variant 2 (Misbehaved): discipline <40 at evolution
- Different sprite sets per variant

**PetSpriteRenderer**
- Mood-based sprite selection (8 moods → different visuals)
- Variant-aware rendering
- Pixel-art fallback if variant sprite missing
- Status icons: ZZ (sleeping), + (happy), * (confused), ! (needy), ♥ (loving)

#### UI Layer (src/activities/tools/)

**VirtualPetActivity**
- Main interaction screen
- Renders: pet sprite + action menu + stats panel
- Input: up/down (menu), confirm (action), back (exit)

**PetActionMenu**
- Scrollable list of 9 actions
- Availability guards (e.g., can't exercise with 1h cooldown)
- Highlights selected action
- Shows "(unavailable)" for locked actions

**PetStatsPanel**
- Renders 5 stat bars: hunger, happiness, health, weight, discipline
- Shows status icons (sick, sleeping, calling)
- Displays care mistake counter
- Evolution stage & variant name

#### Persistence

**Format:** JSON (33 fields)
**Location:** `/.crosspoint/pet/state.json`

**Fields:**
- Core stats: hunger, happiness, health, birthTime, lastTickTime
- Progress: totalPagesRead, currentStreak, lastReadDay, pageAccumulator
- Missions: missionDay, missionPagesRead, missionPetCount
- Weight: weight (0-100)
- Sickness: isSick, sicknessTimer
- Bathroom: wasteCount, mealsSinceClean
- Discipline: discipline, attentionCall, isFakeCall, currentNeed, lastCallTime
- Sleep: isSleeping, lightsOff
- Aging: totalAge, careMistakes
- Evolution: avgCareScore, evolutionVariant

### 3. EPUB Processing Pipeline

**Location:** `lib/Epub/`

**Flow:**

```
EPUB File (ZIP archive)
    │
    ▼
Epub::open()
    │
    ├─ Parse mimetype
    ├─ Parse container.xml → rootfile path
    ├─ Parse OPF metadata:
    │   ├─ Title, author, publisher
    │   ├─ Spine (reading order)
    │   ├─ Manifest (all resources)
    │   └─ Table of contents
    │
    ▼
Epub::renderChapter(index)
    │
    ├─ Check SD cache: .crosspoint/epub_<hash>/sections/<index>.bin
    ├─ If cached: deserialize & return
    │
    ├─ If not cached:
    │   ├─ Extract chapter XHTML from archive
    │   ├─ Parse & layout text:
    │   │   ├─ Strip HTML tags
    │   │   ├─ Word-wrap to display width
    │   │   ├─ Hyphenation (language-specific)
    │   │   ├─ Page break calculation
    │   ├─ Serialize section data
    │   ├─ Cache to SD: sections/<index>.bin
    │   └─ Return
    │
    ▼
Display rendered chapter
```

**Key Files:**
- `Epub.h/cpp` - Archive parser, spine/TOC access
- `EpubRenderer.h` - Chapter rendering pipeline
- `Hyphenation/` - Language .trie.h files (16 languages)

### 4. Settings & State Management

**Files:**
- `src/CrossPointSettings.h` - Schema (fonts, display, layout, theme)
- `src/JsonSettingsIO.cpp` - JSON I/O
- `src/CrossPointState.cpp` - Runtime state (current book, page)

**Persistence:**
```
SD Card Layout:
  /.crosspoint/
    ├─ settings.json      # Display, layout, theme
    ├─ credentials.json   # WiFi, KOReader (encrypted TBD)
    ├─ pet/
    │   └─ state.json    # Virtual pet state (33 fields)
    ├─ recent_books.json  # Recent reading history
    └─ epub_<hash>/
        ├─ book.bin      # Cached metadata
        ├─ progress.bin  # Reading position
        └─ sections/
            ├─ 0.bin    # Chapter 0
            └─ ...
```

### 5. Network & Web Server

**Location:** `src/network/`

**Architecture:**

```
┌─────────────────┐
│  WiFi Module    │
│ (esp_wifi APIs) │
└────────┬────────┘
         │
    ┌────▼──────────────┐
    │ CrossPointWebServer
    │ (HTTP server)      │
    └────┬───────────────┘
         │
    ┌────▼────────────────────┐
    │ Route Handlers:          │
    ├─ GET /                   │
    ├─ GET /files             │
    ├─ POST /upload (WebDAV)  │
    ├─ POST /settings         │
    ├─ POST /ota              │
    └──────────────────────────┘
         │
         ▼
    ┌──────────────────┐
    │ File operations  │
    │ (SD card)        │
    └──────────────────┘
```

**OTA Update:**
- POST `/api/ota` with binary firmware.bin
- `OtaUpdater` writes to inactive partition
- Device reboots, bootloader switches partitions
- Automatic rollback if boot fails

### 6. Internationalization (i18n)

**System:**

```
┌──────────────────────────┐
│  src/i18n/*.yaml         │ ← Source language definitions
│  (english.yaml, etc.)    │
└────────┬─────────────────┘
         │
    ┌────▼──────────────────┐
    │ tools/regenerate_i18n.py
    │ (code generator)       │
    └────┬──────────────────┘
         │
    ┌────▼──────────────────────────┐
    │ Generated C++ Headers:         │
    ├─ src/I18nKeys.h (enum)       │
    ├─ src/I18nStrings.h/cpp       │
    │  (419 total keys, 18 langs)  │
    └────────────────────────────────┘
         │
         ▼
    ┌──────────────────────────┐
    │ At Runtime:              │
    │ getStr(STR_HELLO)        │
    │ → Current language text  │
    └──────────────────────────┘
```

**Key String Categories:**
- UI labels (buttons, menus)
- Activity titles
- Error messages
- Book metadata
- Pet actions/moods/needs (STR_PET_*)

## Data Flow Diagrams

### User Reading an EPUB

```
Reader Activity
    │ onEnter()
    ├─ Load metadata from .crosspoint cache
    ├─ Restore reading position
    │
    ▼ loop()
    │ Button: next page
    │   ├─ currentPage++
    │   ├─ Check if needs next chapter
    │   │
    │   ▼ EpubRenderer::renderChapter()
    │     ├─ Check cache
    │     ├─ If miss: parse XHTML, layout, cache
    │     └─ Return RenderedChapter
    │
    ├─ GfxRenderer::render() ← Send pixels to display
    │
    ├─ Every 30s: save progress to cache
    │
    └─ Button: back
        └─ popActivity()
```

### Pet Interaction Loop

```
VirtualPetActivity
    │ onEnter()
    ├─ Load PetState from /.crosspoint/pet/state.json
    ├─ Schedule hourly decay tick
    │
    ▼ loop()
    │ Input: up/down ← Menu navigation
    │ Input: confirm ← Execute selected action
    │
    ├─ PetManager::feedMeal()
    │   ├─ hunger -= 25
    │   ├─ weight += 5
    │   ├─ Check if sick (overfeeding)
    │   └─ PetPersistence::save()
    │
    ├─ PetManager::checkAttention()  ← Every ~4 hours
    │   ├─ 30% chance fake call
    │   ├─ Set attentionCall = true
    │   └─ Store in currentNeed
    │
    ├─ PetManager::checkEvolution()  ← When thresholds met
    │   ├─ Determine variant (good/chubby/misbehaved)
    │   ├─ stage++
    │   └─ Reset evolution counters
    │
    ├─ Render:
    │   ├─ Sprite (mood-based)
    │   ├─ Action menu (with availability icons)
    │   ├─ Stats panel (bars + icons)
    │   └─ Speech bubble (need-specific)
    │
    └─ Button: back
        ├─ PetPersistence::save()
        └─ popActivity()
```

## Memory & Performance Constraints

**RAM (380KB usable):**
- Pet state: ~200 bytes
- EPUB chapter buffer: ~20KB (max visible text)
- Render buffer: ~40KB (partial updates)
- Remaining: ~320KB for code, strings, heaps

**Strategy:**
- Aggressive SD caching (EPUB chapters, pet data)
- Single-threaded event loop (no concurrent operations)
- Render-on-demand (no unnecessary redraws)
- String pooling & PROGMEM constants

**Display:**
- 540x960 pixels, 16-level grayscale
- Partial update support (fast local changes)
- Full refresh every 20 pages (ghosting mitigation)

## Build & Runtime Flow

**Build:**
```bash
pio run           # Compile, link, strip
```

**Runtime (after boot):**
```
ESP32 ROM bootloader
    │
    ▼ Load PlatformIO/IDF bootloader
    │
    ▼ Load firmware.bin from partition
    │
    ▼ main()
    │ ├─ Initialize HAL (GPIO, display, SD)
    │ ├─ Mount SD card
    │ ├─ Load settings from settings.json
    │ ├─ Load pet state (if exists)
    │ ├─ Initialize WiFi (off by default)
    │ ├─ Create ActivityManager & push OpdsBookBrowserActivity
    │
    ▼ Main loop
    │ ├─ ActivityManager::loop()
    │   ├─ Dispatch input to active activity
    │   ├─ Activity processes & updates state
    │   ├─ Request render
    │ ├─ GfxRenderer::update()
    │   ├─ Render active activity
    │   ├─ Send pixels to e-paper controller
    │   ├─ Wait for frame completion
    │ └─ ~16ms per iteration (60 FPS target)
    │
    └─ Loop continues until power off
```

## Security Considerations

**Current State:**
- WiFi credentials stored in plain JSON (TBD: encryption)
- KOReader sync tokens in plain JSON (TBD: encryption)
- No authentication on web server (device assumes private network)

**Future:**
- Credential encryption with device key
- HTTPS for web server
- OAuth2 for cloud services

## Extension Points

**Adding New Activity:**
1. Create `src/activities/MyActivity.h/cpp`
2. Inherit from `Activity`
3. Implement `onEnter()`, `loop()`, `render()`, `onExit()`
4. Add to `ActivityManager` routing

**Adding New Pet Action:**
1. Add enum value to `PetAction`
2. Implement method in `PetActions.cpp`
3. Add availability guard in `PetActionMenu::isActionAvailable()`
4. Add i18n key to `english.yaml` (STR_PET_ACTION_*)

**Adding New Language:**
1. Create `src/i18n/<lang>.yaml`
2. Run `tools/regenerate_i18n.py`
3. Commit generated files + YAML

## Version History

- **v1.2.0** (Mar 2026) - Virtual pet full feature with evolution, care system
- **v1.1.x** - Clock, Pomodoro, games (Snake, 2048), photo frame
- **v1.0.x** - Core reader, WiFi, OTA, KOReader sync
