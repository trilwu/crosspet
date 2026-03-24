#ifdef ENABLE_BLE
// BluetoothSettingsDeviceList.cpp
// Device list scan/connect UI: handleDeviceListInput() + renderDeviceList()
#include "BluetoothSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <cstring>

#include "CrossPointSettings.h"

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BluetoothSettingsActivity::handleDeviceListInput() {
  if (!btMgr) return;

  const auto& devices = btMgr->getDiscoveredDevices();
  const auto& connectedDevices = btMgr->getConnectedDevices();

  // Items: up to 8 devices + "Rescan" + optional "Disconnect All"
  int menuItems = static_cast<int>(devices.size()) + 1;
  if (!connectedDevices.empty()) menuItems++;
  const int maxIndex = menuItems - 1;

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : maxIndex;
    requestUpdate();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectedIndex = (selectedIndex < maxIndex) ? selectedIndex + 1 : 0;
    requestUpdate();
  }

  // Left: back to main menu
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    viewMode = ViewMode::MAIN_MENU;
    selectedIndex = 0;
    if (btMgr->isScanning()) btMgr->stopScan();
    requestUpdate();
    return;
  }

  // Right: quick rescan
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    LOG_INF("BT", "Quick rescan...");
    lastError = "Scanning...";
    btMgr->startScan(10000);
    lastScanTime = millis();
    selectedIndex = 0;
    requestUpdate();
    return;
  }

  if (!mappedInput.wasPressed(MappedInputManager::Button::Confirm)) return;

  const int rescanIndex = static_cast<int>(devices.size());
  const int disconnectIndex = rescanIndex + 1;

  if (selectedIndex == rescanIndex) {
    // Rescan action
    LOG_INF("BT", "Refreshing scan...");
    lastError = "Scanning...";
    btMgr->startScan(10000);
    lastScanTime = millis();
    selectedIndex = 0;
    requestUpdate();
    return;
  }

  if (!connectedDevices.empty() && selectedIndex == disconnectIndex) {
    // Disconnect all — copy to avoid iterator invalidation during disconnect
    LOG_INF("BT", "Disconnecting from all devices...");
    std::vector<std::string> addrs = connectedDevices;
    for (const auto& addr : addrs) {
      LOG_DBG("BT", "Disconnecting %s", addr.c_str());
      btMgr->disconnectFromDevice(addr);
    }
    lastError = "Disconnected";
    selectedIndex = 0;
    requestUpdate();
    return;
  }

  // Connect to selected device — copy info before connect (scan results may be invalidated)
  if (selectedIndex >= 0 && selectedIndex < static_cast<int>(devices.size())) {
    const std::string devAddr = devices[selectedIndex].address;
    const std::string devName = devices[selectedIndex].name;
    const uint8_t devAddrType = devices[selectedIndex].addressType;
    LOG_INF("BT", "Connecting to %s (%s)", devName.c_str(), devAddr.c_str());
    lastError = "Connecting...";
    requestUpdateAndWait();  // Force render before blocking connect

    if (btMgr->connectToDevice(devAddr)) {
      // Persist bonded device in manager and settings
      btMgr->setBondedDevice(devAddr, devName);
      strncpy(SETTINGS.bleBondedDeviceAddr, devAddr.c_str(), sizeof(SETTINGS.bleBondedDeviceAddr) - 1);
      strncpy(SETTINGS.bleBondedDeviceName, devName.c_str(), sizeof(SETTINGS.bleBondedDeviceName) - 1);
      SETTINGS.bleBondedDeviceAddrType = devAddrType;
      SETTINGS.bleEnabled = 1;
      SETTINGS.saveToFile();
      lastError = std::string("Connected to ") + devName;
      LOG_INF("BT", "Successfully connected to %s", devName.c_str());
    } else {
      lastError = btMgr->lastError.empty() ? "Connection failed" : btMgr->lastError;
      LOG_ERR("BT", "Failed to connect: %s", lastError.c_str());
    }
    requestUpdate();
  }
}

void BluetoothSettingsActivity::renderDeviceList() {
  auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  if (!btMgr) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Bluetooth error");
    return;
  }

  const auto& devices = btMgr->getDiscoveredDevices();
  const auto& connectedDevices = btMgr->getConnectedDevices();

  // Header with scan status subtitle
  char countStr[32];
  snprintf(countStr, sizeof(countStr), btMgr->isScanning() ? tr(STR_SCANNING) : "Found %zu", devices.size());
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BLE_REMOTE),
                 countStr);

  std::string subheaderText;
  if (!lastError.empty() && lastError != "Scanning...") {
    subheaderText = lastError;  // Show connection result/error
  } else if (btMgr->isScanning()) {
    subheaderText = "Searching for devices...";
  } else if (devices.empty()) {
    subheaderText = "No devices found";
  } else {
    char buf[64];
    snprintf(buf, sizeof(buf), "%d device(s) available", static_cast<int>(devices.size()));
    subheaderText = buf;
  }
  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    subheaderText.c_str());

  // Build device label/value vectors (cap at 8 devices)
  std::vector<std::string> deviceLabels;
  std::vector<std::string> deviceValues;
  char buf[128];

  for (int i = 0; i < static_cast<int>(devices.size()) && i < 8; i++) {
    const auto& device = devices[i];
    bool connected = btMgr->isConnected(device.address);
    const char* connSym = connected ? "[*] " : "";
    const char* hidSym = device.isHID ? "[HID] " : "";
    // Show device name, or MAC address if name is "Unknown" or empty
    const bool hasName = !device.name.empty() && device.name != "Unknown";
    const char* displayName = hasName ? device.name.c_str() : device.address.c_str();
    snprintf(buf, sizeof(buf), "%s%s%s", connSym, hidSym, displayName);
    deviceLabels.push_back(buf);

    std::string signal = getSignalStrengthIndicator(device.rssi);
    snprintf(buf, sizeof(buf), "%s %ddBm", signal.c_str(), device.rssi);
    deviceValues.push_back(buf);
  }

  // Overflow indicator
  if (static_cast<int>(devices.size()) > 8) {
    deviceLabels.push_back("...");
    deviceValues.push_back("");
  }

  deviceLabels.push_back("< Rescan >");
  deviceValues.push_back("");

  if (!connectedDevices.empty()) {
    deviceLabels.push_back("< Disconnect All >");
    deviceValues.push_back("");
  }

  const int listCount = static_cast<int>(deviceLabels.size());
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing, pageWidth,
           pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.buttonHintsHeight +
                         metrics.verticalSpacing * 2)},
      listCount, selectedIndex, [&deviceLabels](int index) { return deviceLabels[index]; }, nullptr, nullptr,
      [&deviceValues](int i) { return i < static_cast<int>(deviceValues.size()) ? deviceValues[i] : std::string(""); },
      true);

  GUI.drawHelpText(renderer,
                   Rect{0, pageHeight - metrics.buttonHintsHeight - metrics.contentSidePadding - 15, pageWidth, 20},
                   "Up/Down: Select | Right: Rescan");

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONNECT), tr(STR_DIR_LEFT), tr(STR_RETRY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

std::string BluetoothSettingsActivity::getSignalStrengthIndicator(const int32_t rssi) const {
  if (rssi >= -50) return "||||";   // Excellent
  if (rssi >= -60) return " |||";   // Good
  if (rssi >= -70) return "  ||";   // Fair
  return "   |";                    // Very weak
}
#endif // ENABLE_BLE
