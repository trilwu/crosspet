#pragma once

#include "PetState.h"

// A single daily mission shown in the pet UI
struct PetMission {
  const char* label;
  uint8_t     progress;
  uint8_t     goal;
  bool        done;
};

// Manages virtual pet game logic, stat decay, evolution, and persistence.
// Implementation split across PetManager.cpp, PetPersistence.cpp, PetActions.cpp.
// Singleton accessed via PET_MANAGER macro.
class PetManager {
 public:
  PetManager(const PetManager&) = delete;
  PetManager& operator=(const PetManager&) = delete;

  static PetManager& getInstance();

  // Persistence (PetPersistence.cpp)
  bool load();
  bool save();

  // Game logic — call tick() on boot and periodically to apply time-based decay
  void tick();

  // Feeding — call when pages are read (batched: every 20 pages = 1 meal)
  void onPageTurn();

  // User interaction — petting gives happiness (with cooldown)
  bool pet();

  // Start a new pet from egg (optionally with custom name and type)
  void hatchNew(const char* name = nullptr, uint8_t type = 0);

  // Rename/retype an existing pet
  bool renamePet(const char* name);
  bool changeType(uint8_t type);

  // --- User actions (PetActions.cpp) ---
  bool feedMeal();       // fill hunger + add weight + waste tracking
  bool feedSnack();      // add happiness + add weight (no hunger)
  bool giveMedicine();   // cure sickness
  bool exercise();       // reduce weight + add happiness (1h cooldown)
  bool cleanBathroom();  // clear all waste piles
  bool disciplinePet();  // scold during attention call
  bool ignoreCry();      // ignore attention call (good if fake, bad if real)
  bool toggleLights();   // toggle sleep lights-off flag

  // State queries
  const PetState& getState() const { return state; }
  PetMood getMood() const;
  bool isAlive() const { return state.isAlive(); }
  bool exists() const { return state.exists(); }
  uint32_t getDaysAlive() const;
  const char* getLastFeedback() const { return lastFeedback; }

  // Daily missions — returns 3 missions for today
  void getMissions(PetMission out[3]) const;

 private:
  PetManager() = default;

  PetState state;
  unsigned long lastPetTimeMs = 0;      // millis() of last petting (cooldown)
  unsigned long lastExerciseMs = 0;     // millis() of last exercise (cooldown)
  bool loaded = false;
  const char* lastFeedback = nullptr;   // feedback string for UI display

  // Internal helpers
  void updateStreak();
  bool isTimeValid() const;
  uint32_t getCurrentTime() const;
  uint16_t getDayOfYear() const;

  static uint8_t clampSub(uint8_t val, uint8_t amount);
  static uint8_t clampAdd(uint8_t val, uint8_t amount);

  friend class PetPersistence;  // allow PetPersistence.cpp to access privates via method calls
};

#define PET_MANAGER PetManager::getInstance()
