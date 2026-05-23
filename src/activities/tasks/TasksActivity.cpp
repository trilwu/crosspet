#include "TasksActivity.h"

#include "Logging.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/settings/TasksSettingsActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "tasks/TasksConfig.h"
#include "tasks/TaskProvider.h"

#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

// English month and day names. Kept local so adding the Tasks app doesn't
// force 19 new i18n keys for date rendering. If a localized variant is
// needed later, lift into I18n.
constexpr const char* kMonthNames[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"};
constexpr const char* kDayNames[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Unix epoch sentinel: anything above this is "clock has been set by NTP
// or similar." Matches the threshold WeatherActivity uses for its own
// time-validity gate.
constexpr time_t kClockSetThreshold = 1700000000;  // ~Nov 2023

// Natural compare: digit runs are compared as integers so "task 2" < "task 10"
// instead of plain ASCII order which puts "10" before "2".
int naturalCmp(const char* a, const char* b) {
  while (*a && *b) {
    unsigned char ca = static_cast<unsigned char>(*a);
    unsigned char cb = static_cast<unsigned char>(*b);
    if (ca >= '0' && ca <= '9' && cb >= '0' && cb <= '9') {
      unsigned long va = 0, vb = 0;
      while (*a >= '0' && *a <= '9') { va = va * 10 + static_cast<unsigned long>(*a - '0'); ++a; }
      while (*b >= '0' && *b <= '9') { vb = vb * 10 + static_cast<unsigned long>(*b - '0'); ++b; }
      if (va != vb) return va < vb ? -1 : 1;
    } else {
      if (ca != cb) return ca < cb ? -1 : 1;
      ++a; ++b;
    }
  }
  if (*a == *b) return 0;
  return *a ? 1 : -1;
}

}  // namespace

void TasksActivity::onEnter() {
  Activity::onEnter();
  _entryOrientation = renderer.getOrientation();
  renderer.setOrientation(TASKS_CONFIG.getActivityOrientation());

  _scrollOffset = 0;
  _selectedIndex = 0;
  _lastVisibleIndex = -1;
  _tasks.clear();
  _today[0] = '\0';

  if (!TASKS_CONFIG.hasToken()) {
    _state = State::Setup;
    requestUpdate(true);
    return;
  }

  _state = State::Loading;
  requestUpdate(true);
  startFetch();
}

void TasksActivity::onExit() {
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
  renderer.setOrientation(_entryOrientation);
  Activity::onExit();
}

void TasksActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // Confirm = Refresh (ShowingTasks/ShowingError) OR Retry from Setup (re-load
  // config in case the user populated the token on the SD card).
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (_state == State::Setup) {
      TASKS_CONFIG.load();
      if (TASKS_CONFIG.hasToken()) {
        _state = State::Loading;
        requestUpdate(true);
        startFetch();
      }
      return;
    }
    if (_state == State::ShowingTasks || _state == State::ShowingError) {
      TASKS_CONFIG.load();  // re-check token
      if (!TASKS_CONFIG.hasToken()) {
        _state = State::Setup;
        requestUpdate(true);
        return;
      }
      _state = State::Loading;
      _scrollOffset = 0;
      _tasks.clear();
      requestUpdate(true);
      startFetch();
      return;
    }
  }

  // Left or Right (front buttons) open Tasks Settings. Mirrors WeatherActivity's
  // pattern of using front buttons to enter a submenu while side buttons drive
  // scroll. Available in every state — including Setup, so users without a
  // token can still adjust display preferences.
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
      mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    startActivityForResult(
        std::make_unique<TasksSettingsActivity>(renderer, mappedInput),
        [this](const ActivityResult&) {
          // Restore orientation (settings may have changed it) and re-render.
          renderer.setOrientation(TASKS_CONFIG.getActivityOrientation());
          // Forget may have wiped the token while we were in settings —
          // drop back to the setup screen instead of stale-rendering tasks.
          if (!TASKS_CONFIG.hasToken()) {
            _state = State::Setup;
            _tasks.clear();
            _scrollOffset = 0;
            _selectedIndex = 0;
          }
          requestUpdate(true);
        });
    return;
  }

  // Up/Down (side buttons) scroll the task list. ShowingTasks only.
  if (_state != State::ShowingTasks) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (_selectedIndex + 1 >= static_cast<int>(_tasks.size())) return;
    ++_selectedIndex;
    if (_lastVisibleIndex >= 0 && _selectedIndex > _lastVisibleIndex) {
      ++_scrollOffset;
    }
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (_selectedIndex == 0) return;
    --_selectedIndex;
    if (_selectedIndex < _scrollOffset) _scrollOffset = _selectedIndex;
    requestUpdate();
  }
}

void TasksActivity::startFetch() {
  if (WiFi.status() == WL_CONNECTED) {
    proceedWithFetch();
    return;
  }
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
      [this](const ActivityResult& result) {
        if (result.isCancelled || WiFi.status() != WL_CONNECTED) {
          _state = State::ShowingError;
          _errorStrId = StrId::STR_TASKS_OFFLINE;
          requestUpdate();
          return;
        }
        renderer.setOrientation(TASKS_CONFIG.getActivityOrientation());
        // Allow lwIP's DNS resolver to initialize after WiFi associates —
        // without this getaddrinfo() returns EAI_FAIL (202) for the first
        // request on a fresh connection. Mirrors WeatherActivity::silentRefresh.
        delay(500);
        proceedWithFetch();
      });
}

void TasksActivity::proceedWithFetch() {
  using tasks::FetchResult;
  FetchResult r = TASKS_CONFIG.getActiveProvider().fetch(
      TASKS_CONFIG.getFilter(), _tasks);

  if (r != FetchResult::Ok) {
    _state = State::ShowingError;
    _errorStrId = fetchResultToStrId();
    requestUpdate(true);
    return;
  }

  // Capture local time AT fetch. If clock isn't synced (time_t < threshold),
  // leave _today empty so renderDaily knows to suppress the date header.
  time_t now = time(nullptr);
  if (now >= kClockSetThreshold) {
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    _capturedHour = static_cast<uint8_t>(tm_now.tm_hour);
    _capturedMin = static_cast<uint8_t>(tm_now.tm_min);
    _capturedDay = static_cast<uint8_t>(tm_now.tm_mday);
    _capturedMonth = static_cast<uint8_t>(tm_now.tm_mon + 1);
    _capturedYear = static_cast<uint16_t>(tm_now.tm_year + 1900);
    _capturedDow = static_cast<uint8_t>(tm_now.tm_wday);
    strftime(_today, sizeof(_today), "%Y-%m-%d", &tm_now);
  }

  // Sort by dueDate first (past → today → future, lexicographic on YYYY-MM-DD
  // == chronological), then by title within the same date group.
  std::sort(_tasks.begin(), _tasks.end(),
            [](const tasks::Task& a, const tasks::Task& b) {
              int dateCmp = strcmp(a.dueDate, b.dueDate);
              if (dateCmp != 0) return dateCmp < 0;
              return naturalCmp(a.title, b.title) < 0;
            });

  _state = State::ShowingTasks;
  _scrollOffset = 0;
  _selectedIndex = 0;
  _lastVisibleIndex = -1;
  requestUpdate(true);
}

StrId TasksActivity::fetchResultToStrId() const {
  // Generic error UI: distinguish auth from "everything else" — the user
  // action is different (fix token vs. retry).
  // Kept minimal because we don't want a separate i18n string per error
  // bucket until we know which ones users actually hit.
  return StrId::STR_TASKS_FETCH_FAILED;
}

void TasksActivity::render(RenderLock&&) {
  switch (_state) {
    case State::Setup:        renderSetup();    break;
    case State::Loading:      renderLoading();  break;
    case State::ShowingError: renderError();    break;
    case State::ShowingTasks: renderTaskList(); break;
  }
  renderer.displayBuffer();
}

void TasksActivity::renderSetup() {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TASKS));

  // Multi-line setup instruction. drawPopup is single-line and the JSON path
  // doesn't fit, so render manually with wrap.
  constexpr int kSidePadding = 30;
  const int maxTextWidth = pageWidth - 2 * kSidePadding;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  auto lines = renderer.wrappedText(UI_12_FONT_ID, tr(STR_TASKS_TOKEN_SETUP_HINT), maxTextWidth, 8);

  int y = metrics.topPadding + metrics.headerHeight + 40;
  for (const auto& line : lines) {
    const int w = renderer.getTextWidth(UI_12_FONT_ID, line.c_str());
    renderer.drawText(UI_12_FONT_ID, (pageWidth - w) / 2, y, line.c_str(), true);
    y += lineHeight + 4;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY),
                                            tr(STR_SETTINGS_TITLE), tr(STR_SETTINGS_TITLE));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void TasksActivity::renderLoading() {
  renderer.clearScreen();
  GUI.drawPopup(renderer, tr(STR_TASKS_FETCHING));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void TasksActivity::renderError() {
  renderer.clearScreen();
  GUI.drawPopup(renderer, I18N.get(_errorStrId));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY),
                                            tr(STR_SETTINGS_TITLE), tr(STR_SETTINGS_TITLE));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void TasksActivity::renderTaskList() {
  if (TASKS_CONFIG.getDesignMode() == tasks::DesignMode::Daily) {
    renderDaily();
  } else {
    renderMinimal();
  }
}

void TasksActivity::renderMinimal() {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const auto orientation = renderer.getOrientation();
  const bool isLandscape = (orientation == GfxRenderer::LandscapeClockwise ||
                            orientation == GfxRenderer::LandscapeCounterClockwise);
  const bool hintOnLeft = (orientation == GfxRenderer::LandscapeClockwise);
  const int hintReserve = metrics.buttonHintsHeight + metrics.verticalSpacing * 2;
  const int hintLeftReserve = (isLandscape && hintOnLeft) ? hintReserve : 0;
  const int hintRightReserve = (isLandscape && !hintOnLeft) ? hintReserve : 0;

  // Timestamp header. If the clock isn't set, just show the title — no
  // garbage "00:00" date string.
  char timestamp[32] = "";
  if (_today[0] != '\0') {
    char dateStr[8];
    tasks::formatDate(_capturedDay, _capturedMonth, TASKS_CONFIG.getDateFormat(),
                      dateStr, sizeof(dateStr));
    snprintf(timestamp, sizeof(timestamp), "%s  %02u:%02u",
             dateStr,
             static_cast<unsigned>(_capturedHour),
             static_cast<unsigned>(_capturedMin));
  }
  GUI.drawHeader(
      renderer,
      Rect{hintLeftReserve, metrics.topPadding,
           pageWidth - hintLeftReserve - hintRightReserve, metrics.headerHeight},
      tr(STR_TASKS), timestamp);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY),
                                            tr(STR_SETTINGS_TITLE), tr(STR_SETTINGS_TITLE));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (_tasks.empty()) {
    GUI.drawPopup(renderer, tr(STR_TASKS_NO_TASKS));
    return;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - (isLandscape ? 0 : hintReserve);
  const int sidePadding = metrics.contentSidePadding;
  const int tileX = sidePadding + hintLeftReserve;
  const int tileWidth = pageWidth - sidePadding * 2 - hintLeftReserve - hintRightReserve;
  drawTaskRows(contentTop, contentHeight, tileX, tileWidth);
}

void TasksActivity::renderDaily() {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const auto orientation = renderer.getOrientation();
  const bool isLandscape = (orientation == GfxRenderer::LandscapeClockwise ||
                            orientation == GfxRenderer::LandscapeCounterClockwise);
  const bool hintOnLeft = (orientation == GfxRenderer::LandscapeClockwise);
  const int hintReserve = metrics.buttonHintsHeight + metrics.verticalSpacing * 2;
  const int hintLeftReserve = (isLandscape && hintOnLeft) ? hintReserve : 0;
  const int hintRightReserve = (isLandscape && !hintOnLeft) ? hintReserve : 0;

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY),
                                            tr(STR_SETTINGS_TITLE), tr(STR_SETTINGS_TITLE));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  const int leftEdge = hintLeftReserve;
  const int rightEdge = pageWidth - hintRightReserve;
  const int columnWidth = rightEdge - leftEdge;
  int y = metrics.topPadding + metrics.verticalSpacing * 2;

  // Date + day-of-week header, only when the clock is set. Without it,
  // we'd render bogus "Thursday, January 1, 1970" — better to suppress
  // and let the tasks fill the screen.
  if (_today[0] != '\0') {
    const int monthIdx = (_capturedMonth >= 1 && _capturedMonth <= 12) ? _capturedMonth - 1 : 0;
    char dateLine[32];
    snprintf(dateLine, sizeof(dateLine), "%s %u, %u",
             kMonthNames[monthIdx], static_cast<unsigned>(_capturedDay),
             static_cast<unsigned>(_capturedYear));
    const int dw = renderer.getTextWidth(UI_12_FONT_ID, dateLine);
    renderer.drawText(UI_12_FONT_ID, leftEdge + (columnWidth - dw) / 2, y, dateLine, true);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 4;

    const int dowIdx = (_capturedDow <= 6) ? _capturedDow : 0;
    const char* dow = kDayNames[dowIdx];
    const int dowW = renderer.getTextWidth(LEXEND_18_FONT_ID, dow, EpdFontFamily::BOLD);
    renderer.drawText(LEXEND_18_FONT_ID,
                      leftEdge + (columnWidth - dowW) / 2, y, dow, true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(LEXEND_18_FONT_ID) + 8;

    // Divider between header and task list.
    renderer.drawLine(leftEdge + 12, y, rightEdge - 12, y, true);
    y += 6;
  }

  // Bottom "Updated dd/mm HH:MM" line + divider above it. Mirrors the
  // top header sandwich. Suppressed when no clock.
  int contentBottom = pageHeight - (isLandscape ? 0 : hintReserve);
  if (_today[0] != '\0') {
    char dateStr[8];
    tasks::formatDate(_capturedDay, _capturedMonth, TASKS_CONFIG.getDateFormat(),
                      dateStr, sizeof(dateStr));
    char updatedLine[32];
    snprintf(updatedLine, sizeof(updatedLine), "%s %s  %02u:%02u",
             tr(STR_TASKS_UPDATED), dateStr,
             static_cast<unsigned>(_capturedHour),
             static_cast<unsigned>(_capturedMin));
    const int updatedH = renderer.getLineHeight(SMALL_FONT_ID);
    constexpr int kBottomPadding = 4;
    constexpr int kBottomDividerGap = 6;
    const int updatedY = pageHeight - (isLandscape ? 0 : hintReserve) - updatedH - kBottomPadding;
    const int bottomDividerY = updatedY - kBottomDividerGap;
    renderer.drawLine(leftEdge + 12, bottomDividerY, rightEdge - 12, bottomDividerY, true);
    const int uw = renderer.getTextWidth(SMALL_FONT_ID, updatedLine);
    renderer.drawText(SMALL_FONT_ID, leftEdge + (columnWidth - uw) / 2, updatedY, updatedLine, true);
    contentBottom = bottomDividerY - 4;
  }

  if (_tasks.empty()) {
    GUI.drawPopup(renderer, tr(STR_TASKS_NO_TASKS));
    return;
  }

  const int contentTop = y;
  const int contentHeight = std::max(0, contentBottom - contentTop);
  const int sidePadding = metrics.contentSidePadding;
  const int tileX = sidePadding + hintLeftReserve;
  const int tileWidth = pageWidth - sidePadding * 2 - hintLeftReserve - hintRightReserve;
  drawTaskRows(contentTop, contentHeight, tileX, tileWidth);
}

void TasksActivity::drawTaskRows(int contentTop, int contentHeight, int tileX, int tileWidth) {
  constexpr const char* kBulletNormal = "\xE2\x80\xA2";   // U+2022 BULLET
  constexpr const char* kBulletOverdue = "!";
  constexpr const char* kBulletFuture = "\xE2\x80\xBA";   // U+203A
  constexpr int kTilePaddingX = 10;
  constexpr int kTilePaddingY = 6;
  constexpr int kBulletGap = 8;
  constexpr int kRowGap = 2;
  constexpr int kCursorRadius = 6;
  constexpr int kDateGap = 8;
  constexpr int kMaxLines = 2;

  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int bulletColWidth = std::max({
      renderer.getTextWidth(UI_10_FONT_ID, kBulletNormal),
      renderer.getTextWidth(UI_10_FONT_ID, kBulletOverdue),
      renderer.getTextWidth(UI_10_FONT_ID, kBulletFuture)});
  const int dateColWidth = renderer.getTextWidth(UI_10_FONT_ID, "00/00");

  const int textX = tileX + kTilePaddingX + bulletColWidth + kBulletGap;
  const int textWidth = tileX + tileWidth - kTilePaddingX - textX;

  const int totalTasks = static_cast<int>(_tasks.size());
  if (_scrollOffset > totalTasks - 1) _scrollOffset = std::max(0, totalTasks - 1);
  if (_selectedIndex > totalTasks - 1) _selectedIndex = std::max(0, totalTasks - 1);

  int y = contentTop;
  int rendered = 0;
  int lastFullyVisible = -1;
  for (int taskIdx = _scrollOffset; taskIdx < totalTasks; ++taskIdx) {
    const auto& t = _tasks[taskIdx];

    const bool isFuture = _today[0] != '\0' && t.dueDate[0] != '\0' &&
                          strcmp(t.dueDate, _today) > 0;
    const bool showDate = t.dueDate[0] != '\0';

    char fullTitle[128];
    snprintf(fullTitle, sizeof(fullTitle), "%s%s%s",
             t.dueTime[0] ? t.dueTime : "",
             t.dueTime[0] ? "  " : "",
             t.title);

    const int rowTextWidth = showDate ? textWidth - dateColWidth - kDateGap : textWidth;
    auto lines = renderer.wrappedText(UI_10_FONT_ID, fullTitle, rowTextWidth, kMaxLines);
    const int textBlockHeight = static_cast<int>(lines.size()) * lineHeight;
    const int tileHeight = textBlockHeight + kTilePaddingY * 2;
    if (y + tileHeight > contentTop + contentHeight) break;

    const bool selected = (taskIdx == _selectedIndex);
    if (selected) {
      renderer.fillRoundedRect(tileX, y, tileWidth, tileHeight, kCursorRadius, Color::LightGray);
    }

    const char* marker = t.overdue   ? kBulletOverdue
                         : isFuture  ? kBulletFuture
                                     : kBulletNormal;
    const int markerX = tileX + kTilePaddingX;
    const int firstLineY = y + kTilePaddingY;
    renderer.drawText(UI_10_FONT_ID, markerX, firstLineY, marker, true);

    for (size_t li = 0; li < lines.size(); ++li) {
      renderer.drawText(UI_10_FONT_ID, textX,
                        firstLineY + static_cast<int>(li) * lineHeight,
                        lines[li].c_str(), true);
    }

    if (showDate) {
      const int day = (t.dueDate[8] - '0') * 10 + (t.dueDate[9] - '0');
      const int mon = (t.dueDate[5] - '0') * 10 + (t.dueDate[6] - '0');
      char dateBuf[8];
      tasks::formatDate(day, mon, TASKS_CONFIG.getDateFormat(), dateBuf, sizeof(dateBuf));
      const int dateWidth = renderer.getTextWidth(UI_10_FONT_ID, dateBuf);
      const int dateX = tileX + tileWidth - kTilePaddingX - dateWidth;
      renderer.drawText(UI_10_FONT_ID, dateX, firstLineY, dateBuf, true);
    }

    y += tileHeight + kRowGap;
    rendered++;
    lastFullyVisible = taskIdx;
  }
  _lastVisibleIndex = lastFullyVisible;

  // Scroll bar
  if (rendered < totalTasks - _scrollOffset || _scrollOffset > 0) {
    const int barX = tileX + tileWidth + 1;
    const int barTrackHeight = contentHeight;
    const int barHeight = std::max(8, (barTrackHeight * rendered) / totalTasks);
    const int maxOffset = std::max(1, totalTasks - rendered);
    const int barY = contentTop + ((barTrackHeight - barHeight) * _scrollOffset) / maxOffset;
    renderer.fillRect(barX, barY, 2, barHeight, true);
  }
}
