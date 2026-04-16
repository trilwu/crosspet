#ifdef ENABLE_BLE
// BluetoothSettingsActivity.cpp
// Lifecycle dispatch: onEnter / onExit / loop / render
#include "BluetoothSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"

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
  btMgr = &BluetoothHIDManager::getInstance();
  LOG_INF("BT", "BluetoothHIDManager ready");

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
    } else if (viewMode == ViewMode::DEBUG_MONITOR) {
      viewMode = ViewMode::MAIN_MENU;
      selectedIndex = 0;
      btMgr->setDebugCaptureEnabled(false);
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
  } else if (viewMode == ViewMode::DEBUG_MONITOR) {
    handleDebugMonitorInput();
  } else {
    handleLearnInput();
  }
}

void BluetoothSettingsActivity::render(RenderLock&& lock) {
  if (viewMode == ViewMode::MAIN_MENU) {
    renderMainMenu();
  } else if (viewMode == ViewMode::DEVICE_LIST) {
    renderDeviceList();
  } else if (viewMode == ViewMode::DEBUG_MONITOR) {
    renderDebugMonitor();
  } else {
    renderLearnKeys();
  }
}
#endif // ENABLE_BLE
