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
  doc["homeShowClock"] = homeShowClock;
  doc["homeShowWeather"] = homeShowWeather;
  doc["homeShowPetStatus"] = homeShowPetStatus;
  doc["homeFocusMode"] = homeFocusMode;

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
      homeShowClock = doc["homeShowClock"] | (uint8_t)1;
      homeShowWeather = doc["homeShowWeather"] | (uint8_t)1;
      homeShowPetStatus = doc["homeShowPetStatus"] | (uint8_t)1;
      homeFocusMode = doc["homeFocusMode"] | (uint8_t)0;
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
        homeShowClock = doc["homeShowClock"] | (uint8_t)1;
        homeShowWeather = doc["homeShowWeather"] | (uint8_t)1;
        homeShowPetStatus = doc["homeShowPetStatus"] | (uint8_t)1;
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
