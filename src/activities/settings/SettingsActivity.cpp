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

const StrId SettingsActivity::categoryNames[categoryCount] = {StrId::STR_CAT_DISPLAY, StrId::STR_CAT_READER,
                                                              StrId::STR_CAT_CONTROLS, StrId::STR_CAT_SYSTEM};

void SettingsActivity::onEnter() {
  Activity::onEnter();

  // Build per-category vectors from the shared settings list
  displaySettings.clear();
  readerSettings.clear();
  controlsSettings.clear();
  systemSettings.clear();

  for (const auto& setting : getSettingsList()) {
    if (setting.category == StrId::STR_NONE_OPT) continue;
    if (setting.category == StrId::STR_CAT_DISPLAY) {
      displaySettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_READER) {
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

  // Insert section headers into each category (device UI only, after ACTION items are appended).
  // Display: SCREEN (sleep/refresh settings), APPEARANCE (theme/UI), HOME (home screen widgets)
  // Items order: Sleep Screen(0), Sleep Cover Mode(1), Sleep Cover Filter(2), Keep Clock Alive(3),
  //              Sleep Refresh(4), Hide Battery(5), Refresh Freq(6), UI Theme(7), Sunlight Fix(8),
  //              Dark Mode(9), Temp Unit(10), Home Clock(11), Home Weather(12), Home Pet Status(13), Home Focus Mode(14)
  displaySettings.insert(displaySettings.begin() + 11, SettingInfo::Section("HOME"));
  displaySettings.insert(displaySettings.begin() + 5, SettingInfo::Section("APPEARANCE"));
  displaySettings.insert(displaySettings.begin(), SettingInfo::Section("SCREEN"));

  // Reader: TEXT (font/layout), NAVIGATION (orientation/turn), ACTIONS (status bar)
  // Items order: Font Family(0), Font Size(1), Line Spacing(2), Screen Margin(3), Para Alignment(4),
  //              Embedded Style(5), Hyphenation(6), Orientation(7), Extra Spacing(8), Text AA(9),
  //              Text Darkness(10), Images(11), Auto Page Turn(12), Customise Status Bar(13)
  readerSettings.insert(readerSettings.begin() + 13, SettingInfo::Section("ACTIONS"));
  readerSettings.insert(readerSettings.begin() + 7, SettingInfo::Section("READING"));
  readerSettings.insert(readerSettings.begin(), SettingInfo::Section("TEXT"));

  // Controls: BUTTONS (remapping action + layout toggles), POWER BUTTON (short pwr btn actions)
  // Items order after insert: Remap Front Buttons(0), Side Btn Layout(1), Front Page Btn Layout(2),
  //                           Long Press Skip(3), Short Pwr Btn(4), 2Click(5), 3Click(6)
  controlsSettings.insert(controlsSettings.begin() + 4, SettingInfo::Section("POWER BUTTON"));
  controlsSettings.insert(controlsSettings.begin(), SettingInfo::Section("BUTTONS"));

  // System: GENERAL (sleep timeout, files), CONNECTIVITY (WiFi, KOReader, OPDS), MAINTENANCE (clear, updates, reboot)
  // Items order: Sleep Timeout(0), Show Hidden Files(1), Show Free Heap(2),
  //              WiFi(3), KOReader Sync(4), OPDS Browser(5),
  //              Clear Cache(6), Check Updates(7), Language(8), Device Info(9), Reboot(10)
  systemSettings.insert(systemSettings.begin() + 9, SettingInfo::Section("DEVICE"));
  systemSettings.insert(systemSettings.begin() + 3, SettingInfo::Section("CONNECTIVITY"));
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
