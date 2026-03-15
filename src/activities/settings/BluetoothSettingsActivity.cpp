// BluetoothSettingsActivity.cpp
// Main dispatch: onEnter/onExit/loop/render + main menu input/render
#include "BluetoothSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <cstring>

#include "CrossPointSettings.h"
#include "DeviceProfiles.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BluetoothSettingsActivity::onEnter() {
  Activity::onEnter();

  selectedIndex = 0;
  viewMode = ViewMode::MAIN_MENU;
  lastError = "";
  lastScanTime = 0;
  pendingLearnKey = 0;
  learnedPrevKey = 0;
  learnedNextKey = 0;
  learnStep = LearnStep::WAIT_PREV;

  // Get BLE manager instance and restore persistent state
  try {
    btMgr = &BluetoothHIDManager::getInstance();
    LOG_INF("BT", "BluetoothHIDManager ready");

    // Auto-restore BLE state from settings
    if (SETTINGS.bleEnabled && !btMgr->isEnabled()) { btMgr->enable(); }

    btMgr->setInputCallback([this](uint16_t keycode) {
      if (keycode > 0 && keycode <= 0xFF) {
        pendingLearnKey = static_cast<uint8_t>(keycode);
      }
    });
  } catch (const std::exception& e) {
    LOG_ERR("BT", "Failed to get BLE manager: %s", e.what());
    lastError = "BLE manager error";
    btMgr = nullptr;
  } catch (...) {
    LOG_ERR("BT", "Unknown error getting BLE manager");
    lastError = "Unknown error";
    btMgr = nullptr;
  }

  requestUpdate();
}

void BluetoothSettingsActivity::onExit() {
  if (btMgr) {
    btMgr->setInputCallback(nullptr);
  }
  Activity::onExit();
}

void BluetoothSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (viewMode == ViewMode::DEVICE_LIST) {
      viewMode = ViewMode::MAIN_MENU;
      selectedIndex = 0;
      if (btMgr && btMgr->isScanning()) {
        btMgr->stopScan();
      }
      requestUpdate();
      return;
    } else if (viewMode == ViewMode::LEARN_KEYS) {
      viewMode = ViewMode::MAIN_MENU;
      selectedIndex = 0;
      lastError = "Learn mode canceled";
      requestUpdate();
      return;
    } else {
      finish();
      return;
    }
  }

  // Check if scan completed — small delay to show final results
  if (btMgr && viewMode == ViewMode::DEVICE_LIST && !btMgr->isScanning() && lastScanTime > 0) {
    if (millis() - lastScanTime > 500) {
      lastScanTime = 0;
      requestUpdate();
    }
  }

  if (viewMode == ViewMode::MAIN_MENU) {
    handleMainMenuInput();
  } else if (viewMode == ViewMode::DEVICE_LIST) {
    handleDeviceListInput();
  } else {
    handleLearnInput();
  }
}

void BluetoothSettingsActivity::render(RenderLock&& lock) {
  if (viewMode == ViewMode::MAIN_MENU) {
    renderMainMenu();
  } else if (viewMode == ViewMode::DEVICE_LIST) {
    renderDeviceList();
  } else {
    renderLearnKeys();
  }
}

void BluetoothSettingsActivity::handleMainMenuInput() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : 3;
    requestUpdate();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectedIndex = (selectedIndex < 3) ? selectedIndex + 1 : 0;
    requestUpdate();
  }

  if (!mappedInput.wasPressed(MappedInputManager::Button::Confirm)) return;

  if (!btMgr) {
    lastError = "BLE not available";
    LOG_ERR("BT", "BLE manager not available");
    requestUpdate();
    return;
  }

  try {
    if (selectedIndex == 0) {
      // Toggle Bluetooth on/off
      if (btMgr->isEnabled()) {
        LOG_INF("BT", "Disabling Bluetooth...");
        if (btMgr->disable()) {
          lastError = "Bluetooth disabled";
          SETTINGS.bleEnabled = 0; SETTINGS.saveToFile();
        } else {
          lastError = "Failed to disable";
        }
      } else {
        LOG_INF("BT", "Enabling Bluetooth...");
        if (btMgr->enable()) {
          lastError = "Bluetooth enabled";
          SETTINGS.bleEnabled = 1; SETTINGS.saveToFile();
        } else {
          lastError = btMgr->lastError.empty() ? "Failed to enable" : btMgr->lastError;
        }
      }
    } else if (selectedIndex == 1) {
      // Scan for devices
      if (btMgr->isEnabled()) {
        btMgr->startScan(10000);
        lastScanTime = millis();
        viewMode = ViewMode::DEVICE_LIST;
        selectedIndex = 0;
        lastError = "";
      } else {
        lastError = "Enable BT first";
      }
    } else if (selectedIndex == 2) {
      // Learn page-turn keys
      if (!btMgr->isEnabled()) {
        lastError = "Enable BT first";
      } else if (btMgr->getConnectedDevices().empty()) {
        lastError = "Connect a remote first";
      } else {
        viewMode = ViewMode::LEARN_KEYS;
        learnStep = LearnStep::WAIT_PREV;
        pendingLearnKey = 0;
        learnedPrevKey = 0;
        learnedNextKey = 0;
        lastError = "Press PREVIOUS PAGE button";
      }
    } else if (selectedIndex == 3) {
      // Clear learned key mapping
      DeviceProfiles::clearCustomProfile();
      lastError = "Learned mapping cleared";
    }
  } catch (const std::exception& e) {
    lastError = std::string("Error: ") + e.what();
    LOG_ERR("BT", "Menu action error: %s", e.what());
  } catch (...) {
    lastError = "Unknown error";
    LOG_ERR("BT", "Unknown error in main menu action");
  }

  requestUpdate();
}

void BluetoothSettingsActivity::renderMainMenu() {
  auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BLE_REMOTE));

  // Status line below header
  std::string statusLine;
  if (btMgr) {
    auto connDevices = btMgr->getConnectedDevices();
    if (btMgr->isEnabled()) {
      if (!connDevices.empty()) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Enabled, %zu device(s) connected", connDevices.size());
        statusLine = buf;
      } else {
        statusLine = "Enabled, no devices connected";
      }
    } else {
      statusLine = "Disabled";
    }
  } else {
    statusLine = "Error initializing Bluetooth";
  }

  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    statusLine.c_str());

  // Build menu items dynamically (Enable/Disable label depends on BT state)
  const char* toggleLabel = (btMgr && btMgr->isEnabled()) ? "Disable Bluetooth" : "Enable Bluetooth";
  std::vector<std::string> itemLabels = {toggleLabel, "Scan for Devices", "Learn Page-Turn Keys",
                                         "Clear Learned Keys"};

  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.buttonHintsHeight +
                         metrics.verticalSpacing * 2)},
      4, selectedIndex, [&itemLabels](int index) { return itemLabels[index]; }, nullptr, nullptr,
      [this](int /*i*/) { return lastError; }, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
