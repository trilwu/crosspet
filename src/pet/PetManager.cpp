#include "PetManager.h"

#include "PetCareTracker.h"
#include "PetDecayEngine.h"
#include "PetEvolution.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

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

// --- Feeding (page-driven) ---

void PetManager::onPageTurn() {
  if (!state.exists() || !state.isAlive()) return;

  state.pageAccumulator++;
  state.totalPagesRead++;

  uint16_t today = getDayOfYear();
  resetMissionsIfNewDay(state, today);
  bool missionProgress = false;
  if (state.missionPagesRead < 20) {
    state.missionPagesRead++;
    missionProgress = true;
  }

  if (today > 0 && today != state.lastReadDay) {
    state.lastReadDay = today;
    state.currentStreak++;
    uint8_t streakBonus = (state.currentStreak > 5) ? 5 : state.currentStreak;
    state.happiness = clampAdd(state.happiness, streakBonus);

    // Recalculate streak tier after streak update
    uint8_t oldTier = state.streakTier;
    if (state.currentStreak >= 30)      state.streakTier = 3;
    else if (state.currentStreak >= 14) state.streakTier = 2;
    else if (state.currentStreak >= 7)  state.streakTier = 1;
    else                                state.streakTier = 0;

    // Streak tier milestone (higher priority than page milestone)
    if (state.streakTier > oldTier) {
      pendingMilestone = Milestone::STREAK_UP;
      lastMilestoneValue = state.currentStreak;
    }
  }

  // Daily reading goal reward (fires once on the exact page that reaches the goal)
  if (missionProgress && state.missionPagesRead == PetConfig::DAILY_GOAL_PAGES) {
    state.health = clampAdd(state.health, PetConfig::DAILY_GOAL_HEALTH);
    state.happiness = clampAdd(state.happiness, PetConfig::DAILY_GOAL_HAPPINESS);
    if (pendingMilestone == Milestone::NONE) {  // don't overwrite higher-priority streak milestone
      pendingMilestone = Milestone::DAILY_GOAL;
      lastMilestoneValue = PetConfig::DAILY_GOAL_PAGES;
    }
    LOG_DBG("PET", "Daily reading goal met! health=%d happiness=%d", state.health, state.happiness);
  }

  // Page milestone every 100 pages (only if no higher-priority milestone pending)
  if (pendingMilestone == Milestone::NONE && state.totalPagesRead > 0 && state.totalPagesRead % 100 == 0) {
    pendingMilestone = Milestone::PAGE_MILESTONE;
    lastMilestoneValue = static_cast<uint16_t>(state.totalPagesRead > 65535 ? 65535 : state.totalPagesRead);
  }

  if (state.pageAccumulator >= getEffectivePagesPerMeal()) {
    state.hunger = clampAdd(state.hunger, PetConfig::HUNGER_PER_MEAL);
    state.pageAccumulator -= getEffectivePagesPerMeal();
    if (state.health < PetConfig::MAX_STAT && state.hunger > 0)
      state.health = clampAdd(state.health, 5);

    // Weight and waste tracking per meal
    state.weight = clampAdd(state.weight, PetConfig::WEIGHT_PER_MEAL);
    state.mealsSinceClean++;
    if (state.mealsSinceClean >= PetConfig::MEALS_UNTIL_WASTE) {
      state.mealsSinceClean = 0;
      if (state.wasteCount < PetConfig::MAX_WASTE) state.wasteCount++;
    }

    LOG_DBG("PET", "Fed! hunger=%d weight=%d waste=%d", state.hunger, state.weight, state.wasteCount);
    save();
  } else if (missionProgress) {
    save();
  }
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

void PetManager::onChapterComplete() {
  if (!state.exists() || !state.isAlive()) return;
  state.happiness = clampAdd(state.happiness, PetConfig::CHAPTER_COMPLETE_HAPPINESS);
  LOG_DBG("PET", "Chapter complete! happiness=%d", state.happiness);
  save();
}

void PetManager::onBookFinished() {
  if (!state.exists() || !state.isAlive()) return;
  state.happiness = clampAdd(state.happiness, PetConfig::BOOK_FINISH_HAPPINESS);
  state.hunger = clampAdd(state.hunger, PetConfig::BOOK_FINISH_HUNGER);
  if (state.booksFinished < 255) state.booksFinished++;
  LOG_DBG("PET", "Book finished! happiness=%d hunger=%d booksFinished=%d",
          state.happiness, state.hunger, state.booksFinished);
  save();
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

// --- Milestones ---

PetManager::Milestone PetManager::consumePendingMilestone() {
  Milestone m = pendingMilestone;
  pendingMilestone = Milestone::NONE;
  return m;
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
