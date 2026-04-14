#include "GhostHomeActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>

#include "WifiScannerActivity.h"
#ifdef ENABLE_BLE
#include "BleScannerActivity.h"
#endif

void GhostHomeActivity::onEnter() {
  Activity::onEnter();
  selectedIdx = 0;
  requestUpdate();
}

void GhostHomeActivity::launchSelected() {
  switch (selectedIdx) {
    case 0:
      // WiFi Scanner
      activityManager.pushActivity(std::make_unique<WifiScannerActivity>(renderer, mappedInput));
      break;
    case 1:
      // BLE Scanner — only available when BLE is compiled in
#ifdef ENABLE_BLE
      activityManager.pushActivity(std::make_unique<BleScannerActivity>(renderer, mappedInput));
#endif
      break;
    case 2:
      // Tracker Detection — BLE-based, also guarded by ENABLE_BLE
#ifdef ENABLE_BLE
      {
        auto scanner = std::make_unique<BleScannerActivity>(renderer, mappedInput);
        scanner->setTrackerMode(true);
        activityManager.pushActivity(std::move(scanner));
      }
#endif
      break;
    case 3:
      // Coming Soon — no action
      break;
    default:
      break;
  }
}

void GhostHomeActivity::loop() {
  buttonNavigator.onNext([this] {
    selectedIdx = ButtonNavigator::nextIndex(selectedIdx, TOOL_COUNT);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectedIdx = ButtonNavigator::previousIndex(selectedIdx, TOOL_COUNT);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    launchSelected();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void GhostHomeActivity::render(RenderLock&&) {
  renderer.clearScreen();

  // Draw full ctOS chrome: title bar + corner brackets
  ctos.drawCtosFrame("CROSSPET", "GHOST MODE v0.1");

  // Draw 2x2 tool grid with current selection highlighted
  ctos.drawToolGrid(TOOL_LABELS, TOOL_COUNT, selectedIdx);

  // Status bar: system health + free heap
  char statusBuf[64];
  snprintf(statusBuf, sizeof(statusBuf), "SYS: OK | HEAP: %dKB | SD: mounted",
           static_cast<int>(ESP.getFreeHeap() / 1024));
  const int statusY = renderer.getScreenHeight() - 24;
  ctos.drawStatusBar(statusY, statusBuf);

  renderer.displayBuffer();
}
