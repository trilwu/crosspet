#include "ClockActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <ctime>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
const char* DAY_NAMES[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* MONTH_NAMES[] = {"January",   "February", "March",    "April",
                              "May",       "June",     "July",     "August",
                              "September", "October",  "November", "December"};

bool isTimeValid() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return false;
  return timeinfo.tm_year >= 125;  // Year >= 2025
}
}  // namespace

void ClockActivity::onEnter() {
  Activity::onEnter();
  timeAvailable = isTimeValid();
  lastUpdateMs = millis();
  requestUpdate();
}

void ClockActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // Re-check time validity periodically
  if (!timeAvailable) {
    timeAvailable = isTimeValid();
    if (timeAvailable) requestUpdate();
  }

  // Refresh display every 60 seconds
  if (timeAvailable && millis() - lastUpdateMs >= 60000) {
    lastUpdateMs = millis();
    requestUpdate();
  }
}

void ClockActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Use drawHeader for battery — avoids manual drawBatteryRight positioning bugs
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CLOCK));

  if (!timeAvailable) {
    // Show hint to connect WiFi
    const int y = (pageHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_CONNECT_WIFI_TIME));
  } else {
    struct tm timeinfo;
    getLocalTime(&timeinfo, 0);

    // Format time HH:MM
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    // Format date: "Wednesday, 25 February 2026"
    char dateBuf[64];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %d %s %d", DAY_NAMES[timeinfo.tm_wday], timeinfo.tm_mday,
             MONTH_NAMES[timeinfo.tm_mon], timeinfo.tm_year + 1900);

    // Center time vertically
    const int timeHeight = renderer.getLineHeight(UI_12_FONT_ID);
    const int dateHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int totalHeight = timeHeight + 10 + dateHeight;
    const int startY = (pageHeight - totalHeight) / 2;

    renderer.drawCenteredText(UI_12_FONT_ID, startY, timeBuf, true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, startY + timeHeight + 10, dateBuf);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
