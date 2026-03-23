#pragma once
#include <cstdint>

// CrossPet-specific settings stored separately from CrossPointSettings.
// This allows CrossPet firmware to have its own settings file without
// conflicting with CrossPoint reader settings when users switch firmware.
class CrossPetSettings {
 public:
  // Prevent copy/assign (singleton)
  CrossPetSettings(const CrossPetSettings&) = delete;
  CrossPetSettings& operator=(const CrossPetSettings&) = delete;

  static CrossPetSettings& getInstance();

  // Persist to /.crosspoint/crosspet.json
  bool saveToFile() const;
  // Load from /.crosspoint/crosspet.json; migrates from settings.json if absent
  bool loadFromFile();

  // File browser sort mode: 0=name asc, 1=name desc, 2=size, 3=date
  uint8_t fileSortMode = 0;

  // Home screen widget visibility (1=show, 0=hide)
  uint8_t homeShowClock = 1;
  uint8_t homeShowWeather = 1;
  uint8_t homeShowPetStatus = 1;
  uint8_t homeFocusMode = 0;  // 1=show only current book (no recent covers/stats)

 private:
  CrossPetSettings() = default;
  static CrossPetSettings instance;

  static constexpr char SETTINGS_PATH[] = "/.crosspoint/crosspet.json";
};

// Helper macro to access CrossPet settings
#define PET_SETTINGS CrossPetSettings::getInstance()
