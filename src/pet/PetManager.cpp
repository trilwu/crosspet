#include "PetManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <ctime>

PetManager& PetManager::getInstance() {
  static PetManager instance;
  return instance;
}

// --- Persistence ---

bool PetManager::load() {
  if (!Storage.exists(PetConfig::STATE_PATH)) {
    LOG_DBG("PET", "No pet state file found");
    loaded = true;
    return true;  // No pet yet — not an error
  }

  String json = Storage.readFile(PetConfig::STATE_PATH);
  if (json.isEmpty()) {
    LOG_ERR("PET", "Failed to read pet state");
    return false;
  }

  JsonDocument doc;
  auto err = deserializeJson(doc, json);
  if (err) {
    LOG_ERR("PET", "JSON parse error: %s", err.c_str());
    return false;
  }

  state.stage = static_cast<PetStage>(doc["stage"] | 0);
  state.hunger = doc["hunger"] | (uint8_t)80;
  state.happiness = doc["happiness"] | (uint8_t)80;
  state.health = doc["health"] | (uint8_t)100;
  state.birthTime = doc["birthTime"] | (uint32_t)0;
  state.lastTickTime = doc["lastTickTime"] | (uint32_t)0;
  state.totalPagesRead = doc["totalPagesRead"] | (uint32_t)0;
  state.currentStreak = doc["currentStreak"] | (uint16_t)0;
  state.daysAtStage = doc["daysAtStage"] | (uint8_t)0;
  state.lastReadDay = doc["lastReadDay"] | (uint16_t)0;
  state.pageAccumulator = doc["pageAccumulator"] | (uint16_t)0;
  // Backwards compat: old saves without "initialized" infer from birthTime
  state.initialized = doc["initialized"] | (state.birthTime > 0);
  state.missionDay       = doc["missionDay"]       | (uint16_t)0;
  state.missionPagesRead = doc["missionPagesRead"] | (uint8_t)0;
  state.missionPetCount  = doc["missionPetCount"]  | (uint8_t)0;

  loaded = true;
  LOG_DBG("PET", "Loaded pet: stage=%d hunger=%d happy=%d health=%d",
          (int)state.stage, state.hunger, state.happiness, state.health);
  return true;
}

bool PetManager::save() {
  Storage.mkdir(PetConfig::PET_DIR);

  JsonDocument doc;
  doc["initialized"] = state.initialized;
  doc["stage"] = static_cast<uint8_t>(state.stage);
  doc["hunger"] = state.hunger;
  doc["happiness"] = state.happiness;
  doc["health"] = state.health;
  doc["birthTime"] = state.birthTime;
  doc["lastTickTime"] = state.lastTickTime;
  doc["totalPagesRead"] = state.totalPagesRead;
  doc["currentStreak"] = state.currentStreak;
  doc["daysAtStage"] = state.daysAtStage;
  doc["lastReadDay"] = state.lastReadDay;
  doc["pageAccumulator"] = state.pageAccumulator;
  doc["missionDay"]       = state.missionDay;
  doc["missionPagesRead"] = state.missionPagesRead;
  doc["missionPetCount"]  = state.missionPetCount;

  String json;
  serializeJson(doc, json);
  bool ok = Storage.writeFile(PetConfig::STATE_PATH, json);
  if (ok) {
    LOG_DBG("PET", "State saved");
  } else {
    LOG_ERR("PET", "Failed to save state");
  }
  return ok;
}

// --- Game Logic ---

void PetManager::tick() {
  if (!state.exists() || !state.isAlive()) return;
  if (!isTimeValid()) return;  // Can't decay without valid time

  uint32_t now = getCurrentTime();
  if (state.lastTickTime == 0) {
    // First tick after hatch — just set the timestamp
    state.lastTickTime = now;
    save();
    return;
  }

  // Calculate elapsed hours since last tick
  if (now <= state.lastTickTime) return;  // Clock hasn't advanced
  uint32_t elapsedSec = now - state.lastTickTime;
  uint32_t elapsedHours = elapsedSec / 3600;

  if (elapsedHours > 0) {
    applyDecay(elapsedHours);
    state.lastTickTime = now;

    // Check evolution once per day (when elapsed >= 24h worth or days changed)
    uint8_t elapsedDays = elapsedHours / 24;
    if (elapsedDays > 0) {
      state.daysAtStage += elapsedDays;
      checkEvolution();
      updateStreak();
    }

    save();
  }
}

void PetManager::applyDecay(uint32_t elapsedHours) {
  // Cap to prevent overflow from very long absences (max 30 days = 720h)
  if (elapsedHours > 720) elapsedHours = 720;

  for (uint32_t h = 0; h < elapsedHours; h++) {
    // Hunger decreases every hour
    state.hunger = clampSub(state.hunger, PetConfig::HUNGER_DECAY_PER_HOUR);

    // Happiness decreases every ~2 hours (alternate hours)
    if (h % 2 == 0) {
      state.happiness = clampSub(state.happiness, PetConfig::HAPPINESS_DECAY_PER_HOUR);
    }

    // Health drains when starving
    if (state.hunger == 0) {
      state.health = clampSub(state.health, PetConfig::HEALTH_DECAY_PER_HOUR);
    }

    // Death check
    if (state.health == 0) {
      state.stage = PetStage::DEAD;
      LOG_DBG("PET", "Pet has died after %lu hours of decay", (unsigned long)elapsedHours);
      return;
    }
  }
}

void PetManager::checkEvolution() {
  uint8_t stageIdx = static_cast<uint8_t>(state.stage);
  if (stageIdx >= static_cast<uint8_t>(PetStage::ELDER)) return;  // Already max or dead

  const auto& req = PetConfig::EVOLUTION[stageIdx];
  if (state.daysAtStage >= req.minDays &&
      state.totalPagesRead >= req.minPages &&
      state.hunger >= req.minAvgHunger) {
    state.stage = static_cast<PetStage>(stageIdx + 1);
    state.daysAtStage = 0;
    LOG_DBG("PET", "Evolved to stage %d!", static_cast<int>(state.stage));
  }
}

void PetManager::updateStreak() {
  uint16_t today = getDayOfYear();
  if (today == 0) return;  // No valid time

  // If last read was yesterday, streak continues. Otherwise reset.
  if (state.lastReadDay > 0) {
    uint16_t diff = (today >= state.lastReadDay) ? (today - state.lastReadDay) : 1;
    if (diff > 1) {
      state.currentStreak = 0;
    }
  }
}

// --- Mission helpers ---

static void resetMissionsIfNewDay(PetState& state, uint16_t today) {
  if (today > 0 && today != state.missionDay) {
    state.missionDay = today;
    state.missionPagesRead = 0;
    state.missionPetCount = 0;
  }
}

// --- Feeding ---

void PetManager::onPageTurn() {
  if (!state.exists() || !state.isAlive()) return;

  state.pageAccumulator++;
  state.totalPagesRead++;

  uint16_t today = getDayOfYear();
  resetMissionsIfNewDay(state, today);
  if (state.missionPagesRead < 20) state.missionPagesRead++;

  // Update streak tracking
  if (today > 0 && today != state.lastReadDay) {
    state.lastReadDay = today;
    state.currentStreak++;
    // Streak bonus happiness
    uint8_t streakBonus = (state.currentStreak > 5) ? 5 : state.currentStreak;
    state.happiness = clampAdd(state.happiness, streakBonus);
  }

  // Batch feed every PAGES_PER_MEAL pages
  if (state.pageAccumulator >= PetConfig::PAGES_PER_MEAL) {
    state.hunger = clampAdd(state.hunger, PetConfig::HUNGER_PER_MEAL);
    state.pageAccumulator -= PetConfig::PAGES_PER_MEAL;
    LOG_DBG("PET", "Fed! hunger=%d pages=%lu", state.hunger, (unsigned long)state.totalPagesRead);

    // Health recovers when fed (if not dead)
    if (state.health < PetConfig::MAX_STAT && state.hunger > 0) {
      state.health = clampAdd(state.health, 5);
    }

    save();  // Persist on feed (every ~20 pages)
  }
}

// --- User Interaction ---

bool PetManager::pet() {
  if (!state.exists() || !state.isAlive()) return false;

  unsigned long now = millis();
  if (now - lastPetTimeMs < PetConfig::PET_COOLDOWN_MS) {
    return false;  // Cooldown active
  }

  lastPetTimeMs = now;
  state.happiness = clampAdd(state.happiness, PetConfig::HAPPINESS_PER_PET);
  resetMissionsIfNewDay(state, getDayOfYear());
  if (state.missionPetCount < 3) state.missionPetCount++;
  LOG_DBG("PET", "Petted! happiness=%d", state.happiness);
  save();
  return true;
}

void PetManager::hatchNew() {
  state = PetState();  // Reset to defaults
  state.initialized = true;
  state.stage = PetStage::EGG;
  state.hunger = 80;
  state.happiness = 80;
  state.health = 100;

  if (isTimeValid()) {
    state.birthTime = getCurrentTime();
    state.lastTickTime = state.birthTime;
  }

  save();
  LOG_DBG("PET", "New egg hatched!");
}

// --- State Queries ---

PetMood PetManager::getMood() const {
  if (!state.isAlive()) return PetMood::DEAD;
  if (state.hunger == 0) return PetMood::SICK;
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

// --- Helpers ---

bool PetManager::isTimeValid() const {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return false;
  return timeinfo.tm_year >= 125;  // Year >= 2025
}

uint32_t PetManager::getCurrentTime() const {
  return static_cast<uint32_t>(time(nullptr));
}

uint16_t PetManager::getDayOfYear() const {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return 0;
  if (timeinfo.tm_year < 125) return 0;
  return static_cast<uint16_t>(timeinfo.tm_yday + 1);  // 1-366
}

void PetManager::getMissions(PetMission out[3]) const {
  out[0] = {"Read 20 pages",    state.missionPagesRead, 20, state.missionPagesRead >= 20};
  out[1] = {"Pet 3 times",      state.missionPetCount,   3, state.missionPetCount  >=  3};
  out[2] = {"Keep fed (>40%)",  state.hunger,           40, state.hunger            >= 40};
}

uint8_t PetManager::clampSub(uint8_t val, uint8_t amount) {
  return (val > amount) ? (val - amount) : 0;
}

uint8_t PetManager::clampAdd(uint8_t val, uint8_t amount) {
  uint16_t result = static_cast<uint16_t>(val) + amount;
  return (result > PetConfig::MAX_STAT) ? PetConfig::MAX_STAT : static_cast<uint8_t>(result);
}
