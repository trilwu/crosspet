#include "PetManager.h"

#include "PetCareTracker.h"
#include "PetDecayEngine.h"
#include "PetEvolution.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

#include "ReadingStats.h"

#include <cstring>
#include <ctime>

PetManager& PetManager::getInstance() {
  static PetManager instance;
  return instance;
}

// --- Game Logic ---

void PetManager::tick() {
  if (!state.exists() || !state.isAlive()) return;
  if (!isTimeValid()) return;

  uint32_t now = getCurrentTime();
  if (state.lastTickTime == 0) {
    state.lastTickTime = now;
    save();
    return;
  }
  if (now <= state.lastTickTime) return;

  uint32_t elapsedSec = now - state.lastTickTime;
  uint32_t elapsedHours = elapsedSec / 3600;
  if (elapsedHours == 0) return;

  // Determine the hour-of-day at the start of the elapsed period for sleep simulation
  struct tm startTm;
  time_t startTime = static_cast<time_t>(state.lastTickTime);
  localtime_r(&startTime, &startTm);
  uint8_t startHour = static_cast<uint8_t>(startTm.tm_hour);

  PetDecayEngine::applyDecay(state, elapsedHours, startHour);
  PetCareTracker::checkCareMistakes(state, elapsedHours);
  PetCareTracker::expireAttentionCall(state, now);
  PetCareTracker::generateAttentionCall(state, now);

  state.lastTickTime = now;

  uint8_t elapsedDays = static_cast<uint8_t>(elapsedHours / 24);
  if (elapsedDays > 0) {
    state.daysAtStage += elapsedDays;
    state.totalAge    += elapsedDays;
    PetCareTracker::updateCareScore(state);
    PetEvolution::checkEvolution(state);
    updateStreak();

    // Elder lifespan death check
    if (state.stage == PetStage::ELDER && state.isAlive()) {
      uint16_t lifespan = (state.careMistakes < PetConfig::ELDER_LIFESPAN_DAYS)
                              ? PetConfig::ELDER_LIFESPAN_DAYS - state.careMistakes
                              : 1;
      if (state.totalAge >= lifespan) {
        state.stage = PetStage::DEAD;
        LOG_DBG("PET", "Pet died of old age");
      }
    }
  }

  save();
}


void PetManager::updateStreak() {
  uint16_t today = getDayOfYear();
  if (today == 0) return;
  if (state.lastReadDay > 0) {
    uint16_t diff = (today >= state.lastReadDay) ? (today - state.lastReadDay) : 1;
    if (diff > 1) state.currentStreak = 0;
  }
}

static void resetMissionsIfNewDay(PetState& state, uint16_t today) {
  if (today > 0 && today != state.missionDay) {
    state.missionDay = today;
    state.missionPagesRead = 0;
    state.missionPetCount = 0;
  }
}

// --- Sync from ReadingStats (lazy evaluation) ---

void PetManager::syncFromReadingStats() {
  if (!state.exists() || !state.isAlive()) return;

  auto& stats = ReadingStats::getInstance();

  // First-sync migration guard: don't retroactively apply all historical reading
  if (state.lastKnownReadSeconds == 0 && state.lastTickTime > 0) {
    state.lastKnownReadSeconds = stats.totalReadSeconds;
    state.lastUpdateTimestamp = getCurrentTime();
    LOG_DBG("PET", "Migration: initialized lastKnownReadSeconds=%lu", (unsigned long)stats.totalReadSeconds);
    return;
  }

  // Sync streak and books from ReadingStats
  state.currentStreak = stats.currentStreak;
  state.lastReadDay = getDayOfYear();  // keep in sync to prevent tick() from resetting streak
  state.booksFinished = (stats.booksFinished > 255) ? 255 : static_cast<uint8_t>(stats.booksFinished);

  // Derive totalPagesRead from reading time (~90 sec/page avg) for evolution thresholds
  state.totalPagesRead = stats.totalReadSeconds / 90;

  // Calculate meals from reading time diff (10 min reading = 1 meal)
  uint32_t readSecsDiff = 0;
  if (stats.totalReadSeconds > state.lastKnownReadSeconds) {
    readSecsDiff = stats.totalReadSeconds - state.lastKnownReadSeconds;
  }
  uint32_t meals = readSecsDiff / 600;
  if (meals > 50) meals = 50;  // cap to prevent stat overflow

  for (uint32_t i = 0; i < meals; i++) {
    state.hunger = clampAdd(state.hunger, PetConfig::HUNGER_PER_MEAL);
    state.weight = clampAdd(state.weight, PetConfig::WEIGHT_PER_MEAL);
    state.mealsSinceClean++;
    if (state.mealsSinceClean >= PetConfig::MEALS_UNTIL_WASTE) {
      state.mealsSinceClean = 0;
      if (state.wasteCount < PetConfig::MAX_WASTE) state.wasteCount++;
    }
  }
  if (meals > 0) {
    if (state.health < PetConfig::MAX_STAT && state.hunger > 0)
      state.health = clampAdd(state.health, 5);
    // Happiness from reading activity (replaces chapter/book/streak bonuses)
    uint8_t happinessBonus = static_cast<uint8_t>(meals * 3 > 30 ? 30 : meals * 3);
    state.happiness = clampAdd(state.happiness, happinessBonus);
  }

  // Approximate today's reading for daily mission (90 sec/page)
  state.missionPagesRead = static_cast<uint8_t>(
      stats.todayReadSeconds / 90 > 20 ? 20 : stats.todayReadSeconds / 90);

  // Update streak tier
  if (state.currentStreak >= 30)      state.streakTier = 3;
  else if (state.currentStreak >= 14) state.streakTier = 2;
  else if (state.currentStreak >= 7)  state.streakTier = 1;
  else                                state.streakTier = 0;

  state.lastKnownReadSeconds = stats.totalReadSeconds;
  state.lastUpdateTimestamp = getCurrentTime();
  LOG_DBG("PET", "Synced: meals=%lu pages=%lu streak=%d happy=%d",
          (unsigned long)meals, (unsigned long)state.totalPagesRead,
          state.currentStreak, state.happiness);
}

// --- User interaction ---

bool PetManager::pet() {
  if (!state.exists() || !state.isAlive()) return false;

  unsigned long now = millis();
  if (now - lastPetTimeMs < PetConfig::PET_COOLDOWN_MS) return false;

  lastPetTimeMs = now;
  state.happiness = clampAdd(state.happiness, PetConfig::HAPPINESS_PER_PET);
  resetMissionsIfNewDay(state, getDayOfYear());
  if (state.missionPetCount < 3) state.missionPetCount++;
  LOG_DBG("PET", "Petted! happiness=%d", state.happiness);
  save();
  return true;
}

void PetManager::hatchNew(const char* name, uint8_t type) {
  state = PetState();  // Resets all fields to struct defaults
  state.initialized = true;
  state.stage = PetStage::EGG;
  state.hunger = 80;
  state.happiness = 80;
  state.health = 100;
  state.petType = type;
  if (name && name[0]) {
    strncpy(state.petName, name, sizeof(state.petName) - 1);
    state.petName[sizeof(state.petName) - 1] = '\0';
  }
  if (isTimeValid()) {
    state.birthTime = getCurrentTime();
    state.lastTickTime = state.birthTime;
  }
  save();
  LOG_DBG("PET", "New egg hatched! name='%s' type=%d", state.petName, state.petType);
}

bool PetManager::renamePet(const char* name) {
  if (!state.exists()) return false;
  if (name && name[0]) {
    strncpy(state.petName, name, sizeof(state.petName) - 1);
    state.petName[sizeof(state.petName) - 1] = '\0';
  } else {
    state.petName[0] = '\0';
  }
  return save();
}

bool PetManager::changeType(uint8_t type) {
  if (!state.exists()) return false;
  state.petType = type;
  return save();
}

// --- Reading rewards ---

uint16_t PetManager::getEffectivePagesPerMeal() const {
  uint8_t tier = (state.streakTier < 4) ? state.streakTier : 3;
  return PetConfig::STREAK_PAGES_PER_MEAL[tier];
}

// --- State queries ---

PetMood PetManager::getMood() const {
  if (!state.isAlive())  return PetMood::DEAD;
  if (state.isSleeping)  return PetMood::SLEEPING;
  if (state.isSick)      return PetMood::SICK;
  if (state.attentionCall) return PetMood::NEEDY;
  // Low discipline → occasional refusal behaviour
  if (state.discipline < 30 && random(100) < 20) return PetMood::REFUSING;
  if (state.hunger > 70 && state.health > 70) return PetMood::HAPPY;
  if (state.hunger > 30 && state.health > 30) return PetMood::NEUTRAL;
  return PetMood::SAD;
}

uint32_t PetManager::getDaysAlive() const {
  if (!state.exists() || state.birthTime == 0 || !isTimeValid()) return 0;
  uint32_t now = getCurrentTime();
  if (now <= state.birthTime) return 0;
  return (now - state.birthTime) / 86400;
}

void PetManager::getMissions(PetMission out[3]) const {
  out[0] = {tr(STR_PET_MISSION_READ), state.missionPagesRead, 20, state.missionPagesRead >= 20};
  out[1] = {tr(STR_PET_MISSION_PET),  state.missionPetCount,   3, state.missionPetCount  >=  3};
  out[2] = {tr(STR_PET_MISSION_FED),  state.hunger,            40, state.hunger           >= 40};
}

// --- Helpers ---

bool PetManager::isTimeValid() const {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return false;
  return timeinfo.tm_year >= 125;
}

uint32_t PetManager::getCurrentTime() const {
  return static_cast<uint32_t>(time(nullptr));
}

uint16_t PetManager::getDayOfYear() const {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return 0;
  if (timeinfo.tm_year < 125) return 0;
  return static_cast<uint16_t>(timeinfo.tm_yday + 1);
}

uint8_t PetManager::clampSub(uint8_t val, uint8_t amount) {
  return (val > amount) ? (val - amount) : 0;
}

uint8_t PetManager::clampAdd(uint8_t val, uint8_t amount) {
  uint16_t result = static_cast<uint16_t>(val) + amount;
  return (result > PetConfig::MAX_STAT) ? PetConfig::MAX_STAT : static_cast<uint8_t>(result);
}
