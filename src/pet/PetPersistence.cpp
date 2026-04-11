#include "PetManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <atomic>
#include <cstring>
#include <ctime>

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

  // Core fields
  state.stage           = static_cast<PetStage>(doc["stage"] | 0);
  state.hunger          = doc["hunger"]          | (uint8_t)80;
  state.happiness       = doc["happiness"]       | (uint8_t)80;
  state.health          = doc["health"]          | (uint8_t)100;
  state.birthTime       = doc["birthTime"]       | (uint32_t)0;
  state.lastTickTime    = doc["lastTickTime"]    | (uint32_t)0;
  state.totalPagesRead  = doc["totalPagesRead"]  | (uint32_t)0;
  state.currentStreak   = doc["currentStreak"]   | (uint16_t)0;
  state.daysAtStage     = doc["daysAtStage"]     | (uint8_t)0;
  state.lastReadDay     = doc["lastReadDay"]      | (uint16_t)0;
  state.pageAccumulator = doc["pageAccumulator"] | (uint16_t)0;
  // Customization fields — backward compat: missing = empty name, default type
  const char* petNameStr = doc["petName"] | "";
  strncpy(state.petName, petNameStr, sizeof(state.petName) - 1);
  state.petName[sizeof(state.petName) - 1] = '\0';
  state.petType = doc["petType"] | (uint8_t)0;

  // Backward compat: infer initialized from birthTime if field missing
  state.initialized     = doc["initialized"]     | (state.birthTime > 0);
  state.missionDay      = doc["missionDay"]      | (uint16_t)0;
  state.missionPagesRead = doc["missionPagesRead"] | (uint8_t)0;
  state.missionPetCount = doc["missionPetCount"] | (uint8_t)0;

  // New fields — backward-compat: missing keys use struct defaults
  state.weight           = doc["weight"]           | (uint8_t)50;
  state.isSick           = doc["isSick"]           | false;
  state.sicknessTimer    = doc["sicknessTimer"]    | (uint8_t)0;
  state.wasteCount       = doc["wasteCount"]       | (uint8_t)0;
  state.mealsSinceClean  = doc["mealsSinceClean"]  | (uint8_t)0;
  state.discipline       = doc["discipline"]       | (uint8_t)50;
  state.attentionCall    = doc["attentionCall"]    | false;
  state.isFakeCall       = doc["isFakeCall"]       | false;
  state.currentNeed      = static_cast<PetNeed>(doc["currentNeed"] | (uint8_t)0);
  state.lastCallTime     = doc["lastCallTime"]     | (uint32_t)0;
  state.isSleeping       = doc["isSleeping"]       | false;
  state.lightsOff        = doc["lightsOff"]        | (uint8_t)0;
  state.totalAge         = doc["totalAge"]         | (uint16_t)0;
  state.careMistakes     = doc["careMistakes"]     | (uint8_t)0;
  state.avgCareScore     = doc["avgCareScore"]     | (uint8_t)50;
  state.evolutionVariant = doc["evolutionVariant"] | (uint8_t)0;
  state.booksFinished    = doc["booksFinished"]    | (uint8_t)0;
  state.streakTier       = doc["streakTier"]       | (uint8_t)0;

  // Restore clock if RTC lost time (power cycle) but SD card has a saved timestamp
  uint32_t savedTime = doc["savedTime"] | (uint32_t)0;
  if (savedTime > 0) {
    time_t now = time(nullptr);
    struct tm check;
    gmtime_r(&now, &check);
    if (check.tm_year < 125) {
      struct timeval tv = {static_cast<time_t>(savedTime), 0};
      settimeofday(&tv, nullptr);
      LOG_DBG("PET", "Restored clock from SD: %lu", (unsigned long)savedTime);
    }
  }

  loaded = true;
  LOG_DBG("PET", "Loaded pet: stage=%d hunger=%d happy=%d health=%d",
          (int)state.stage, state.hunger, state.happiness, state.health);
  return true;
}

bool PetManager::save() {
  Storage.mkdir(PetConfig::PET_DIR);

  JsonDocument doc;
  // Customization
  doc["petName"]        = state.petName;
  doc["petType"]        = state.petType;
  // Core fields
  doc["initialized"]    = state.initialized;
  doc["stage"]          = static_cast<uint8_t>(state.stage);
  doc["hunger"]         = state.hunger;
  doc["happiness"]      = state.happiness;
  doc["health"]         = state.health;
  doc["birthTime"]      = state.birthTime;
  doc["lastTickTime"]   = state.lastTickTime;
  doc["totalPagesRead"] = state.totalPagesRead;
  doc["currentStreak"]  = state.currentStreak;
  doc["daysAtStage"]    = state.daysAtStage;
  doc["lastReadDay"]    = state.lastReadDay;
  doc["pageAccumulator"] = state.pageAccumulator;
  doc["missionDay"]     = state.missionDay;
  doc["missionPagesRead"] = state.missionPagesRead;
  doc["missionPetCount"]  = state.missionPetCount;

  // New fields
  doc["weight"]           = state.weight;
  doc["isSick"]           = state.isSick;
  doc["sicknessTimer"]    = state.sicknessTimer;
  doc["wasteCount"]       = state.wasteCount;
  doc["mealsSinceClean"]  = state.mealsSinceClean;
  doc["discipline"]       = state.discipline;
  doc["attentionCall"]    = state.attentionCall;
  doc["isFakeCall"]       = state.isFakeCall;
  doc["currentNeed"]      = static_cast<uint8_t>(state.currentNeed);
  doc["lastCallTime"]     = state.lastCallTime;
  doc["isSleeping"]       = state.isSleeping;
  doc["lightsOff"]        = state.lightsOff;
  doc["totalAge"]         = state.totalAge;
  doc["careMistakes"]     = state.careMistakes;
  doc["avgCareScore"]     = state.avgCareScore;
  doc["evolutionVariant"] = state.evolutionVariant;
  doc["booksFinished"]    = state.booksFinished;
  doc["streakTier"]       = state.streakTier;

  // Persist current timestamp for clock restoration after power cycle
  doc["savedTime"] = (uint32_t)time(nullptr);

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

// --- Async save (background FreeRTOS task) ---

static std::atomic<bool> _asyncSaveDone{true};

static void asyncSaveTask(void* param) {
  auto* mgr = static_cast<PetManager*>(param);
  mgr->save();
  _asyncSaveDone = true;
  vTaskDelete(nullptr);
}

void PetManager::saveAsync() {
  if (!state.exists()) return;
  if (!_asyncSaveDone) return;  // Previous save still running
  _asyncSaveDone = false;
  xTaskCreate(asyncSaveTask, "petSave", 4096, this, 1, nullptr);
}

bool PetManager::isSaveDone() const { return _asyncSaveDone; }

void PetManager::waitForSave() {
  while (!_asyncSaveDone) { vTaskDelay(pdMS_TO_TICKS(10)); }
}
