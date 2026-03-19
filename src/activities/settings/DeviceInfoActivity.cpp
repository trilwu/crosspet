#include "DeviceInfoActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void DeviceInfoActivity::onEnter() {
  Activity::onEnter();
  needsRefresh = true;
  requestUpdate();
}

void DeviceInfoActivity::loop() {
  // Wait for Confirm button to be fully released before accepting input,
  // prevents carry-over from SettingsActivity's wasPressed(Confirm) trigger
  if (waitForRelease) {
    if (!mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
      waitForRelease = false;
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
    return;
  }

  // Refresh RAM display on any nav button
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
      mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    needsRefresh = true;
    requestUpdate();
  }
}

void DeviceInfoActivity::render(RenderLock&&) {
  if (!needsRefresh) return;
  needsRefresh = false;

  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight},
                 tr(STR_DEVICE_INFO));

  const int startY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 4;
  const int labelX = metrics.contentSidePadding + 4;
  const int valueX = sw / 2 + 10;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  int y = startY;

  auto drawRow = [&](const char* label, const char* value) {
    renderer.drawText(SMALL_FONT_ID, labelX, y, label, true);
    renderer.drawText(SMALL_FONT_ID, valueX, y, value, true, EpdFontFamily::BOLD);
    y += lineH;
  };

  char buf[48];

  // Version
  drawRow(tr(STR_VERSION), CROSSPOINT_VERSION);

  // Chip
  snprintf(buf, sizeof(buf), "%s rev %d", ESP.getChipModel(), ESP.getChipRevision());
  drawRow("Chip", buf);

  // CPU frequency
  snprintf(buf, sizeof(buf), "%d MHz", ESP.getCpuFreqMHz());
  drawRow("CPU", buf);

  // Free heap
  snprintf(buf, sizeof(buf), "%d KB / %d KB", ESP.getFreeHeap() / 1024, ESP.getHeapSize() / 1024);
  drawRow(tr(STR_FREE_RAM), buf);

  // Min free heap (watermark)
  snprintf(buf, sizeof(buf), "%d KB", ESP.getMinFreeHeap() / 1024);
  drawRow("Min Free", buf);

  // Max allocatable block
  snprintf(buf, sizeof(buf), "%d KB", ESP.getMaxAllocHeap() / 1024);
  drawRow("Max Block", buf);

  // Flash
  snprintf(buf, sizeof(buf), "%d MB", ESP.getFlashChipSize() / 1024 / 1024);
  drawRow("Flash", buf);

  // Battery
  snprintf(buf, sizeof(buf), "%d%%", powerManager.getBatteryPercentage());
  drawRow(tr(STR_BATTERY), buf);

  // Uptime
  uint32_t uptimeSec = millis() / 1000;
  if (uptimeSec >= 3600) {
    snprintf(buf, sizeof(buf), "%dh %dm", uptimeSec / 3600, (uptimeSec % 3600) / 60);
  } else {
    snprintf(buf, sizeof(buf), "%dm %ds", uptimeSec / 60, uptimeSec % 60);
  }
  drawRow("Uptime", buf);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT),
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
