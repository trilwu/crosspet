#include "CrossPetSettings.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

// Initialize static singleton instance
CrossPetSettings CrossPetSettings::instance;

CrossPetSettings& CrossPetSettings::getInstance() {
  return instance;
}

bool CrossPetSettings::saveToFile() const {
  Storage.mkdir("/.crosspoint");

  JsonDocument doc;
  doc["homeFocusMode"] = homeFocusMode;

  doc["appClock"] = appClock;
  doc["appWeather"] = appWeather;
  doc["appPomodoro"] = appPomodoro;
  doc["appVirtualPet"] = appVirtualPet;
  doc["appReadingStats"] = appReadingStats;
  doc["appSleepImagePicker"] = appSleepImagePicker;
  doc["appGames"] = appGames;

  String json;
  serializeJson(doc, json);
  bool ok = Storage.writeFile(SETTINGS_PATH, json);
  if (ok) {
    LOG_DBG("CPS", "CrossPet settings saved");
  } else {
    LOG_ERR("CPS", "Failed to save CrossPet settings");
  }
  return ok;
}

bool CrossPetSettings::loadFromFile() {
  // Primary: load from crosspet.json
  if (Storage.exists(SETTINGS_PATH)) {
    String json = Storage.readFile(SETTINGS_PATH);
    if (!json.isEmpty()) {
      JsonDocument doc;
      auto error = deserializeJson(doc, json);
      if (error) {
        LOG_ERR("CPS", "CrossPet settings JSON parse error: %s", error.c_str());
        return false;
      }
      homeFocusMode = doc["homeFocusMode"] | (uint8_t)0;

      // Migrate legacy homeShow* fields to app* toggles (if appClock not yet saved)
      appClock = doc["appClock"] | doc["homeShowClock"] | (uint8_t)1;
      appWeather = doc["appWeather"] | doc["homeShowWeather"] | (uint8_t)1;
      appPomodoro = doc["appPomodoro"] | (uint8_t)1;
      appVirtualPet = doc["appVirtualPet"] | (uint8_t)1;
      appReadingStats = doc["appReadingStats"] | (uint8_t)1;
      appSleepImagePicker = doc["appSleepImagePicker"] | (uint8_t)1;
      // Migrate: if any legacy per-game toggle was off, set master toggle off
      if (doc.containsKey("appGames")) {
        appGames = doc["appGames"] | (uint8_t)1;
      } else {
        // Legacy migration: all games were on by default, keep on unless explicitly saved off
        appGames = 1;
      }
      LOG_DBG("CPS", "CrossPet settings loaded from file");
      return true;
    }
  }

  // Migration: if crosspet.json absent, try to read values from settings.json
  constexpr char LEGACY_PATH[] = "/.crosspoint/settings.json";
  if (Storage.exists(LEGACY_PATH)) {
    String json = Storage.readFile(LEGACY_PATH);
    if (!json.isEmpty()) {
      JsonDocument doc;
      auto error = deserializeJson(doc, json);
      if (!error) {
        // Migrate legacy homeShow* to app* toggles
        appClock = doc["homeShowClock"] | (uint8_t)1;
        appWeather = doc["homeShowWeather"] | (uint8_t)1;
        LOG_DBG("CPS", "CrossPet settings migrated from settings.json");
        // Persist the migrated values to crosspet.json
        saveToFile();
        return true;
      }
    }
  }

  // No existing settings — defaults are already set by struct initializers
  LOG_DBG("CPS", "CrossPet settings: no file found, using defaults");
  return false;
}
