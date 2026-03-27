#include "SettingsActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <WiFi.h>

#include "ButtonRemapActivity.h"
#include "CalibreSettingsActivity.h"
#include "ClearCacheActivity.h"
#include "DeviceInfoActivity.h"
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

int SettingsActivity::nextSelectableIndex(int current, int dir) const {
  int next = current;
  const int count = static_cast<int>(allSettings.size());
  do {
    next = (next + dir + count) % count;
  } while (allSettings[next].type == SettingType::SECTION && next != current);
  return next;
}

void SettingsActivity::onEnter() {
  LOG_DBG("SET", "Free heap: %d bytes", ESP.getFreeHeap());

  if (ESP.getFreeHeap() < 50000) {
    LOG_ERR("SET", "Insufficient heap for settings: %d bytes", ESP.getFreeHeap());
    onGoHome();
    return;
  }

  Activity::onEnter();

  // Build flat settings list from the shared list (device-relevant categories only)
  // SECTION items from getSettingsList() serve as visual separators.
  allSettings.clear();

  for (const auto& setting : getSettingsList()) {
    if (setting.type == SettingType::SECTION) {
      // Keep only the four device UI sections
      if (strcmp(setting.sectionLabel, "Display") == 0 ||
          strcmp(setting.sectionLabel, "Reader") == 0 ||
          strcmp(setting.sectionLabel, "Controls") == 0 ||
          strcmp(setting.sectionLabel, "System") == 0) {
        allSettings.push_back(setting);
      }
      continue;
    }
    // Skip web-only categories
    if (setting.category == StrId::STR_NONE_OPT) continue;
    if (setting.category != StrId::STR_CAT_DISPLAY &&
        setting.category != StrId::STR_CAT_READER &&
        setting.category != StrId::STR_CAT_CONTROLS &&
        setting.category != StrId::STR_CAT_SYSTEM) {
      continue;
    }
    allSettings.push_back(setting);
  }

  // Inject Remap Front Buttons right after the Controls section header.
  // Find Controls section header index and insert action after it.
  {
    int insertPos = -1;
    for (int i = 0; i < static_cast<int>(allSettings.size()); i++) {
      if (allSettings[i].type == SettingType::SECTION &&
          allSettings[i].sectionLabel != nullptr &&
          strcmp(allSettings[i].sectionLabel, "Controls") == 0) {
        insertPos = i + 1;
        break;
      }
    }
    if (insertPos >= 0) {
      allSettings.insert(allSettings.begin() + insertPos,
                         SettingInfo::Action(StrId::STR_REMAP_FRONT_BUTTONS, SettingAction::RemapFrontButtons));
    }
  }

  // Inject Customise Status Bar at end of Reader section (before Controls section header)
  {
    int insertPos = -1;
    for (int i = 0; i < static_cast<int>(allSettings.size()); i++) {
      if (allSettings[i].type == SettingType::SECTION &&
          allSettings[i].sectionLabel != nullptr &&
          strcmp(allSettings[i].sectionLabel, "Controls") == 0) {
        insertPos = i;
        break;
      }
    }
    if (insertPos >= 0) {
      allSettings.insert(allSettings.begin() + insertPos,
                         SettingInfo::Action(StrId::STR_CUSTOMISE_STATUS_BAR, SettingAction::CustomiseStatusBar));
    }
  }

  // Append device-only System actions at the end
  allSettings.push_back(SettingInfo::Action(StrId::STR_WIFI_NETWORKS, SettingAction::Network));
  allSettings.push_back(SettingInfo::Action(StrId::STR_KOREADER_SYNC, SettingAction::KOReaderSync));
  allSettings.push_back(SettingInfo::Action(StrId::STR_OPDS_BROWSER, SettingAction::OPDSBrowser));
  allSettings.push_back(SettingInfo::Action(StrId::STR_CLEAR_READING_CACHE, SettingAction::ClearCache));
  allSettings.push_back(SettingInfo::Action(StrId::STR_CHECK_UPDATES, SettingAction::CheckForUpdates));
  allSettings.push_back(SettingInfo::Action(StrId::STR_LANGUAGE, SettingAction::Language));
  // BLE is in Tools menu (not Settings) to reduce activity stack depth for RAM
  allSettings.push_back(SettingInfo::Action(StrId::STR_DEVICE_INFO, SettingAction::DeviceInfo));
  allSettings.push_back(SettingInfo::Action(StrId::STR_REBOOT, SettingAction::Reboot));

  // Start on the first selectable item (skip any leading SECTION items)
  selectedSettingIndex = 0;
  if (!allSettings.empty() && allSettings[0].type == SettingType::SECTION) {
    selectedSettingIndex = nextSelectableIndex(0, 1);
  }

  requestUpdate();
}

void SettingsActivity::onExit() {
  Activity::onExit();

  UITheme::getInstance().reload();  // Re-apply theme in case it was changed
}

void SettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    toggleCurrentSetting();
    requestUpdate();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    onGoHome();
    return;
  }

  // Navigate down, skipping SECTION items
  buttonNavigator.onNextRelease([this] {
    selectedSettingIndex = nextSelectableIndex(selectedSettingIndex, 1);
    requestUpdate();
  });

  // Navigate up, skipping SECTION items
  buttonNavigator.onPreviousRelease([this] {
    selectedSettingIndex = nextSelectableIndex(selectedSettingIndex, -1);
    requestUpdate();
  });

  // Fast scroll: same direction, skips SECTIONs
  buttonNavigator.onNextContinuous([this] {
    selectedSettingIndex = nextSelectableIndex(selectedSettingIndex, 1);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this] {
    selectedSettingIndex = nextSelectableIndex(selectedSettingIndex, -1);
    requestUpdate();
  });
}

void SettingsActivity::toggleCurrentSetting() {
  if (selectedSettingIndex < 0 || selectedSettingIndex >= static_cast<int>(allSettings.size())) {
    return;
  }

  const auto& setting = allSettings[selectedSettingIndex];

  // SECTION items are non-interactive
  if (setting.type == SettingType::SECTION) {
    return;
  }

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
      // BleRemote handled in ToolsActivity (not Settings)
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

static const char* getSettingDescription(const SettingInfo& s) {
  if (!s.key) return nullptr;
  if (strcmp(s.key, "keepClockAlive") == 0) return tr(STR_HELP_KEEP_CLOCK_ALIVE);
  if (strcmp(s.key, "sleepRefreshInterval") == 0) return tr(STR_HELP_SLEEP_REFRESH);
  if (strcmp(s.key, "fadingFix") == 0) return tr(STR_HELP_FADING_FIX);
  if (strcmp(s.key, "refreshFrequency") == 0) return tr(STR_HELP_REFRESH_FREQ);
  if (strcmp(s.key, "textAntiAliasing") == 0) return tr(STR_HELP_TEXT_AA);
  if (strcmp(s.key, "sleepScreen") == 0) return tr(STR_HELP_SLEEP_SCREEN);
  if (strcmp(s.key, "fontFamily") == 0) return tr(STR_HELP_FONT_FAMILY);
  if (strcmp(s.key, "fontSize") == 0) return tr(STR_HELP_FONT_SIZE);
  if (strcmp(s.key, "orientation") == 0) return tr(STR_HELP_ORIENTATION);
  if (strcmp(s.key, "hyphenationEnabled") == 0) return tr(STR_HELP_HYPHENATION);
  if (strcmp(s.key, "embeddedStyle") == 0) return tr(STR_HELP_EMBEDDED_STYLE);
  if (strcmp(s.key, "hideBatteryPercentage") == 0) return tr(STR_HELP_HIDE_BATTERY);
  if (strcmp(s.key, "sleepTimeout") == 0) return tr(STR_HELP_TIME_TO_SLEEP);
  return nullptr;
}

void SettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SETTINGS_TITLE),
                 CROSSPOINT_VERSION);

  // Flat list: no tab bar — reclaim that height for more list items
  const int listTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listHeight = pageHeight - listTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int itemCount = static_cast<int>(allSettings.size());

  GUI.drawList(
      renderer,
      Rect{0, listTop, pageWidth, listHeight},
      itemCount, selectedSettingIndex,
      [this](int index) -> std::string {
        const auto& s = allSettings[index];
        // SECTION items use \x01 prefix — drawList renders them as bold headers
        if (s.type == SettingType::SECTION) {
          return std::string("\x01") + (s.sectionLabel ? s.sectionLabel : "");
        }
        return std::string(I18N.get(s.nameId));
      },
      [this](int index) -> std::string {
        if (index != selectedSettingIndex) return "";
        const auto& s = allSettings[index];
        if (s.type == SettingType::SECTION) return "";
        const auto* desc = getSettingDescription(s);
        return desc ? std::string(desc) : "";
      },
      nullptr,
      [this](int i) -> std::string {
        const auto& setting = allSettings[i];
        if (setting.type == SettingType::SECTION) return "";
        std::string valueText = "";
        if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
          const bool value = SETTINGS.*(setting.valuePtr);
          valueText = value ? tr(STR_STATE_ON) : tr(STR_STATE_OFF);
        } else if (setting.type == SettingType::TOGGLE && setting.valueGetter) {
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
