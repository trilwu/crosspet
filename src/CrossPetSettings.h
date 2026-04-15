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

  // Home screen layout
  uint8_t homeFocusMode = 0;  // 1=show only current book (no recent covers/stats)

  // Per-app visibility — controls both Tools menu and home screen widgets (1=show, 0=hide)
  uint8_t appClock = 1;
  uint8_t appWeather = 1;
  uint8_t appPomodoro = 1;
  uint8_t appVirtualPet = 1;
  uint8_t appReadingStats = 1;
  uint8_t appSleepImagePicker = 1;
  uint8_t appGames = 1;  // Master toggle for all games (Chess, Caro, Sudoku, Minesweeper, 2048)
  uint8_t appFlashcard = 1;  // Per-app visibility toggle (1=show, 0=hide)
  uint8_t flashcardNewPerDay = 10;      // New cards per day limit
  uint8_t flashcardMaxReviewPerDay = 250; // Max reviews per day (capped at 255 for uint8_t)

 private:
  CrossPetSettings() = default;
  static CrossPetSettings instance;

  static constexpr char SETTINGS_PATH[] = "/.crosspoint/crosspet.json";
};

// Helper macro to access CrossPet settings
#define PET_SETTINGS CrossPetSettings::getInstance()
