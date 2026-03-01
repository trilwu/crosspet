#pragma once

#include <cstdint>

// Evolution stages for the virtual pet
enum class PetStage : uint8_t {
  EGG = 0,
  HATCHLING = 1,
  YOUNGSTER = 2,
  COMPANION = 3,
  ELDER = 4,
  DEAD = 5
};

// Visual mood derived from stats (used for sprite selection)
enum class PetMood : uint8_t {
  HAPPY = 0,    // hunger > 70 && health > 70
  NEUTRAL = 1,  // hunger > 30 && health > 30
  SAD = 2,      // low stats
  SICK = 3,     // isSick flag set
  SLEEPING = 4, // shown on sleep screen
  DEAD = 5,
  NEEDY = 6,    // attentionCall active (icon blink)
  REFUSING = 7, // misbehaving (low discipline)
};

// What the pet actually needs (used for attention call content)
enum class PetNeed : uint8_t {
  NONE = 0,
  HUNGRY = 1,
  SICK = 2,
  DIRTY = 3,
  BORED = 4,
  SLEEPY = 5,
};

// Pet species types — cosmetic label for the pet
namespace PetTypeNames {
  constexpr const char* NAMES[] = {
    "Default", "Cat", "Dog", "Dragon", "Bunny", "Robot", "Bear", "Slime"
  };
  constexpr uint8_t COUNT = 8;
  inline const char* get(uint8_t t) { return (t < COUNT) ? NAMES[t] : NAMES[0]; }
}

// Persistent pet state — serialized to JSON on SD card
struct PetState {
  bool initialized = false;  // true after hatchNew(); prevents false-positive exists() with no clock
  PetStage stage = PetStage::EGG;

  // Customization
  char petName[20] = {};     // user-given name (empty = show stage name)
  uint8_t petType = 0;       // PetType index — cosmetic species label

  uint8_t hunger = 80;       // 0-100 (0=starving, 100=full)
  uint8_t happiness = 80;    // 0-100
  uint8_t health = 100;      // 0-100 (decreases when hunger=0)
  uint32_t birthTime = 0;    // unix timestamp of egg creation
  uint32_t lastTickTime = 0; // unix timestamp of last stat update
  uint32_t totalPagesRead = 0;
  uint16_t currentStreak = 0;   // consecutive days with reading
  uint8_t daysAtStage = 0;      // days in current evolution stage
  uint16_t lastReadDay = 0;     // day-of-year of last reading (for streak)
  uint16_t pageAccumulator = 0; // pages since last feed (batched every 20)

  // Daily mission progress — reset each new day
  uint16_t missionDay = 0;       // day-of-year when missions last reset
  uint8_t  missionPagesRead = 0; // pages read today (for Read 20 Pages mission)
  uint8_t  missionPetCount = 0;  // times petted today (for Pet 3x mission)

  // Weight system (0-100, normal=50, overweight>80, underweight<20)
  uint8_t weight = 50;

  // Sickness
  bool isSick = false;          // true when sick (from overfeeding or neglect)
  uint8_t sicknessTimer = 0;   // hours remaining sick if untreated

  // Bathroom
  uint8_t wasteCount = 0;       // number of uncleared waste piles (0-4, max 4)
  uint8_t mealsSinceClean = 0;  // meals eaten since last bathroom event

  // Discipline (0-100, higher = better behaved)
  uint8_t discipline = 50;
  bool attentionCall = false;   // pet is calling for attention (may be real or fake need)
  bool isFakeCall = false;      // true if current call is fake (discipline test)
  PetNeed currentNeed = PetNeed::NONE; // what pet actually needs (NONE for fake calls)
  uint32_t lastCallTime = 0;    // unix timestamp when last attention call was generated

  // Sleep cycle
  bool isSleeping = false;      // derived from clock, cached for display
  uint8_t lightsOff = 0;        // 1 = user turned lights off

  // Aging & lifespan
  uint16_t totalAge = 0;        // total days alive
  uint8_t careMistakes = 0;     // lifetime care mistakes

  // Evolution quality tracking
  uint8_t avgCareScore = 50;    // rolling average care quality (0-100)
  uint8_t evolutionVariant = 0; // 0=good, 1=chubby, 2=misbehaved

  bool isAlive() const { return stage != PetStage::DEAD; }
  bool exists() const { return initialized; }
};

// Game balance constants
namespace PetConfig {
  constexpr uint8_t MAX_STAT = 100;
  constexpr uint16_t PAGES_PER_MEAL = 20;
  constexpr uint8_t HUNGER_PER_MEAL = 25;
  constexpr uint8_t HAPPINESS_PER_PET = 10;
  constexpr uint8_t HAPPINESS_PER_SNACK = 15;
  constexpr uint32_t PET_COOLDOWN_MS = 30000; // 30 seconds between petting
  constexpr uint32_t EXERCISE_COOLDOWN_MS = 3600000; // 1 hour between exercise

  // Decay rates (per hour)
  constexpr uint8_t HUNGER_DECAY_PER_HOUR = 1;
  constexpr uint8_t HAPPINESS_DECAY_PER_HOUR = 1;
  constexpr uint8_t HEALTH_DECAY_PER_HOUR = 2;    // only when hunger == 0

  // Weight system
  constexpr uint8_t WEIGHT_PER_MEAL = 5;
  constexpr uint8_t WEIGHT_PER_SNACK = 3;
  constexpr uint8_t WEIGHT_PER_EXERCISE = 8;
  constexpr uint8_t OVERWEIGHT_THRESHOLD = 80;
  constexpr uint8_t UNDERWEIGHT_THRESHOLD = 20;
  constexpr uint8_t NORMAL_WEIGHT = 50;

  // Sickness
  constexpr uint8_t SICK_RECOVERY_HOURS = 24;
  constexpr uint8_t SICK_HAPPINESS_PENALTY = 2; // per hour while sick

  // Bathroom
  constexpr uint8_t MEALS_UNTIL_WASTE = 3;
  constexpr uint8_t MAX_WASTE = 4;
  constexpr uint8_t WASTE_HAPPINESS_PENALTY = 1; // per waste pile per hour

  // Discipline
  constexpr uint8_t DISCIPLINE_PER_SCOLD = 10;
  constexpr uint8_t DISCIPLINE_PER_IGNORE_FAKE = 5;
  constexpr uint8_t DISCIPLINE_PENALTY_INDULGE = 5;
  constexpr uint8_t FAKE_CALL_CHANCE_PERCENT = 30;
  constexpr uint32_t ATTENTION_CALL_INTERVAL_SEC = 14400; // ~4 hours
  constexpr uint32_t ATTENTION_CALL_EXPIRE_SEC = 7200;   // 2 hours to respond

  // Sleep cycle
  constexpr uint8_t SLEEP_HOUR = 22;  // 10 PM
  constexpr uint8_t WAKE_HOUR = 7;    // 7 AM

  // Aging & care mistakes
  constexpr uint16_t ELDER_LIFESPAN_DAYS = 20;
  constexpr uint8_t CARE_MISTAKE_LIFESPAN_PENALTY = 1;
  constexpr uint8_t CARE_MISTAKE_HUNGRY_HOURS = 6;
  constexpr uint8_t CARE_MISTAKE_SICK_HOURS = 12;
  constexpr uint8_t CARE_MISTAKE_DIRTY_HOURS = 4;

  // Evolution thresholds: {minDays, minTotalPages, minAvgHunger}
  struct EvolutionReq {
    uint8_t minDays;
    uint16_t minPages;
    uint8_t minAvgHunger;
  };

  // Stage transitions: EGG→HATCH, HATCH→YOUNG, YOUNG→COMP, COMP→ELDER
  constexpr EvolutionReq EVOLUTION[] = {
    {1,   20,   0},  // Egg → Hatchling
    {3,  100,  40},  // Hatchling → Youngster
    {7,  500,  50},  // Youngster → Companion
    {14, 1500, 60},  // Companion → Elder
  };

  constexpr const char* STATE_PATH = "/.crosspoint/pet/state.json";
  constexpr const char* PET_DIR = "/.crosspoint/pet";
}
