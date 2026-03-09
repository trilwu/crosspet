#include "BleRemotePairingActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "CrossPointSettings.h"
#include "ble/BluetoothHIDManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

// Reference to the global BLE HID manager defined in main.cpp
extern BluetoothHIDManager btHidManager;

// ---- Lifecycle -------------------------------------------------------

void BleRemotePairingActivity::onEnter() {
  Activity::onEnter();

  btHidManager.setPairingActive(true);  // prevent main loop from deinit'ing BLE
  selectedDeviceIndex = 0;
  idleMenuSelection = 0;
  errorMessage.clear();

  // If already paired, show idle state with device info; otherwise jump to scan
  if (SETTINGS.bleBondedDeviceAddr[0] != '\0') {
    state = BleUiState::IDLE;
  } else {
    startScanning();
  }

  requestUpdate();
}

void BleRemotePairingActivity::onExit() {
  Activity::onExit();

  if (btHidManager.isScanning()) {
    btHidManager.stopScan();
  }
  btHidManager.setPairingActive(false);  // allow main loop to manage BLE again
}

// ---- State helpers ---------------------------------------------------

void BleRemotePairingActivity::startScanning() {
  btHidManager.clearDiscoveredDevices();
  selectedDeviceIndex = 0;
  state = BleUiState::SCANNING;
  stateEnteredAt = millis();
  requestUpdate();

  btHidManager.init();
  btHidManager.startScan(20);
}

void BleRemotePairingActivity::connectToSelected() {
  auto devices = btHidManager.getDiscoveredDevices();
  if (selectedDeviceIndex >= (int)devices.size()) return;

  state = BleUiState::CONNECTING;
  stateEnteredAt = millis();
  requestUpdate();

  const auto& device = devices[selectedDeviceIndex];
  LOG_DBG("BLE", "Connecting to %s (%s)", device.name.c_str(), device.address.toString().c_str());

  if (btHidManager.connectToDevice(device.address)) {
    // Persist bonded device info (including address type for reliable reconnect)
    strncpy(SETTINGS.bleBondedDeviceAddr, device.address.toString().c_str(),
            sizeof(SETTINGS.bleBondedDeviceAddr) - 1);
    SETTINGS.bleBondedDeviceAddr[sizeof(SETTINGS.bleBondedDeviceAddr) - 1] = '\0';
    strncpy(SETTINGS.bleBondedDeviceName, device.name.c_str(), sizeof(SETTINGS.bleBondedDeviceName) - 1);
    SETTINGS.bleBondedDeviceName[sizeof(SETTINGS.bleBondedDeviceName) - 1] = '\0';
    SETTINGS.bleBondedDeviceAddrType = device.address.getType();
    SETTINGS.bleEnabled = 1;  // Auto-enable BLE when pairing succeeds
    SETTINGS.saveToFile();

    state = BleUiState::CONNECTED;
    stateEnteredAt = millis();
  } else {
    errorMessage = btHidManager.getLastError();
    if (errorMessage.empty()) errorMessage = "Connection failed";
    state = BleUiState::ERROR;
    LOG_DBG("BLE", "Connection failed: %s", errorMessage.c_str());
  }
  requestUpdate();
}

void BleRemotePairingActivity::forgetDevice() {
  btHidManager.disconnect();
  SETTINGS.bleBondedDeviceAddr[0] = '\0';
  SETTINGS.bleBondedDeviceName[0] = '\0';
  SETTINGS.bleBondedDeviceAddrType = 0;
  SETTINGS.saveToFile();
  startScanning();
}

// ---- Loop (input handling per state) ---------------------------------

void BleRemotePairingActivity::loop() {
  switch (state) {
    case BleUiState::SCANNING: {
      // Poll until scan finishes
      if (!btHidManager.isScanning()) {
        state = BleUiState::SCAN_COMPLETE;
        requestUpdate();
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        btHidManager.stopScan();
        finish();
      }
      // Long-press Confirm → restart scan
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && mappedInput.getHeldTime() > 800) {
        btHidManager.stopScan();
        startScanning();
      }
      return;
    }

    case BleUiState::IDLE: {
      const bool hasPairedDevice = (SETTINGS.bleBondedDeviceAddr[0] != '\0');
      const int menuItems = hasPairedDevice ? 2 : 1;  // scan [+ forget]

      if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        finish();
        return;
      }

      if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        if (idleMenuSelection == 0) {
          startScanning();
        } else {
          forgetDevice();
        }
        return;
      }

      // Navigate menu items
      buttonNavigator.onNext([this, menuItems] {
        idleMenuSelection = ButtonNavigator::nextIndex(idleMenuSelection, menuItems);
        requestUpdate();
      });
      buttonNavigator.onPrevious([this, menuItems] {
        idleMenuSelection = ButtonNavigator::previousIndex(idleMenuSelection, menuItems);
        requestUpdate();
      });
      return;
    }

    case BleUiState::SCAN_COMPLETE: {
      auto devices = btHidManager.getDiscoveredDevices();
      const int deviceCount = (int)devices.size();

      if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        finish();
        return;
      }

      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (deviceCount > 0) {
          if (mappedInput.getHeldTime() > 800) {
            startScanning();  // Long-press Confirm → rescan
          } else {
            connectToSelected();
          }
        } else {
          startScanning();  // No devices → retry scan
        }
        return;
      }

      if (deviceCount > 0) {
        buttonNavigator.onNext([this, deviceCount] {
          selectedDeviceIndex = ButtonNavigator::nextIndex(selectedDeviceIndex, deviceCount);
          requestUpdate();
        });
        buttonNavigator.onPrevious([this, deviceCount] {
          selectedDeviceIndex = ButtonNavigator::previousIndex(selectedDeviceIndex, deviceCount);
          requestUpdate();
        });
      }
      return;
    }

    case BleUiState::CONNECTING:
      // Connecting is blocking (connectToDevice is synchronous); nothing to poll
      return;

    case BleUiState::CONNECTED:
      // Auto-finish after 1.5 s
      if (millis() - stateEnteredAt >= 1500) {
        finish();
      }
      return;

    case BleUiState::ERROR: {
      if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        startScanning();
        return;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        finish();
        return;
      }
      return;
    }
  }
}

// ---- Render ----------------------------------------------------------

std::string BleRemotePairingActivity::getSignalStrengthIndicator(const int rssi) const {
  if (rssi >= -50) return "||||";
  if (rssi >= -60) return " |||";
  if (rssi >= -70) return "  ||";
  return "   |";
}

void BleRemotePairingActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BLE_REMOTE), "");

  switch (state) {
    case BleUiState::IDLE:
      renderIdle();
      break;
    case BleUiState::SCANNING:
      renderScanning();
      break;
    case BleUiState::SCAN_COMPLETE:
      renderScanComplete();
      break;
    case BleUiState::CONNECTING:
      renderConnecting();
      break;
    case BleUiState::CONNECTED:
      renderConnected();
      break;
    case BleUiState::ERROR:
      renderError();
      break;
  }

  renderer.displayBuffer();
}

void BleRemotePairingActivity::renderIdle() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const bool hasPairedDevice = (SETTINGS.bleBondedDeviceAddr[0] != '\0');
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (hasPairedDevice) {
    // Show currently paired device name
    char pairedStr[64];
    snprintf(pairedStr, sizeof(pairedStr), tr(STR_BLE_PAIRED_WITH), SETTINGS.bleBondedDeviceName);

    const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int textTop = contentTop + metrics.verticalSpacing;
    renderer.drawCenteredText(SMALL_FONT_ID, textTop, pairedStr);

    // Simple two-item menu: Scan Again / Forget Device
    const int menuTop = textTop + lineHeight + metrics.verticalSpacing * 2;
    GUI.drawList(renderer, Rect{0, menuTop, pageWidth, contentHeight - (menuTop - contentTop)}, 2,
                 idleMenuSelection,
                 [](int i) -> std::string {
                   if (i == 0) return tr(STR_BLE_PAIR_DEVICE);
                   return tr(STR_BLE_FORGET_DEVICE);
                 },
                 nullptr, nullptr, nullptr);
  } else {
    // No paired device — single action: scan
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, 1, idleMenuSelection,
                 [](int /*i*/) -> std::string { return tr(STR_BLE_PAIR_DEVICE); }, nullptr, nullptr, nullptr);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BleRemotePairingActivity::renderScanning() const {
  const auto pageHeight = renderer.getScreenHeight();
  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int top = (pageHeight - lineHeight) / 2;

  renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_BLE_SCANNING));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BleRemotePairingActivity::renderScanComplete() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  auto devices = btHidManager.getDiscoveredDevices();
  const int deviceCount = (int)devices.size();

  if (deviceCount == 0) {
    const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int top = (pageHeight - lineHeight) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_BLE_NO_DEVICES));
  } else {
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, deviceCount, selectedDeviceIndex,
                 [&devices](int i) {
                   return devices[i].advertisesHid ? devices[i].name + " [HID]" : devices[i].name;
                 },
                 nullptr, nullptr,
                 [this, &devices](int i) {
                   char rssi[16];
                   snprintf(rssi, sizeof(rssi), "%s %ddB", getSignalStrengthIndicator(devices[i].rssi).c_str(),
                            devices[i].rssi);
                   return std::string(rssi);
                 });
  }

  const char* confirmLabel = (deviceCount > 0) ? tr(STR_CONFIRM) : tr(STR_RETRY);
  const char* upLabel = (deviceCount > 0) ? tr(STR_DIR_UP) : "";
  const char* downLabel = (deviceCount > 0) ? tr(STR_DIR_DOWN) : "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, upLabel, downLabel);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BleRemotePairingActivity::renderConnecting() const {
  const auto pageHeight = renderer.getScreenHeight();
  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int top = (pageHeight - lineHeight * 2) / 2;

  renderer.drawCenteredText(UI_12_FONT_ID, top - 20, tr(STR_BLE_CONNECTING), true, EpdFontFamily::BOLD);

  auto devices = btHidManager.getDiscoveredDevices();
  if (selectedDeviceIndex < (int)devices.size()) {
    std::string nameInfo = devices[selectedDeviceIndex].name;
    if (nameInfo.length() > 28) {
      nameInfo.replace(25, nameInfo.length() - 25, "...");
    }
    renderer.drawCenteredText(UI_10_FONT_ID, top + 20, nameInfo.c_str());
  }
}

void BleRemotePairingActivity::renderConnected() const {
  const auto pageHeight = renderer.getScreenHeight();
  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int top = (pageHeight - lineHeight * 2) / 2;

  renderer.drawCenteredText(UI_12_FONT_ID, top - 20, tr(STR_BLE_CONNECTED), true, EpdFontFamily::BOLD);

  if (SETTINGS.bleBondedDeviceName[0] != '\0') {
    renderer.drawCenteredText(UI_10_FONT_ID, top + 20, SETTINGS.bleBondedDeviceName);
  }
}

void BleRemotePairingActivity::renderError() const {
  const auto pageHeight = renderer.getScreenHeight();
  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int top = (pageHeight - lineHeight * 2) / 2;

  renderer.drawCenteredText(UI_12_FONT_ID, top - 20, tr(STR_BLE_DISCONNECTED), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, top + 10, errorMessage.c_str());

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
