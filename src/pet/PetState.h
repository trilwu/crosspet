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
  SAD = 2,      // hunger > 0 && health > 0
  SICK = 3,     // hunger == 0 (health draining)
  SLEEPING = 4, // shown on sleep screen
  DEAD = 5
};

// Persistent pet state — serialized to JSON on SD card
struct PetState {
  bool initialized = false;  // true after hatchNew(); prevents false-positive exists() with no clock
  PetStage stage = PetStage::EGG;
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
  uint16_t missionDay = 0;      // day-of-year when missions last reset
  uint8_t  missionPagesRead = 0; // pages read today (for Read 20 Pages mission)
  uint8_t  missionPetCount = 0;  // times petted today (for Pet 3x mission)

  bool isAlive() const { return stage != PetStage::DEAD; }
  bool exists() const { return initialized; }
};

// Game balance constants
namespace PetConfig {
  constexpr uint8_t MAX_STAT = 100;
  constexpr uint16_t PAGES_PER_MEAL = 20;
  constexpr uint8_t HUNGER_PER_MEAL = 25;
  constexpr uint8_t HAPPINESS_PER_PET = 10;
  constexpr uint32_t PET_COOLDOWN_MS = 300000; // 5 minutes between petting

  // Decay rates (per hour)
  constexpr uint8_t HUNGER_DECAY_PER_HOUR = 1;
  constexpr uint8_t HAPPINESS_DECAY_PER_HOUR = 1; // effectively per 2h due to rounding
  constexpr uint8_t HEALTH_DECAY_PER_HOUR = 2;    // only when hunger == 0

  // Evolution thresholds: {minDays, minTotalPages}
  struct EvolutionReq {
    uint8_t minDays;
    uint16_t minPages;
    uint8_t minAvgHunger; // average hunger must be above this
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
