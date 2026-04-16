#ifdef ENABLE_BLE
// BluetoothSettingsDebugMonitor.cpp
// Debug HID monitor: handleDebugMonitorInput() + renderDebugMonitor()
#include "BluetoothSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BluetoothSettingsActivity::handleDebugMonitorInput() {
  // The input callback already captures keycode to pendingLearnKey
  // Just update debug display when new data arrives
  if (pendingLearnKey != 0) {
    debugLastEventMs = millis();
    pendingLearnKey = 0;
    requestUpdate();
  }
}

void BluetoothSettingsActivity::renderDebugMonitor() {
  auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "HID Debug Monitor");

  // Connection status
  auto connDevs = btMgr ? btMgr->getConnectedDevices() : std::vector<std::string>();
  const char* connStatus = connDevs.empty() ? "No device connected" : "Monitoring...";
  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    connStatus);

  const int bodyY = metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing;

  // Last event time
  char timeBuf[48];
  if (debugLastEventMs > 0) {
    unsigned long ago = (millis() - debugLastEventMs);
    snprintf(timeBuf, sizeof(timeBuf), "Last event: %lu ms ago", ago);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "Waiting for HID input...");
  }
  renderer.drawCenteredText(UI_12_FONT_ID, bodyY + 24, timeBuf);

  // Heap info
  char heapBuf[32];
  snprintf(heapBuf, sizeof(heapBuf), "Free heap: %dKB", ESP.getFreeHeap() / 1024);
  renderer.drawCenteredText(UI_10_FONT_ID, bodyY + 56, heapBuf);

  // Connected device count
  char devBuf[48];
  snprintf(devBuf, sizeof(devBuf), "Connected devices: %zu", connDevs.size());
  renderer.drawCenteredText(UI_10_FONT_ID, bodyY + 80, devBuf);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
#endif // ENABLE_BLE
