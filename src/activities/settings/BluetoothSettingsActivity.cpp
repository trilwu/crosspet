#ifdef ENABLE_BLE
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
    LOG_INF("BT", "[HEAP] Before BLE manager: %d bytes free", ESP.getFreeHeap());
    btMgr = &BluetoothHIDManager::getInstance();
    LOG_INF("BT", "[HEAP] After getInstance: %d bytes free", ESP.getFreeHeap());

    // Don't auto-enable here — user must press "Enable" to start NimBLE
    // This preserves heap until BLE is actually needed
    LOG_INF("BT", "[HEAP] BT settings entered: %d bytes free", ESP.getFreeHeap());

    // Show bonded device info (reconnect is manual via device list)
    if (strlen(SETTINGS.bleBondedDeviceAddr) > 0 && btMgr->getConnectedDevices().empty()) {
      char buf[64];
      snprintf(buf, sizeof(buf), "Paired: %s (not connected)", SETTINGS.bleBondedDeviceName);
      lastError = buf;
    }

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

// Menu actions — indices are dynamic based on whether bonded device exists
enum MenuAction { TOGGLE_BT = 0, RECONNECT, SCAN, LEARN_KEYS, CLEAR_KEYS, FORGET_DEVICE };

// Build dynamic menu: toggle + optional reconnect + scan + learn + clear + optional forget
static int buildMenuActions(bool hasBonded, bool isConnected, MenuAction* actions, int maxActions) {
  int n = 0;
  actions[n++] = TOGGLE_BT;
  if (hasBonded && !isConnected) actions[n++] = RECONNECT;
  actions[n++] = SCAN;
  actions[n++] = LEARN_KEYS;
  actions[n++] = CLEAR_KEYS;
  if (hasBonded) actions[n++] = FORGET_DEVICE;
  return n;
}

void BluetoothSettingsActivity::handleMainMenuInput() {
  const bool hasBonded = strlen(SETTINGS.bleBondedDeviceAddr) > 0;
  const bool isConnected = btMgr && !btMgr->getConnectedDevices().empty();
  MenuAction actions[6];
  const int menuCount = buildMenuActions(hasBonded, isConnected, actions, 6);

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : menuCount - 1;
    requestUpdate();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectedIndex = (selectedIndex < menuCount - 1) ? selectedIndex + 1 : 0;
    requestUpdate();
  }

  if (!mappedInput.wasPressed(MappedInputManager::Button::Confirm)) return;

  if (!btMgr) {
    lastError = "BLE not available";
    requestUpdate();
    return;
  }

  if (selectedIndex >= menuCount) { requestUpdate(); return; }
  MenuAction action = actions[selectedIndex];

  try {
    switch (action) {
      case TOGGLE_BT:
        if (btMgr->isEnabled()) {
          if (btMgr->disable()) { lastError = "Bluetooth disabled"; SETTINGS.bleEnabled = 0; SETTINGS.saveToFile(); }
          else lastError = "Failed to disable";
        } else {
          if (btMgr->enable()) { lastError = "Bluetooth enabled"; SETTINGS.bleEnabled = 1; SETTINGS.saveToFile(); }
          else lastError = btMgr->lastError.empty() ? "Failed to enable" : btMgr->lastError;
        }
        break;
      case RECONNECT: {
        if (!btMgr->isEnabled()) { lastError = "Enable BT first"; break; }
        lastError = "Reconnecting...";
        requestUpdateAndWait();
        if (btMgr->connectToDevice(std::string(SETTINGS.bleBondedDeviceAddr))) {
          lastError = std::string("Connected to ") + SETTINGS.bleBondedDeviceName;
        } else {
          lastError = btMgr->lastError.empty() ? "Reconnect failed" : btMgr->lastError;
        }
        break;
      }
      case SCAN:
        if (btMgr->isEnabled()) {
          btMgr->startScan(10000);
          lastScanTime = millis();
          viewMode = ViewMode::DEVICE_LIST;
          selectedIndex = 0;
          lastError = "";
        } else { lastError = "Enable BT first"; }
        break;
      case LEARN_KEYS:
        if (!btMgr->isEnabled()) { lastError = "Enable BT first"; }
        else if (btMgr->getConnectedDevices().empty()) { lastError = "Connect a remote first"; }
        else {
          viewMode = ViewMode::LEARN_KEYS;
          learnStep = LearnStep::WAIT_PREV;
          pendingLearnKey = 0; learnedPrevKey = 0; learnedNextKey = 0;
          lastError = "Press PREVIOUS PAGE button";
        }
        break;
      case CLEAR_KEYS:
        DeviceProfiles::clearCustomProfile();
        lastError = "Learned mapping cleared";
        break;
      case FORGET_DEVICE:
        memset(SETTINGS.bleBondedDeviceAddr, 0, sizeof(SETTINGS.bleBondedDeviceAddr));
        memset(SETTINGS.bleBondedDeviceName, 0, sizeof(SETTINGS.bleBondedDeviceName));
        SETTINGS.bleEnabled = 0;
        SETTINGS.saveToFile();
        if (btMgr->isEnabled()) btMgr->disable();
        lastError = "Device forgotten";
        break;
    }
  } catch (const std::exception& e) {
    lastError = std::string("Error: ") + e.what();
  } catch (...) {
    lastError = "Unknown error";
  }

  requestUpdate();
}

void BluetoothSettingsActivity::renderMainMenu() {
  auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BLE_REMOTE));

  // Status line below header — includes free heap for debugging
  char statusBuf[80];
  if (btMgr) {
    auto connDevices = btMgr->getConnectedDevices();
    if (btMgr->isEnabled()) {
      if (!connDevices.empty()) {
        snprintf(statusBuf, sizeof(statusBuf), "Connected (%zu) | %dKB free", connDevices.size(), ESP.getFreeHeap() / 1024);
      } else {
        snprintf(statusBuf, sizeof(statusBuf), "Enabled, disconnected | %dKB free", ESP.getFreeHeap() / 1024);
      }
    } else {
      snprintf(statusBuf, sizeof(statusBuf), "Disabled | %dKB free", ESP.getFreeHeap() / 1024);
    }
  } else {
    snprintf(statusBuf, sizeof(statusBuf), "BT error | %dKB free", ESP.getFreeHeap() / 1024);
  }
  std::string statusLine = statusBuf;

  // Show last action result as right-side label in subheader
  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    statusLine.c_str(), lastError.empty() ? nullptr : lastError.c_str());

  // Build dynamic menu labels matching handleMainMenuInput actions
  const bool hasBonded = strlen(SETTINGS.bleBondedDeviceAddr) > 0;
  const bool isConnected = btMgr && !btMgr->getConnectedDevices().empty();
  MenuAction actions[6];
  const int menuCount = buildMenuActions(hasBonded, isConnected, actions, 6);

  std::vector<std::string> itemLabels;
  for (int i = 0; i < menuCount; i++) {
    switch (actions[i]) {
      case TOGGLE_BT: itemLabels.push_back((btMgr && btMgr->isEnabled()) ? "Disable Bluetooth" : "Enable Bluetooth"); break;
      case RECONNECT: {
        char buf[48];
        snprintf(buf, sizeof(buf), "Reconnect: %s", SETTINGS.bleBondedDeviceName);
        itemLabels.push_back(buf);
        break;
      }
      case SCAN: itemLabels.push_back("Scan for Devices"); break;
      case LEARN_KEYS: itemLabels.push_back("Learn Page-Turn Keys"); break;
      case CLEAR_KEYS: itemLabels.push_back("Clear Learned Keys"); break;
      case FORGET_DEVICE: itemLabels.push_back("Forget Paired Device"); break;
    }
  }

  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.buttonHintsHeight +
                         metrics.verticalSpacing * 2)},
      menuCount, selectedIndex, [&itemLabels](int index) { return itemLabels[index]; }, nullptr, nullptr,
      nullptr, false);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
#endif // ENABLE_BLE
