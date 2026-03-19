#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "BookStats.h"
#include "ReadingStats.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/StringUtils.h"

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void ReadingStatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

// ── Render ────────────────────────────────────────────────────────────────────

void ReadingStatsActivity::render(RenderLock&&) {
  const int pageWidth  = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  constexpr int MARGIN = 28;

  renderer.clearScreen();

  const int lhSmall = renderer.getLineHeight(SMALL_FONT_ID);
  const int lhUi10  = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhUi12  = renderer.getLineHeight(UI_12_FONT_ID);
  const int sepMargin = MARGIN + 20;
  const int sepW = pageWidth - 2 * sepMargin;

  // Header: "Reading Stats"
  int y = 24;
  renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_READING_STATS), true, EpdFontFamily::BOLD);
  y += lhUi12 + 6;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- TODAY ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_TODAY));
  y += lhSmall + 4;

  char todayBuf[32];
  StringUtils::formatReadingDuration(todayBuf, sizeof(todayBuf), READ_STATS.todayReadSeconds);
  renderer.drawText(UI_10_FONT_ID, MARGIN, y, todayBuf, true, EpdFontFamily::BOLD);
  y += lhUi10 + 14;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- ALL TIME ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_ALL_TIME));
  y += lhSmall + 4;

  char totalBuf[32];
  StringUtils::formatReadingDuration(totalBuf, sizeof(totalBuf), READ_STATS.totalReadSeconds);
  renderer.drawText(UI_10_FONT_ID, MARGIN, y, totalBuf, true, EpdFontFamily::BOLD);
  y += lhUi10 + 14;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- STATS GRID: Sessions | Books | Streak | Best ---
  {
    const int colW = (pageWidth - MARGIN * 2) / 4;
    const char* labels[] = {tr(STR_STATS_SESSIONS), tr(STR_STATS_BOOKS_DONE),
                             tr(STR_STATS_STREAK), tr(STR_STATS_BEST_STREAK)};
    char values[4][16];
    snprintf(values[0], sizeof(values[0]), "%u", (unsigned)READ_STATS.totalSessions);
    snprintf(values[1], sizeof(values[1]), "%u", (unsigned)READ_STATS.booksFinished);
    snprintf(values[2], sizeof(values[2]), tr(STR_STATS_DAYS), (unsigned)READ_STATS.currentStreak);
    snprintf(values[3], sizeof(values[3]), tr(STR_STATS_DAYS), (unsigned)READ_STATS.longestStreak);

    // Card outline around the stats grid
    const int cardPad = 8;
    const int cardH = lhUi10 + lhSmall + 8 + cardPad * 2;
    renderer.drawRoundedRect(MARGIN - cardPad, y - cardPad, pageWidth - MARGIN * 2 + cardPad * 2, cardH, 1, 10, true);

    // Thin vertical dividers between columns
    for (int i = 1; i < 4; i++) {
      const int divX = MARGIN + i * colW;
      renderer.drawLine(divX, y - cardPad + 4, divX, y + lhUi10 + lhSmall + 8 + cardPad - 4);
    }

    for (int i = 0; i < 4; i++) {
      const int cx = MARGIN + i * colW + colW / 2;
      const int vw = renderer.getTextWidth(UI_10_FONT_ID, values[i]);
      renderer.drawText(UI_10_FONT_ID, cx - vw / 2, y, values[i], true, EpdFontFamily::BOLD);
      const int lw = renderer.getTextWidth(SMALL_FONT_ID, labels[i]);
      renderer.drawText(SMALL_FONT_ID, cx - lw / 2, y + lhUi10 + 2, labels[i]);
    }
    y += lhUi10 + lhSmall + 18;
  }
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- RECENT BOOKS ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_LAST_BOOK));
  y += lhSmall + 6;

  const auto& allBooks = BOOK_STATS.getBooks();
  if (!allBooks.empty()) {
    // Collect up to 5 most-recently-read books
    struct BookRef { const char* title; uint32_t secs; uint8_t progress; uint32_t ts; };
    BookRef recent[5];
    int count = 0;
    for (const auto& kv : allBooks) {
      const auto& e = kv.second;
      if (count < 5) {
        recent[count++] = {e.title, e.totalSeconds, e.progress, e.lastReadTimestamp};
      } else {
        int minIdx = 0;
        for (int j = 1; j < 5; j++) {
          if (recent[j].ts < recent[minIdx].ts) minIdx = j;
        }
        if (e.lastReadTimestamp > recent[minIdx].ts) {
          recent[minIdx] = {e.title, e.totalSeconds, e.progress, e.lastReadTimestamp};
        }
      }
    }
    // Sort descending by timestamp
    for (int i = 1; i < count; i++) {
      BookRef tmp = recent[i];
      int j = i - 1;
      while (j >= 0 && recent[j].ts < tmp.ts) { recent[j + 1] = recent[j]; j--; }
      recent[j + 1] = tmp;
    }

    constexpr int BAR_H = 4;
    const int barW = pageWidth - MARGIN * 2;
    const int maxY = pageHeight - 20;

    for (int i = 0; i < count && y < maxY; i++) {
      char timeBuf[24];
      StringUtils::formatReadingDuration(timeBuf, sizeof(timeBuf), recent[i].secs);
      const int timeW = renderer.getTextWidth(SMALL_FONT_ID, timeBuf);
      const int titleMaxW = pageWidth - MARGIN * 2 - timeW - 8;
      const std::string titleStr = renderer.truncatedText(
          SMALL_FONT_ID, recent[i].title, titleMaxW, EpdFontFamily::BOLD);
      renderer.drawText(SMALL_FONT_ID, MARGIN, y, titleStr.c_str(), true, EpdFontFamily::BOLD);
      renderer.drawText(SMALL_FONT_ID, pageWidth - MARGIN - timeW, y, timeBuf);
      y += lhSmall + 3;

      // Progress bar + percentage
      char progBuf[8];
      snprintf(progBuf, sizeof(progBuf), "%u%%", (unsigned)recent[i].progress);
      const int progW = renderer.getTextWidth(SMALL_FONT_ID, progBuf);
      const int progBarW = barW - progW - 8;

      renderer.fillRect(MARGIN, y, progBarW, 1);
      renderer.fillRect(MARGIN, y + BAR_H - 1, progBarW, 1);
      renderer.fillRect(MARGIN, y, 1, BAR_H);
      renderer.fillRect(MARGIN + progBarW - 1, y, 1, BAR_H);
      const int filledW = static_cast<int>((progBarW - 2) * recent[i].progress / 100);
      if (filledW > 0) {
        renderer.fillRect(MARGIN + 1, y + 1, filledW, BAR_H - 2);
      }
      renderer.drawText(SMALL_FONT_ID, MARGIN + progBarW + 6, y - (lhSmall - BAR_H) / 2, progBuf);
      y += BAR_H + 10;
    }
  } else {
    renderer.drawText(UI_10_FONT_ID, MARGIN, y, tr(STR_STATS_NO_BOOKS), true, EpdFontFamily::REGULAR);
  }

  // Button hint: Back to exit
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), nullptr, nullptr, nullptr);
  GUI.drawButtonHints(renderer, labels.btn1, nullptr, nullptr, nullptr);

  renderer.displayBuffer();
}
