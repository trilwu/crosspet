#include "SettingsActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <WiFi.h>

#include "ButtonRemapActivity.h"
#include "CalibreSettingsActivity.h"
#include "ClearCacheActivity.h"
#include "DeviceInfoActivity.h"
#include "CrossPetSettings.h"
#include "CrossPointSettings.h"
#include "KOReaderSettingsActivity.h"
#include "LanguageSelectActivity.h"
#include "MappedInputManager.h"
#include "OtaUpdateActivity.h"
#include "SettingsList.h"
#include "StatusBarSettingsActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

const StrId SettingsActivity::categoryNames[categoryCount] = {StrId::STR_CAT_DISPLAY, StrId::STR_CAT_READER,
                                                              StrId::STR_CAT_CONTROLS, StrId::STR_CAT_SYSTEM,
                                                              StrId::STR_CROSSPET};

void SettingsActivity::onEnter() {
  LOG_DBG("SET", "Free heap: %d bytes", ESP.getFreeHeap());

  if (ESP.getFreeHeap() < 50000) {
    LOG_ERR("SET", "Insufficient heap for settings: %d bytes", ESP.getFreeHeap());
    onGoHome();
    return;
  }

  Activity::onEnter();

  // Build per-category vectors from the shared settings list
  displaySettings.clear();
  readerSettings.clear();
  controlsSettings.clear();
  systemSettings.clear();
  displaySettings.reserve(12);
  readerSettings.reserve(20);
  controlsSettings.reserve(12);
  systemSettings.reserve(16);

  for (const auto& setting : getSettingsList()) {
    if (setting.category == StrId::STR_NONE_OPT) continue;
    if (setting.category == StrId::STR_CAT_DISPLAY) {
      if (setting.nameId == StrId::STR_HOME_FOCUS_MODE) continue;  // → CrossPet tab
      if (setting.nameId == StrId::STR_DARK_MODE) continue;        // → CrossPet tab
      displaySettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_READER) {
      if (setting.nameId == StrId::STR_FONT_FAMILY || setting.nameId == StrId::STR_FONT_SIZE) continue;  // → CROSSPET section
      readerSettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_CONTROLS) {
      controlsSettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_SYSTEM) {
      systemSettings.push_back(setting);
    }
    // Web-only categories (KOReader Sync, OPDS Browser) are skipped for device UI
  }

  // Append device-only ACTION items
  controlsSettings.insert(controlsSettings.begin(),
                          SettingInfo::Action(StrId::STR_REMAP_FRONT_BUTTONS, SettingAction::RemapFrontButtons));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_WIFI_NETWORKS, SettingAction::Network));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_KOREADER_SYNC, SettingAction::KOReaderSync));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_OPDS_BROWSER, SettingAction::OPDSBrowser));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_CLEAR_READING_CACHE, SettingAction::ClearCache));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_CHECK_UPDATES, SettingAction::CheckForUpdates));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_LANGUAGE, SettingAction::Language));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_DEVICE_INFO, SettingAction::DeviceInfo));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_REBOOT, SettingAction::Reboot));
  readerSettings.push_back(SettingInfo::Action(StrId::STR_CUSTOMISE_STATUS_BAR, SettingAction::CustomiseStatusBar));

  // Build APPS tab — per-app visibility toggles (added dynamically to avoid
  // stack overflow from too many std::function allocations in static init).
  appsSettings.clear();
  appsSettings.reserve(16);
  appsSettings.push_back(SettingInfo::Section("APPS"));
  struct AppToggle { StrId id; uint8_t CrossPetSettings::* field; const char* key; };
  const AppToggle appToggles[] = {
    {StrId::STR_CLOCK,              &CrossPetSettings::appClock,            "appClock"},
    {StrId::STR_WEATHER,            &CrossPetSettings::appWeather,          "appWeather"},
    {StrId::STR_POMODORO,           &CrossPetSettings::appPomodoro,         "appPomodoro"},
    {StrId::STR_VIRTUAL_PET,        &CrossPetSettings::appVirtualPet,       "appVirtualPet"},
    {StrId::STR_READING_STATS_APP,  &CrossPetSettings::appReadingStats,     "appReadingStats"},
    {StrId::STR_SLEEP_IMAGE_PICKER, &CrossPetSettings::appSleepImagePicker, "appSleepImagePicker"},
    {StrId::STR_CHESS,              &CrossPetSettings::appChess,            "appChess"},
    {StrId::STR_CARO,               &CrossPetSettings::appCaro,             "appCaro"},
    {StrId::STR_SUDOKU,             &CrossPetSettings::appSudoku,           "appSudoku"},
    {StrId::STR_MINESWEEPER,        &CrossPetSettings::appMinesweeper,      "appMinesweeper"},
    {StrId::STR_2048,               &CrossPetSettings::app2048,             "app2048"},
  };
  for (const auto& t : appToggles) {
    auto field = t.field;
    appsSettings.push_back(SettingInfo::DynamicToggle(
        t.id,
        [field] { return PET_SETTINGS.*field; },
        [field](uint8_t v) { PET_SETTINGS.*field = v; PET_SETTINGS.saveToFile(); },
        t.key, StrId::STR_CROSSPET));
  }
  // CrossPet options — separated from app toggles
  appsSettings.push_back(SettingInfo::Section("OPTIONS"));
  appsSettings.push_back(SettingInfo::Toggle(StrId::STR_DARK_MODE, &CrossPointSettings::darkMode,
      "darkMode", StrId::STR_CROSSPET));
  appsSettings.push_back(SettingInfo::DynamicToggle(
      StrId::STR_HOME_FOCUS_MODE,
      [] { return PET_SETTINGS.homeFocusMode; },
      [](uint8_t v) { PET_SETTINGS.homeFocusMode = v; PET_SETTINGS.saveToFile(); },
      "homeFocusMode", StrId::STR_CROSSPET));

  // CROSSPET section in Reader — Font Family & Font Size (skipped from TEXT above)
  readerSettings.push_back(SettingInfo::Section("CROSSPET"));
  readerSettings.push_back(SettingInfo::Enum(StrId::STR_FONT_FAMILY, &CrossPointSettings::fontFamily,
      {StrId::STR_BOOKERLY, StrId::STR_LEXEND, StrId::STR_BOKERLAM}, "fontFamily", StrId::STR_CAT_READER));
  readerSettings.push_back(SettingInfo::Enum(StrId::STR_FONT_SIZE, &CrossPointSettings::fontSize,
      {StrId::STR_SMALL, StrId::STR_MEDIUM, StrId::STR_LARGE, StrId::STR_X_LARGE}, "fontSize", StrId::STR_CAT_READER));

  // Insert section headers (bottom-up to keep indices stable). Log sizes for debugging.
  LOG_DBG("SET", "display=%d reader=%d controls=%d system=%d apps=%d heap=%d",
          (int)displaySettings.size(), (int)readerSettings.size(),
          (int)controlsSettings.size(), (int)systemSettings.size(),
          (int)appsSettings.size(), ESP.getFreeHeap());

  // Display (8 items): SCREEN (sleep 0-2), APPEARANCE (UI 3-7)
  if (displaySettings.size() >= 4) {
    displaySettings.insert(displaySettings.begin() + 3, SettingInfo::Section("APPEARANCE"));
  }
  displaySettings.insert(displaySettings.begin(), SettingInfo::Section("SCREEN"));

  // Reader (14 items): TEXT (0-4), READING (5-9), ACTIONS (10), CROSSPET section+items (11-13)
  if (readerSettings.size() >= 11) {
    readerSettings.insert(readerSettings.begin() + 10, SettingInfo::Section("ACTIONS"));
  }
  if (readerSettings.size() >= 6) {
    readerSettings.insert(readerSettings.begin() + 5, SettingInfo::Section("READING"));
  }
  readerSettings.insert(readerSettings.begin(), SettingInfo::Section("TEXT"));

  // Controls: BUTTONS (remap+layout 0-3), POWER BUTTON (pwr btn 4-6)
  if (controlsSettings.size() >= 5) {
    controlsSettings.insert(controlsSettings.begin() + 4, SettingInfo::Section("POWER BUTTON"));
  }
  controlsSettings.insert(controlsSettings.begin(), SettingInfo::Section("BUTTONS"));

  // System: GENERAL (0-2), CONNECTIVITY (3-5), DEVICE (6-10)
  if (systemSettings.size() >= 7) {
    systemSettings.insert(systemSettings.begin() + 6, SettingInfo::Section("DEVICE"));
  }
  if (systemSettings.size() >= 4) {
    systemSettings.insert(systemSettings.begin() + 3, SettingInfo::Section("CONNECTIVITY"));
  }
  systemSettings.insert(systemSettings.begin(), SettingInfo::Section("GENERAL"));

  // Reset selection to first category
  selectedCategoryIndex = 0;
  selectedSettingIndex = 0;

  // Initialize with first category (Display)
  currentSettings = &displaySettings;
  settingsCount = static_cast<int>(displaySettings.size());

  // Trigger first update
  requestUpdate();
}

void SettingsActivity::onExit() {
  Activity::onExit();

  UITheme::getInstance().reload();  // Re-apply theme in case it was changed
}

void SettingsActivity::loop() {
  bool hasChangedCategory = false;

  // Handle actions with early return
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selectedSettingIndex == 0) {
      selectedCategoryIndex = (selectedCategoryIndex < categoryCount - 1) ? (selectedCategoryIndex + 1) : 0;
      hasChangedCategory = true;
      requestUpdate();
    } else {
      toggleCurrentSetting();
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    onGoHome();
    return;
  }

  // Handle navigation — skip SECTION items (non-interactive headers)
  buttonNavigator.onNextRelease([this] {
    do {
      selectedSettingIndex = ButtonNavigator::nextIndex(selectedSettingIndex, settingsCount + 1);
    } while (selectedSettingIndex > 0 && selectedSettingIndex <= settingsCount &&
             (*currentSettings)[selectedSettingIndex - 1].type == SettingType::SECTION);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    do {
      selectedSettingIndex = ButtonNavigator::previousIndex(selectedSettingIndex, settingsCount + 1);
    } while (selectedSettingIndex > 0 && selectedSettingIndex <= settingsCount &&
             (*currentSettings)[selectedSettingIndex - 1].type == SettingType::SECTION);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, &hasChangedCategory] {
    hasChangedCategory = true;
    selectedCategoryIndex = ButtonNavigator::nextIndex(selectedCategoryIndex, categoryCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, &hasChangedCategory] {
    hasChangedCategory = true;
    selectedCategoryIndex = ButtonNavigator::previousIndex(selectedCategoryIndex, categoryCount);
    requestUpdate();
  });

  if (hasChangedCategory) {
    switch (selectedCategoryIndex) {
      case 0:
        currentSettings = &displaySettings;
        break;
      case 1:
        currentSettings = &readerSettings;
        break;
      case 2:
        currentSettings = &controlsSettings;
        break;
      case 3:
        currentSettings = &systemSettings;
        break;
      case 4:
        currentSettings = &appsSettings;
        break;
    }
    settingsCount = static_cast<int>(currentSettings->size());

    if (selectedSettingIndex == 0) {
      // Tab bar is focused — stay there
    } else {
      // Find first non-SECTION item in new category
      selectedSettingIndex = 1;
      while (selectedSettingIndex <= settingsCount &&
             (*currentSettings)[selectedSettingIndex - 1].type == SettingType::SECTION) {
        selectedSettingIndex++;
      }
      // If somehow all items are sections (shouldn't happen), fall back to tab bar
      if (selectedSettingIndex > settingsCount) selectedSettingIndex = 0;
    }
  }
}

void SettingsActivity::toggleCurrentSetting() {
  int selectedSetting = selectedSettingIndex - 1;
  if (selectedSetting < 0 || selectedSetting >= settingsCount) {
    return;
  }

  const auto& setting = (*currentSettings)[selectedSetting];

  if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
    // Toggle the boolean value using the member pointer
    const bool currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = !currentValue;
  } else if (setting.type == SettingType::TOGGLE && setting.valueGetter && setting.valueSetter) {
    // DynamicToggle: backed by external getter/setter (e.g. CrossPetSettings)
    const uint8_t currentValue = setting.valueGetter();
    setting.valueSetter(currentValue ? 0 : 1);
    return;  // setter is responsible for saving; skip SETTINGS.saveToFile() below
  } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
    const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = (currentValue + 1) % static_cast<uint8_t>(setting.enumValues.size());
  } else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
    const int8_t currentValue = SETTINGS.*(setting.valuePtr);
    if (currentValue + setting.valueRange.step > setting.valueRange.max) {
      SETTINGS.*(setting.valuePtr) = setting.valueRange.min;
    } else {
      SETTINGS.*(setting.valuePtr) = currentValue + setting.valueRange.step;
    }
  } else if (setting.type == SettingType::ACTION) {
    auto resultHandler = [this](const ActivityResult&) { SETTINGS.saveToFile(); };

    switch (setting.action) {
      case SettingAction::RemapFrontButtons:
        startActivityForResult(std::make_unique<ButtonRemapActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::CustomiseStatusBar:
        startActivityForResult(std::make_unique<StatusBarSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::KOReaderSync:
        startActivityForResult(std::make_unique<KOReaderSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::OPDSBrowser:
        startActivityForResult(std::make_unique<CalibreSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::Network: {
        startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, false),
                               [this](const ActivityResult&) {
                                 // WifiSelectionActivity doesn't own WiFi lifecycle — clean up here
                                 WiFi.disconnect(false);
                                 WiFi.mode(WIFI_OFF);
                                 SETTINGS.saveToFile();
                               });
        break;
      }
      case SettingAction::ClearCache:
        startActivityForResult(std::make_unique<ClearCacheActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::CheckForUpdates:
        startActivityForResult(std::make_unique<OtaUpdateActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::Language:
        startActivityForResult(std::make_unique<LanguageSelectActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::DeviceInfo:
        startActivityForResult(std::make_unique<DeviceInfoActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::Reboot:
        renderer.clearScreen();
        renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2 - 10,
                                  "Rebooting...", true, EpdFontFamily::BOLD);
        renderer.displayBuffer();
        delay(500);
        ESP.restart();
        break;
      case SettingAction::None:
        // Do nothing
        break;
    }
    return;  // Results will be handled in the result handler, so we can return early here
  } else {
    return;
  }

  SETTINGS.saveToFile();
}

void SettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SETTINGS_TITLE),
                 CROSSPOINT_VERSION);

  std::vector<TabInfo> tabs;
  tabs.reserve(categoryCount);
  for (int i = 0; i < categoryCount; i++) {
    tabs.push_back({I18N.get(categoryNames[i]), selectedCategoryIndex == i});
  }
  GUI.drawTabBar(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight}, tabs,
                 selectedSettingIndex == 0);

  const auto& settings = *currentSettings;
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.buttonHintsHeight +
                         metrics.verticalSpacing * 2)},
      settingsCount, selectedSettingIndex - 1,
      [&settings](int index) {
        const auto& s = settings[index];
        // Section header: prefix with \x01 marker for special rendering in drawList
        if (s.type == SettingType::SECTION) {
          return s.sectionLabel ? std::string("\x01") + s.sectionLabel : std::string("");
        }
        return std::string(I18N.get(s.nameId));
      },
      nullptr, nullptr,
      [&settings](int i) {
        const auto& setting = settings[i];
        // Section headers have no value
        if (setting.type == SettingType::SECTION) return std::string("");
        std::string valueText = "";
        if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
          const bool value = SETTINGS.*(setting.valuePtr);
          valueText = value ? tr(STR_STATE_ON) : tr(STR_STATE_OFF);
        } else if (setting.type == SettingType::TOGGLE && setting.valueGetter) {
          // DynamicToggle: backed by external getter (e.g. CrossPetSettings)
          valueText = setting.valueGetter() ? tr(STR_STATE_ON) : tr(STR_STATE_OFF);
        } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
          const uint8_t value = SETTINGS.*(setting.valuePtr);
          valueText = I18N.get(setting.enumValues[value]);
        } else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
          valueText = std::to_string(SETTINGS.*(setting.valuePtr));
        }
        return valueText;
      },
      true);

  // Draw help text
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TOGGLE), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  // Always use standard refresh for settings screen
  renderer.displayBuffer();
}
