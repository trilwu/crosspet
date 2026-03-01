#pragma once

#include "PetState.h"

// A single daily mission shown in the pet UI
struct PetMission {
  const char* label;   // display text
  uint8_t     progress; // current value
  uint8_t     goal;     // target value
  bool        done;
};

// Manages virtual pet game logic, stat decay, evolution, and persistence.
// Singleton accessed via PET_MANAGER macro.
class PetManager {
 public:
  PetManager(const PetManager&) = delete;
  PetManager& operator=(const PetManager&) = delete;

  static PetManager& getInstance();

  // Persistence
  bool load();
  bool save();

  // Game logic — call tick() on boot and periodically to apply time-based decay
  void tick();

  // Feeding — call when pages are read (batched: every 20 pages = 1 meal)
  void onPageTurn();

  // User interaction — petting gives happiness (with cooldown)
  bool pet();

  // Start a new pet from egg
  void hatchNew();

  // State queries
  const PetState& getState() const { return state; }
  PetMood getMood() const;
  bool isAlive() const { return state.isAlive(); }
  bool exists() const { return state.exists(); }
  uint32_t getDaysAlive() const;

  // Daily missions — returns 3 missions for today
  void getMissions(PetMission out[3]) const;

 private:
  PetManager() = default;

  PetState state;
  unsigned long lastPetTimeMs = 0; // millis() of last petting (cooldown)
  bool loaded = false;

  // Internal helpers
  void applyDecay(uint32_t elapsedHours);
  void checkEvolution();
  void updateStreak();
  bool isTimeValid() const;
  uint32_t getCurrentTime() const;
  uint16_t getDayOfYear() const;

  static uint8_t clampSub(uint8_t val, uint8_t amount);
  static uint8_t clampAdd(uint8_t val, uint8_t amount);
};

#define PET_MANAGER PetManager::getInstance()
