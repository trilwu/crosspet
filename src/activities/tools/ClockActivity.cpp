#include "ClockActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <sys/time.h>

#include <ctime>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
const char* DAY_NAMES[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* MONTH_NAMES[] = {"January",   "February", "March",    "April",
                              "May",       "June",     "July",     "August",
                              "September", "October",  "November", "December"};
const char* FIELD_LABELS[] = {"Hour", "Minute", "Day", "Month", "Year"};
static constexpr int FIELD_COUNT = 5;

bool isTimeValid() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return false;
  return timeinfo.tm_year >= 125;  // Year >= 2025
}

int daysInMonth(int month, int year) {
  // month 0-based (tm_mon), year = tm_year (years since 1900)
  if (month == 1) {
    int y = year + 1900;
    return ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : 28;
  }
  static const int days[] = {31,0,31,30,31,30,31,31,30,31,30,31};
  return days[month];
}
}  // namespace

void ClockActivity::applyEditedTime() {
  // Clamp day to valid range for the month
  int maxDay = daysInMonth(editTime.tm_mon, editTime.tm_year);
  if (editTime.tm_mday > maxDay) editTime.tm_mday = maxDay;

  struct timeval tv;
  tv.tv_sec = mktime(&editTime);
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

void ClockActivity::onEnter() {
  Activity::onEnter();
  editing = false;
  editField = 0;
  timeAvailable = isTimeValid();
  lastUpdateMs = millis();
  requestUpdate();
}

void ClockActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (editing) {
      editing = false;
      timeAvailable = isTimeValid();
      requestUpdate();
    } else {
      finish();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!editing) {
      // Enter edit mode — seed with current time or default
      if (timeAvailable) {
        getLocalTime(&editTime, 0);
      } else {
        editTime = {};
        editTime.tm_year = 125;  // 2025
        editTime.tm_mon  = 0;
        editTime.tm_mday = 1;
      }
      editField = 0;
      editing = true;
    } else {
      // Save and exit edit mode
      applyEditedTime();
      editing = false;
      timeAvailable = true;
      lastUpdateMs = millis();
    }
    requestUpdate();
    return;
  }

  if (editing) {
    bool changed = false;

    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      editField = (editField - 1 + FIELD_COUNT) % FIELD_COUNT;
      changed = true;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      editField = (editField + 1) % FIELD_COUNT;
      changed = true;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      switch (editField) {
        case 0: editTime.tm_hour = (editTime.tm_hour + 1) % 24; break;
        case 1: editTime.tm_min  = (editTime.tm_min  + 1) % 60; break;
        case 2: { int max = daysInMonth(editTime.tm_mon, editTime.tm_year);
                  editTime.tm_mday = editTime.tm_mday % max + 1; break; }
        case 3: editTime.tm_mon  = (editTime.tm_mon  + 1) % 12; break;
        case 4: editTime.tm_year = (editTime.tm_year < 200) ? editTime.tm_year + 1 : 125; break;
      }
      changed = true;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      switch (editField) {
        case 0: editTime.tm_hour = (editTime.tm_hour + 23) % 24; break;
        case 1: editTime.tm_min  = (editTime.tm_min  + 59) % 60; break;
        case 2: { int max = daysInMonth(editTime.tm_mon, editTime.tm_year);
                  editTime.tm_mday = (editTime.tm_mday - 2 + max) % max + 1; break; }
        case 3: editTime.tm_mon  = (editTime.tm_mon  + 11) % 12; break;
        case 4: editTime.tm_year = (editTime.tm_year > 125) ? editTime.tm_year - 1 : 125; break;
      }
      changed = true;
    }

    if (changed) requestUpdate();
    return;
  }

  // Normal clock mode: re-check time validity
  if (!timeAvailable) {
    timeAvailable = isTimeValid();
    if (timeAvailable) requestUpdate();
  }

  // Refresh every 60 seconds
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
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CLOCK));

  if (editing) {
    // --- Edit mode ---
    const int timeH = renderer.getLineHeight(UI_12_FONT_ID);
    const int dateH = renderer.getLineHeight(UI_10_FONT_ID);
    const int labelH = renderer.getLineHeight(SMALL_FONT_ID);
    const int totalH = timeH + 12 + dateH + 12 + labelH;
    const int startY = (pageHeight - totalH) / 2;

    // Time row: HH:MM with active field highlighted
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", editTime.tm_hour, editTime.tm_min);
    renderer.drawCenteredText(UI_12_FONT_ID, startY, timeBuf, true, EpdFontFamily::BOLD);

    // Date row
    char dateBuf[64];
    snprintf(dateBuf, sizeof(dateBuf), "%d %s %d",
             editTime.tm_mday, MONTH_NAMES[editTime.tm_mon], editTime.tm_year + 1900);
    renderer.drawCenteredText(UI_10_FONT_ID, startY + timeH + 12, dateBuf);

    // Active field label
    char fieldBuf[32];
    snprintf(fieldBuf, sizeof(fieldBuf), "< %s >", FIELD_LABELS[editField]);
    renderer.drawCenteredText(SMALL_FONT_ID, startY + timeH + 12 + dateH + 12, fieldBuf);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  } else if (!timeAvailable) {
    const int y = (pageHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_CONNECT_WIFI_TIME));

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Set Time", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  } else {
    struct tm timeinfo;
    getLocalTime(&timeinfo, 0);

    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    char dateBuf[64];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %d %s %d",
             DAY_NAMES[timeinfo.tm_wday], timeinfo.tm_mday,
             MONTH_NAMES[timeinfo.tm_mon], timeinfo.tm_year + 1900);

    const int timeHeight = renderer.getLineHeight(UI_12_FONT_ID);
    const int dateHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int cellH = renderer.getLineHeight(SMALL_FONT_ID) + 10;
    // calendar: 1 header row + max 6 body rows
    const int calH = cellH * 7;

    // Center the whole block (time + date + calendar) in content area
    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentBot = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
    const int blockH = timeHeight + 8 + dateHeight + 16 + calH;
    const int startY = contentTop + (contentBot - contentTop - blockH) / 2;

    renderer.drawCenteredText(UI_12_FONT_ID, startY, timeBuf, true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, startY + timeHeight + 8, dateBuf);
    renderCalendar(startY + timeHeight + 8 + dateHeight + 16, timeinfo);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Set Time", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}

void ClockActivity::renderCalendar(int startY, const struct tm& t) const {
  static const char* const DAY_HDR[] = {"S", "M", "T", "W", "T", "F", "S"};
  const int pageWidth = renderer.getScreenWidth();
  const int margin = 20;
  const int calW = pageWidth - margin * 2;
  const int cellW = calW / 7;
  const int cellH = renderer.getLineHeight(SMALL_FONT_ID) + 10;
  const int x0 = margin;

  // Day-of-week header row
  for (int d = 0; d < 7; d++) {
    int tw = renderer.getTextWidth(SMALL_FONT_ID, DAY_HDR[d]);
    renderer.drawText(SMALL_FONT_ID, x0 + d * cellW + (cellW - tw) / 2, startY, DAY_HDR[d]);
  }

  // First weekday of this month
  struct tm fm = t;
  fm.tm_mday = 1;
  fm.tm_hour = 0; fm.tm_min = 0; fm.tm_sec = 0;
  mktime(&fm);

  int col = fm.tm_wday;  // 0=Sun
  int maxDay = daysInMonth(t.tm_mon, t.tm_year);
  int rowY = startY + cellH;

  for (int day = 1; day <= maxDay; day++) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%d", day);
    int tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
    int cx = x0 + col * cellW;
    int tx = cx + (cellW - tw) / 2;

    if (day == t.tm_mday) {
      // Filled box highlight for today
      renderer.fillRect(cx + 2, rowY - 2, cellW - 4, cellH - 2);
      renderer.drawText(SMALL_FONT_ID, tx, rowY, buf, false);  // white text
    } else {
      renderer.drawText(SMALL_FONT_ID, tx, rowY, buf);
    }

    if (++col == 7) { col = 0; rowY += cellH; }
  }
}
