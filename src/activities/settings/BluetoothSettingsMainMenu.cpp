#ifdef ENABLE_BLE
// BluetoothSettingsMainMenu.cpp
// Main menu: buildMenuActions / handleMainMenuInput / renderMainMenu
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

// Menu actions — indices are dynamic based on whether bonded device exists
enum MenuAction { TOGGLE_BT = 0, RECONNECT, SCAN, LEARN_KEYS, CLEAR_KEYS, FORGET_DEVICE, DEBUG_MONITOR_ACTION };

// Build dynamic menu: toggle + optional reconnect + scan + learn + clear + optional forget + debug
static int buildMenuActions(bool hasBonded, bool isConnected, MenuAction* actions, int /*maxActions*/) {
  int n = 0;
  actions[n++] = TOGGLE_BT;
  if (hasBonded && !isConnected) actions[n++] = RECONNECT;
  actions[n++] = SCAN;
  actions[n++] = LEARN_KEYS;
  actions[n++] = CLEAR_KEYS;
  if (hasBonded) actions[n++] = FORGET_DEVICE;
  actions[n++] = DEBUG_MONITOR_ACTION;
  return n;
}

void BluetoothSettingsActivity::handleMainMenuInput() {
  const bool hasBonded = strlen(SETTINGS.bleBondedDeviceAddr) > 0;
  const bool isConnected = btMgr && !btMgr->getConnectedDevices().empty();
  MenuAction actions[7];
  const int menuCount = buildMenuActions(hasBonded, isConnected, actions, 7);

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
    case DEBUG_MONITOR_ACTION:
      if (!btMgr->isEnabled()) { lastError = "Enable BT first"; break; }
      if (btMgr->getConnectedDevices().empty()) { lastError = "Connect a device first"; break; }
      viewMode = ViewMode::DEBUG_MONITOR;
      selectedIndex = 0;
      memset(debugRawBytes, 0, sizeof(debugRawBytes));
      debugRawLength = 0;
      debugLastEventMs = 0;
      btMgr->setDebugCaptureEnabled(true);
      lastError = "";
      break;
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

  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    statusLine.c_str(), lastError.empty() ? nullptr : lastError.c_str());

  // Build dynamic menu labels matching handleMainMenuInput actions
  const bool hasBonded = strlen(SETTINGS.bleBondedDeviceAddr) > 0;
  const bool isConnected = btMgr && !btMgr->getConnectedDevices().empty();
  MenuAction actions[7];
  const int menuCount = buildMenuActions(hasBonded, isConnected, actions, 7);

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
      case DEBUG_MONITOR_ACTION: itemLabels.push_back("Debug Monitor"); break;
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
