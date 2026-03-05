# Code Standards & Best Practices

**Last Updated:** March 4, 2026
**Language:** C++17 (ESP32 firmware)

## File Organization

### Directory Structure
```
src/
├── main.cpp                    # Program entry point
├── activities/                 # UI Activity hierarchy
│   ├── Activity.h             # Base class (virtual lifecycle)
│   ├── ActivityManager.h/cpp  # Activity stack & routing
│   ├── browser/               # EPUB reader & file picker
│   ├── network/               # Settings, OTA, WebDAV screens
│   ├── settings/              # Device configuration screens
│   ├── tools/                 # Clock, games, pet, pomodoro
│   └── util/                  # Message boxes, keyboards
├── pet/                        # Virtual pet subsystem
│   ├── PetState.h             # Data model (33 fields)
│   ├── PetManager.h/cpp       # Main controller
│   ├── PetActions.cpp         # 8 user actions
│   ├── PetPersistence.cpp     # JSON save/load
│   ├── PetDecayEngine.h/cpp   # Stat decay logic
│   ├── PetCareTracker.h/cpp   # Attention calls
│   ├── PetEvolution.h/cpp     # Evolution variants
│   └── PetSpriteRenderer.h/cpp # Rendering
├── network/                    # WiFi, OTA, WebDAV
├── ble/                        # Bluetooth remote control
├── components/                 # UI themes, icons
├── util/                       # Utility functions
├── i18n/                       # i18n YAML source files
├── CrossPointSettings.h/cpp    # Settings schema & I/O
├── CrossPointState.h/cpp       # Runtime state cache
├── MappedInputManager.h/cpp    # Button remapping
├── RecentBooksStore.h/cpp      # Reading history
├── SettingsList.h              # Settings UI list
└── I18nStrings.h/cpp          # Generated i18n lookup
```

### Naming Conventions

**Files:**
- Headers: `.h` (NO inline implementation)
- Implementation: `.cpp`
- Kebab-case for activity files: `my-activity.h/cpp` (EXCEPT class names stay PascalCase)
- Example: `pet-sprite-renderer.h` contains class `PetSpriteRenderer`

**Classes:**
- PascalCase: `PetManager`, `VirtualPetActivity`, `ActivityManager`
- Suffix with type: `*Activity`, `*Manager`, `*Engine`, `*Tracker`

**Functions/Methods:**
- camelCase: `feedMeal()`, `checkEvolution()`, `toggleLights()`
- Getter prefix: `get*()`, `is*()`
- Setter prefix: `set*()`

**Variables:**
- Local: camelCase: `currentMood`, `hungerValue`
- Member: camelCase with `m_` prefix: `m_hunger`, `m_petState`
- Constants: UPPER_SNAKE_CASE: `MAX_STAT`, `HUNGER_DECAY_PER_HOUR`
- Enums: Values in UPPER_SNAKE_CASE: `PetMood::HAPPY`, `PetStage::YOUNGSTER`

## C++ Style Guide

### Headers (.h)

**Structure:**
```cpp
#pragma once

#include <vector>
#include <cstdint>

#include "pet/PetState.h"
#include "GfxRenderer.h"

// Forward declarations (if needed)
class PetManager;

// Constants
constexpr uint8_t DEFAULT_HUNGER = 80;

// Enums
enum class PetAction : uint8_t {
  FEED_MEAL = 0,
  FEED_SNACK = 1,
  ACTION_COUNT
};

// Class definition (keep under 100 lines if possible)
class MyClass {
 public:
  explicit MyClass(GfxRenderer& renderer);
  ~MyClass() = default;

  // Public methods
  void init();
  int getValue() const;

 private:
  // Private members
  GfxRenderer& m_renderer;
  uint8_t m_hunger = 50;

  // Private methods
  void updateState();
};
```

**Rules:**
- Include guards: `#pragma once` (not `#ifndef`)
- Include order: stdlib → local → project
- Forward declarations for pointers/references
- Group public methods → private members → private methods
- Default member initializers

### Implementation (.cpp)

**Structure:**
```cpp
#include "PetManager.h"

#include "pet/PetState.h"
#include "pet/PetDecayEngine.h"

// Implementation
PetManager::PetManager() : m_petState(nullptr) {
  // Constructor body
}

void PetManager::feedMeal() {
  if (!m_petState) return;
  m_petState->hunger = std::min(100, m_petState->hunger + 25);
}

int PetManager::getValue() const {
  return m_hunger;
}
```

**Rules:**
- One class per file
- Include `#include "ClassName.h"` first
- Namespace not required (all in global or activity-specific)
- Use `std::min()`, `std::max()` for bounds

### Common Patterns

**Constructor with Dependency Injection:**
```cpp
explicit MyActivity(GfxRenderer& renderer, MappedInputManager& input)
    : Activity("My Activity", renderer, input) {}
```

**Virtual Method Override:**
```cpp
void onEnter() override;
void loop() override;
void render(RenderLock&&) override;
void onExit() override {}  // Empty if not used
```

**Const-Correct Methods:**
```cpp
void setState(const PetState& state);  // Takes const reference
PetState& getState() const;            // Returns const reference
bool isMoodHappy() const;              // Query method
```

**Null Safety:**
```cpp
if (!m_petManager) {
  return;  // Guard clause
}

// Safe to use m_petManager
m_petManager->feedMeal();
```

**Memory Limits (ESP32-C3: 380KB RAM):**
```cpp
// BAD: Large stack allocation
void bad() {
  uint8_t buf[10000];  // 10KB stack overflow!
}

// GOOD: Dynamic allocation or SD cache
void good() {
  std::vector<uint8_t> buf;
  buf.resize(1000);  // Heap allocation
}
```

## Enum Usage

**Pet Enums (from PetState.h):**

```cpp
// Life cycle
enum class PetStage : uint8_t {
  EGG = 0,
  HATCHLING = 1,
  YOUNGSTER = 2,
  COMPANION = 3,
  ELDER = 4,
  DEAD = 5
};

// Visual mood (for sprite selection)
enum class PetMood : uint8_t {
  HAPPY = 0,
  NEUTRAL = 1,
  SAD = 2,
  SICK = 3,
  SLEEPING = 4,
  DEAD = 5,
  NEEDY = 6,        // attention call active
  REFUSING = 7      // misbehaving
};

// Pet's actual need (drives attention call content)
enum class PetNeed : uint8_t {
  NONE = 0,
  HUNGRY = 1,
  SICK = 2,
  DIRTY = 3,
  BORED = 4,
  SLEEPY = 5
};

// User actions
enum class PetAction : uint8_t {
  FEED_MEAL = 0,
  FEED_SNACK = 1,
  MEDICINE = 2,
  EXERCISE = 3,
  CLEAN = 4,
  SCOLD = 5,
  IGNORE_CRY = 6,
  TOGGLE_LIGHTS = 7,
  PET_PET = 8,
  ACTION_COUNT
};
```

**Usage:**
```cpp
if (petState.stage == PetStage::YOUNGSTER) {
  // Handle youngster-specific logic
}

PetMood mood = determineMood(petState);
switch (mood) {
  case PetMood::HAPPY:
    renderHappySprite();
    break;
  case PetMood::SICK:
    renderSickSprite();
    break;
  // ...
}
```

## Data Structures

### PetState (33 fields, ~200 bytes)
```cpp
struct PetState {
  // Core
  bool initialized = false;
  PetStage stage = PetStage::EGG;
  uint8_t hunger = 80;
  uint8_t happiness = 80;
  uint8_t health = 100;

  // Timestamps
  uint32_t birthTime = 0;
  uint32_t lastTickTime = 0;

  // Progress
  uint32_t totalPagesRead = 0;
  uint16_t currentStreak = 0;
  uint16_t lastReadDay = 0;

  // Missions (reset daily)
  uint16_t missionDay = 0;
  uint8_t missionPagesRead = 0;
  uint8_t missionPetCount = 0;

  // Weight system
  uint8_t weight = 50;  // 0-100, normal=50

  // Sickness
  bool isSick = false;
  uint8_t sicknessTimer = 0;

  // Bathroom
  uint8_t wasteCount = 0;
  uint8_t mealsSinceClean = 0;

  // Discipline
  uint8_t discipline = 50;
  bool attentionCall = false;
  bool isFakeCall = false;
  PetNeed currentNeed = PetNeed::NONE;
  uint32_t lastCallTime = 0;

  // Sleep
  bool isSleeping = false;
  uint8_t lightsOff = 0;

  // Aging
  uint16_t totalAge = 0;
  uint8_t careMistakes = 0;

  // Evolution quality (v1.3+)
  uint8_t avgCareScore = 50;
  uint8_t evolutionVariant = 0;  // 0=Scholar, 1=Balanced, 2=Wild (reading-driven v1.3+)
  uint16_t booksFinished = 0;    // Book completion counter for evolution gates

  // Helpers
  bool isAlive() const { return stage != PetStage::DEAD; }
  bool exists() const { return initialized; }
};
```

## Game Balance Constants (PetConfig)

Located in `PetState.h`:

```cpp
namespace PetConfig {
  // Stat bounds
  constexpr uint8_t MAX_STAT = 100;

  // Gameplay loop
  constexpr uint16_t PAGES_PER_MEAL = 20;
  constexpr uint8_t HUNGER_PER_MEAL = 25;
  constexpr uint8_t HAPPINESS_PER_PET = 10;

  // Decay rates (per hour)
  constexpr uint8_t HUNGER_DECAY_PER_HOUR = 1;
  constexpr uint8_t HAPPINESS_DECAY_PER_HOUR = 1;
  constexpr uint8_t HEALTH_DECAY_PER_HOUR = 2;  // only when hungry

  // Weight system
  constexpr uint8_t WEIGHT_PER_MEAL = 5;
  constexpr uint8_t OVERWEIGHT_THRESHOLD = 80;

  // Sickness
  constexpr uint8_t SICK_RECOVERY_HOURS = 24;

  // Attention calls
  constexpr uint8_t FAKE_CALL_CHANCE_PERCENT = 30;
  constexpr uint32_t ATTENTION_CALL_INTERVAL_SEC = 14400;  // ~4 hours
  constexpr uint32_t ATTENTION_CALL_EXPIRE_SEC = 7200;     // 2 hours

  // Evolution requirements
  struct EvolutionReq {
    uint8_t minDays;
    uint16_t minPages;
    uint8_t minAvgHunger;
  };

  constexpr EvolutionReq EVOLUTION[] = {
    {1,   20,   0},   // EGG → HATCHLING
    {3,  100,  40},   // HATCHLING → YOUNGSTER
    {7,  500,  50},   // YOUNGSTER → COMPANION
    {14, 1500, 60},   // COMPANION → ELDER
  };
}
```

**How to Tune:**
- Increase `HUNGER_DECAY_PER_HOUR` → Pet gets hungry faster
- Decrease `PAGES_PER_MEAL` → Reading less provides more food
- Increase `FAKE_CALL_CHANCE_PERCENT` → More discipline tests
- Adjust `EVOLUTION[]` → Change evolution difficulty

## Activity Pattern

**Template:**
```cpp
// MyActivity.h
#pragma once

#include "../Activity.h"

class MyActivity : public Activity {
 public:
  explicit MyActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("My Activity", renderer, input) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int m_selectedIndex = 0;

  void handleInput();
};

// MyActivity.cpp
#include "MyActivity.h"

#include "ActivityManager.h"

void MyActivity::onEnter() {
  // Initialize state
  m_selectedIndex = 0;
}

void MyActivity::loop() {
  // Handle input & update state
  if (m_mappedInput.wasButtonPressed(InputButton::UP)) {
    m_selectedIndex = std::max(0, m_selectedIndex - 1);
  }

  if (m_mappedInput.wasButtonPressed(InputButton::DOWN)) {
    m_selectedIndex = std::min(2, m_selectedIndex + 1);
  }

  if (m_mappedInput.wasButtonPressed(InputButton::CONFIRM)) {
    // Execute action
  }

  if (m_mappedInput.wasButtonPressed(InputButton::BACK)) {
    ActivityManager::instance().popActivity();
  }
}

void MyActivity::render(RenderLock&&) {
  auto& renderer = m_renderer;
  renderer.fillRect(0, 0, 540, 960, GfxColor::WHITE);
  renderer.drawText(50, 50, "My Activity", GfxFont::NOTO_SANS_18_BOLD);
  // Draw UI elements
}
```

## Error Handling

**Guidelines:**
- No exceptions (ESP32 embedded constraint)
- Return `bool` for success/failure
- Guard clauses for null checks
- Log errors to serial (debug only)

```cpp
// BAD: No error handling
void bad() {
  PetManager* manager = new PetManager();
  manager->feedMeal();  // Null pointer?
}

// GOOD: Guard clauses
void good() {
  PetManager* manager = nullptr;
  if (!manager) {
    return;  // Early exit
  }

  if (m_petState->hunger >= 100) {
    return;  // Pet already full
  }

  manager->feedMeal();
}

// GOOD: Result codes
bool feedPet(const PetState& state) {
  if (!state.initialized) return false;
  if (state.hunger >= 100) return false;
  // Process feeding
  return true;
}
```

## Persistence (JSON)

**Pattern for Pet State:**
```cpp
// src/pet/PetPersistence.cpp

bool PetPersistence::save(const PetState& state) {
  StaticJsonDocument<1024> doc;

  doc["initialized"] = state.initialized;
  doc["stage"] = (uint8_t)state.stage;
  doc["hunger"] = state.hunger;
  doc["happiness"] = state.happiness;
  // ... 30 more fields

  File file = SD.open(PetConfig::STATE_PATH, FILE_WRITE);
  if (!file) return false;

  serializeJson(doc, file);
  file.close();
  return true;
}

bool PetPersistence::load(PetState& state) {
  File file = SD.open(PetConfig::STATE_PATH);
  if (!file) return false;

  StaticJsonDocument<1024> doc;
  deserializeJson(doc, file);
  file.close();

  state.initialized = doc["initialized"];
  state.stage = (PetStage)(uint8_t)doc["stage"];
  state.hunger = doc["hunger"];
  // ... deserialize all fields

  return true;
}
```

## Reading Stats System (src/ReadingStats.h/cpp)

**Purpose:** Track daily and lifetime reading statistics with binary persistence.

**Data Structure:**
```cpp
struct ReadingSession {
  uint32_t startTime = 0;           // Unix timestamp
  uint32_t endTime = 0;
  uint32_t totalSeconds = 0;        // Duration of reading session
  char lastBookPath[256] = "";      // Last-read EPUB file path
  uint8_t lastProgressPercent = 0;  // Book progress at session end
};

class ReadingStats {  // Singleton
  uint32_t todaysReadingSeconds = 0;   // Reset at midnight
  uint32_t lifetimeReadingSeconds = 0; // Cumulative total
  uint16_t currentStreak = 0;          // Consecutive days reading
  ReadingSession currentSession;       // In-progress session
};
```

**Usage Pattern:**
```cpp
// Start reading session (on EpubReaderActivity::onEnter)
READ_STATS.startSession();

// End reading session (on EpubReaderActivity::onExit)
READ_STATS.endSession(bookPath, progressPercent);

// Load stats on boot (in main.cpp setup)
READ_STATS.loadFromFile();

// Display on sleep screen
SleepActivity::renderReadingStatsSleepScreen();
```

**Persistence:**
- **File:** `/.crosspoint/reading_stats.bin` (binary format)
- **Load on boot:** Automatically in main.cpp
- **Save on session end:** Atomic write with temp file + rename
- **Reset daily:** Midnight boundary check (using RTC time)

**Files:**
- `src/ReadingStats.h` - Header (data structures, API)
- `src/ReadingStats.cpp` - Implementation (load, save, session tracking)

## Internationalization (i18n)

**Adding a New String:**

1. Add to `src/i18n/english.yaml`:
```yaml
STR_PET_FEED_MEAL: "Feed a meal"
STR_PET_FEED_SNACK: "Feed a snack"
STR_PET_MEDICINE: "Give medicine"
```

2. Run code generator:
```bash
python3 tools/regenerate_i18n.py
```

3. Commit generated files:
- `src/I18nKeys.h` (enum)
- `src/I18nStrings.h` (declarations)
- `src/I18nStrings.cpp` (implementations)
- `src/i18n/*.yaml` (all languages)

4. Use in code:
```cpp
const char* actionLabel = getStr(STR_PET_FEED_MEAL);
```

**Current Status:**
- 419 total keys
- 18 languages
- 37 pet-specific keys (STR_PET_*)

## Code Quality Checklist

**Before Committing:**

- [ ] No compilation warnings
- [ ] No `TODO` comments left behind
- [ ] File under 200 LOC (modularize if longer)
- [ ] Function under 50 lines (extract if longer)
- [ ] Const-correct methods
- [ ] Memory bounds checked (ESP32 limited)
- [ ] Proper error handling (no exceptions)
- [ ] i18n strings used instead of hardcoded text
- [ ] Game balance constants in PetConfig (not magic numbers)
- [ ] Comments for complex logic only

**Clang-Format:**
```bash
# Auto-format all files
clang-format -i src/**/*.{h,cpp}

# Check without modifying
clang-format --output-replacements-xml src/MyFile.cpp
```

## Testing Strategy

**Unit Tests:**
- Hyphenation evaluation: `test/hyphenation_eval/`
- EPUB edge cases: `test/epubs/`

**Manual Testing:**
1. Flash to device
2. Test on physical X4
3. Verify display, input, WiFi
4. Check SD card state files

## Documentation Standards

**Comment Style:**
```cpp
// Single line comment for simple explanation

// Multi-line comment for complex logic
// - First point
// - Second point
// - Third point

/* Rarely used: block comment style */
```

**Function Documentation (if complex):**
```cpp
// Calculates pet mood based on stats
// Returns: PetMood reflecting current stat averages
// Note: Attention call takes priority over hunger/sickness
PetMood calculateMood(const PetState& state) {
  // Implementation
}
```

## Release Checklist

Before cutting a release:

- [ ] Increment version in `platformio.ini`
- [ ] Update `docs/codebase-summary.md` (Recent Changes section)
- [ ] Update project changelog
- [ ] Run full test suite
- [ ] Test on physical device
- [ ] Build final firmware binary
- [ ] Tag commit with version
- [ ] Upload binary to GitHub releases

## Performance Targets

- **Display refresh:** <500ms full, <100ms partial
- **Activity transition:** <100ms (no stutter)
- **Pet action:** <50ms (immediate feedback)
- **EPUB pagination:** <200ms between chapters
- **RAM usage:** <300KB peak (leaving 80KB buffer)
