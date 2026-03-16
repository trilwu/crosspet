#include "HomeActivity.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "BookStats.h"
#include "ReadingStats.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/library.h"
#include "components/icons/settings2.h"
#include "components/icons/tools.h"
#include "components/icons/transfer.h"
#include "fontIds.h"

// ── CrossPet layout constants ────────────────────────────────────────────────
namespace {
constexpr int CP_HEADER_H       = 44;
constexpr int CP_CARD_MARGIN    = 12;
constexpr int CP_CARD_Y         = 52;
constexpr int CP_CARD_H         = 216;
constexpr int CP_CARD_R         = 14;
constexpr int CP_COVER_H        = 188;
constexpr int CP_PAD            = 14;
constexpr int CP_RECENT_LABEL_Y = 280;
constexpr int CP_RECENT_COVER_Y = 302;
constexpr int CP_MAX_RECENT     = 3;
constexpr int CP_BOTTOM_BAR_H   = 80;
constexpr int CP_BOTTOM_ITEMS   = 4;
constexpr int CP_BOTTOM_ICON_SZ = 32;
constexpr int CP_BOTTOM_R       = 10;
}  // namespace

// ── Continue reading card ─────────────────────────────────────────────────────

void HomeActivity::renderContinueReadingCard() {
  const int screenW = renderer.getScreenWidth();
  const int cardX = CP_CARD_MARGIN;
  const int cardW = screenW - 2 * CP_CARD_MARGIN;

  renderer.drawRoundedRect(cardX, CP_CARD_Y, cardW, CP_CARD_H, 1, CP_CARD_R, true);

  if (recentBooks.empty()) {
    renderer.drawText(UI_12_FONT_ID, cardX + CP_PAD,
                      CP_CARD_Y + (CP_CARD_H - renderer.getLineHeight(UI_12_FONT_ID)) / 2,
                      tr(STR_NO_RECENT_BOOKS), true);
    return;
  }

  const RecentBook& book = recentBooks[0];
  const int coverX = cardX + CP_PAD;
  const int coverY = CP_CARD_Y + CP_PAD;
  const int smallLineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int medLineH = renderer.getLineHeight(UI_12_FONT_ID);

  // Cover image on left
  int actualCoverW = 170;
  if (!book.coverBmpPath.empty()) {
    const std::string thumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, CP_COVER_H);
    FsFile f;
    if (Storage.openFileForRead("HOME", thumbPath, f)) {
      Bitmap bmp(f);
      if (bmp.parseHeaders() == BmpReaderError::Ok) {
        actualCoverW = bmp.getWidth();
        renderer.drawBitmap(bmp, coverX, coverY, actualCoverW, CP_COVER_H);
      }
      f.close();
    }
  }

  // Right side: title, author, progress, per-book stats
  const int infoX = coverX + actualCoverW + 14;
  const int infoW = cardX + cardW - infoX - CP_PAD;
  int infoY = coverY;

  auto title = renderer.truncatedText(UI_12_FONT_ID, book.title.c_str(), infoW);
  renderer.drawText(UI_12_FONT_ID, infoX, infoY, title.c_str(), true, EpdFontFamily::BOLD);
  infoY += medLineH + 4;

  if (!book.author.empty()) {
    auto author = renderer.truncatedText(SMALL_FONT_ID, book.author.c_str(), infoW);
    renderer.drawText(SMALL_FONT_ID, infoX, infoY, author.c_str(), true);
    infoY += smallLineH + 8;
  }

  // Progress percentage + bar
  char pct[8];
  snprintf(pct, sizeof(pct), "%d%%", book.progressPercent);
  renderer.drawText(UI_12_FONT_ID, infoX, infoY, pct, true, EpdFontFamily::BOLD);
  infoY += medLineH + 4;

  const int barW = infoW;
  const int barH = 8;
  renderer.drawRect(infoX, infoY, barW, barH);
  const int fillW = barW * book.progressPercent / 100;
  if (fillW > 2) renderer.fillRect(infoX + 1, infoY + 1, fillW - 2, barH - 2);
  infoY += barH + 12;

  // 2-column per-book stats
  const auto* bs = BOOK_STATS.getBook(book.path.c_str());
  const int col2X = infoX + infoW / 2;
  char statBuf[32];

  renderer.drawText(SMALL_FONT_ID, infoX, infoY, tr(STR_HOME_STAT_TIME_READ), true);
  renderer.drawText(SMALL_FONT_ID, col2X, infoY, tr(STR_HOME_STAT_SESSIONS), true);
  infoY += smallLineH + 2;

  uint32_t bookMin = bs ? bs->totalSeconds / 60 : 0;
  if (bookMin >= 60)
    snprintf(statBuf, sizeof(statBuf), tr(STR_HOME_STAT_HOURS_MINUTES), (int)(bookMin / 60), (int)(bookMin % 60));
  else
    snprintf(statBuf, sizeof(statBuf), tr(STR_HOME_STAT_MINUTES), (int)bookMin);
  renderer.drawText(SMALL_FONT_ID, infoX, infoY, statBuf, true, EpdFontFamily::BOLD);

  snprintf(statBuf, sizeof(statBuf), tr(STR_HOME_STAT_TIMES_READ), bs ? bs->sessions : 0);
  renderer.drawText(SMALL_FONT_ID, col2X, infoY, statBuf, true, EpdFontFamily::BOLD);
}

// ── Recent cover thumbnails ───────────────────────────────────────────────────

void HomeActivity::renderRecentCovers() {
  if (recentBooks.size() <= 1) return;

  const int screenW = renderer.getScreenWidth();
  const int areaW = screenW - 2 * CP_CARD_MARGIN;

  renderer.drawText(SMALL_FONT_ID, CP_CARD_MARGIN + CP_PAD, CP_RECENT_LABEL_Y, tr(STR_HOME_RECENTLY_READ), true, EpdFontFamily::BOLD);

  constexpr int gap = 10;
  constexpr int coverR = 12;
  const int totalGap = gap * (CP_MAX_RECENT - 1);
  const int cardW = (areaW - totalGap) / CP_MAX_RECENT;
  const int cardH = (int)(cardW * 1.4f);

  int count = 0;
  for (int i = 1; i < static_cast<int>(recentBooks.size()) && count < CP_MAX_RECENT; i++, count++) {
    const RecentBook& b = recentBooks[i];
    const int cx = CP_CARD_MARGIN + count * (cardW + gap);

    renderer.drawRoundedRect(cx, CP_RECENT_COVER_Y, cardW, cardH, 1, coverR, true);

    if (!b.coverBmpPath.empty()) {
      const std::string thumbPath = UITheme::getCoverThumbPath(b.coverBmpPath, CP_COVER_H);
      FsFile f;
      if (Storage.openFileForRead("HOME_RECENT", thumbPath, f)) {
        Bitmap bmp(f);
        if (bmp.parseHeaders() == BmpReaderError::Ok) {
          int bmpW = std::min((int)bmp.getWidth(), cardW - 4);
          int bmpH = std::min((int)bmp.getHeight(), cardH - 4);
          renderer.drawBitmap(bmp, cx + (cardW - bmpW) / 2, CP_RECENT_COVER_Y + 2, bmpW, bmpH);
        }
        f.close();
      }
    }

    // Progress bar at bottom of card
    const int barY = CP_RECENT_COVER_Y + cardH - 5;
    const int barX = cx + 4;
    const int barW = cardW - 8;
    renderer.fillRect(barX, barY, barW, 4, false);
    const int fillW = barW * b.progressPercent / 100;
    if (fillW > 1) renderer.fillRect(barX, barY, fillW, 4);

    // Title below card
    const int titleY = CP_RECENT_COVER_Y + cardH + 6;
    auto title = renderer.truncatedText(SMALL_FONT_ID, b.title.c_str(), cardW - 4);
    const int titleW = renderer.getTextWidth(SMALL_FONT_ID, title.c_str());
    renderer.drawText(SMALL_FONT_ID, cx + (cardW - titleW) / 2, titleY, title.c_str(), true);
  }
}

void HomeActivity::renderRecentSelection() {
  if (recentBooks.size() <= 1) return;

  const int screenW = renderer.getScreenWidth();
  const int areaW = screenW - 2 * CP_CARD_MARGIN;
  const int recentCount = std::min(CP_MAX_RECENT, static_cast<int>(recentBooks.size()) - 1);

  constexpr int gap = 10;
  const int totalGap = gap * (CP_MAX_RECENT - 1);
  const int cardW = (areaW - totalGap) / CP_MAX_RECENT;
  const int cardH = (int)(cardW * 1.4f);

  for (int i = 0; i < recentCount; i++) {
    if (selectorIndex == i + 1) {
      const int cx = CP_CARD_MARGIN + i * (cardW + gap);
      renderer.drawRoundedRect(cx - 3, CP_RECENT_COVER_Y - 3, cardW + 6, cardH + 6, 2, 14, true);
      break;
    }
  }
}

// ── Reading stats panel in gap between recent covers and bottom bar ───────────

void HomeActivity::renderReadingStatsBar() {
  const int screenW = renderer.getScreenWidth();
  const int areaW = screenW - 2 * CP_CARD_MARGIN;

  // Compute gap boundaries
  constexpr int coverGap = 10;
  const int totalGap = coverGap * (CP_MAX_RECENT - 1);
  const int coverCardW = (areaW - totalGap) / CP_MAX_RECENT;
  const int coverCardH = (int)(coverCardW * 1.4f);
  const int recentBottom = CP_RECENT_COVER_Y + coverCardH + 6 + renderer.getLineHeight(SMALL_FONT_ID);
  const int barY = renderer.getScreenHeight() - BaseMetrics::values.buttonHintsHeight - CP_BOTTOM_BAR_H;
  const int smallH = renderer.getLineHeight(SMALL_FONT_ID);

  // Stats panel dimensions — 3-column card
  constexpr int panelH = 72;
  constexpr int panelR = 12;
  const int panelX = CP_CARD_MARGIN;
  const int panelY = (recentBottom + barY - panelH) / 2;
  const int panelW = areaW;
  const int colW = panelW / 3;

  // Subtle vertical dividers only (no background)
  const int divY1 = panelY + 8;
  const int divY2 = panelY + panelH - 8;
  renderer.fillRect(panelX + colW, divY1, 1, divY2 - divY1, true);
  renderer.fillRect(panelX + colW * 2, divY1, 1, divY2 - divY1, true);

  // Prepare stat values
  const uint32_t todaySec = READ_STATS.todayReadSeconds;
  const uint32_t totalSec = READ_STATS.totalReadSeconds;
  const int todayMin = todaySec / 60;
  const int totalHr = totalSec / 3600;
  const int totalMin = (totalSec % 3600) / 60;
  const int streak = READ_STATS.currentStreak;

  // Labels (top row, centered in each column)
  const int labelY = panelY + 12;
  const char* labels[] = {tr(STR_HOME_STAT_TODAY), tr(STR_HOME_STAT_TOTAL), tr(STR_HOME_STAT_SESSIONS)};
  for (int i = 0; i < 3; i++) {
    int lw = renderer.getTextWidth(SMALL_FONT_ID, labels[i]);
    int cx = panelX + colW * i + (colW - lw) / 2;
    renderer.drawText(SMALL_FONT_ID, cx, labelY, labels[i], true);
  }

  // Values (bottom row, centered in each column)
  const int valueY = labelY + smallH + 6;
  char val0[24], val1[24], val2[16];
  snprintf(val0, sizeof(val0), tr(STR_HOME_STAT_MINUTES), todayMin);
  if (totalHr > 0)
    snprintf(val1, sizeof(val1), tr(STR_HOME_STAT_HOURS_MINUTES), totalHr, totalMin);
  else
    snprintf(val1, sizeof(val1), tr(STR_HOME_STAT_MINUTES), totalMin);
  snprintf(val2, sizeof(val2), "%d", READ_STATS.totalSessions);

  const char* values[] = {val0, val1, val2};
  for (int i = 0; i < 3; i++) {
    int vw = renderer.getTextWidth(SMALL_FONT_ID, values[i]);
    int cx = panelX + colW * i + (colW - vw) / 2;
    renderer.drawText(SMALL_FONT_ID, cx, valueY, values[i], true);
  }
}

// ── Bottom navigation bar ─────────────────────────────────────────────────────

// Render bottom bar icons + labels (static, cached in cover buffer)
void HomeActivity::renderBottomBarIcons() {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();

  const int barY = screenH - BaseMetrics::values.buttonHintsHeight - CP_BOTTOM_BAR_H;
  renderer.drawLine(0, barY, screenW, barY);

  constexpr int barPad = 8;
  constexpr int innerTop = 10;
  const int itemW = (screenW - 2 * barPad) / CP_BOTTOM_ITEMS;
  const int contentH = CP_BOTTOM_BAR_H - innerTop;

  struct BarItem { const uint8_t* icon; const char* label; };
  const BarItem items[] = {
    {ToolsIcon, tr(STR_TOOLS)},
    {LibraryIcon, tr(STR_BROWSE_FILES)},
    {TransferIcon, tr(STR_FILE_TRANSFER)},
    {Settings2Icon, tr(STR_SETTINGS_TITLE)},
  };

  for (int i = 0; i < CP_BOTTOM_ITEMS; i++) {
    const int x = barPad + i * itemW;
    const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
    const int totalH = CP_BOTTOM_ICON_SZ + 4 + lineH;
    const int startY = barY + innerTop + (contentH - totalH) / 2;
    renderer.drawIcon(items[i].icon, x + (itemW - CP_BOTTOM_ICON_SZ) / 2, startY, CP_BOTTOM_ICON_SZ, CP_BOTTOM_ICON_SZ);
    auto label = renderer.truncatedText(SMALL_FONT_ID, items[i].label, itemW - 4);
    const int lblW = renderer.getTextWidth(SMALL_FONT_ID, label.c_str());
    renderer.drawText(SMALL_FONT_ID, x + (itemW - lblW) / 2, startY + CP_BOTTOM_ICON_SZ + 4, label.c_str(), true);
  }
}

// Render bottom bar selection highlight only (dynamic, per-frame)
void HomeActivity::renderBottomBarSelection() {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int recentCount = std::min(CP_MAX_RECENT, static_cast<int>(recentBooks.size()) - 1);
  const int barStart = 1 + recentCount;

  const int barY = screenH - BaseMetrics::values.buttonHintsHeight - CP_BOTTOM_BAR_H;
  constexpr int barPad = 8;
  constexpr int innerTop = 10;
  const int itemW = (screenW - 2 * barPad) / CP_BOTTOM_ITEMS;
  const int contentH = CP_BOTTOM_BAR_H - innerTop;

  for (int i = 0; i < CP_BOTTOM_ITEMS; i++) {
    if (selectorIndex == barStart + i) {
      const int x = barPad + i * itemW;
      renderer.fillRoundedRect(x + 2, barY + innerTop - 2, itemW - 4, contentH, CP_BOTTOM_R, Color::LightGray);
      renderer.drawRoundedRect(x + 2, barY + innerTop - 2, itemW - 4, contentH, 2, CP_BOTTOM_R, true);
      // Redraw icon + label inside selection (they were overwritten by fill)
      struct BarItem { const uint8_t* icon; const char* label; };
      const BarItem items[] = {
        {ToolsIcon, tr(STR_TOOLS)},
        {LibraryIcon, tr(STR_BROWSE_FILES)},
        {TransferIcon, tr(STR_FILE_TRANSFER)},
        {Settings2Icon, tr(STR_SETTINGS_TITLE)},
      };
      const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
      const int totalH = CP_BOTTOM_ICON_SZ + 4 + lineH;
      const int startY = barY + innerTop + (contentH - totalH) / 2;
      renderer.drawIcon(items[i].icon, x + (itemW - CP_BOTTOM_ICON_SZ) / 2, startY, CP_BOTTOM_ICON_SZ, CP_BOTTOM_ICON_SZ);
      auto label = renderer.truncatedText(SMALL_FONT_ID, items[i].label, itemW - 4);
      const int lblW = renderer.getTextWidth(SMALL_FONT_ID, label.c_str());
      renderer.drawText(SMALL_FONT_ID, x + (itemW - lblW) / 2, startY + CP_BOTTOM_ICON_SZ + 4, label.c_str(), true);
      break;
    }
  }
}

void HomeActivity::renderSelectionHighlight() {
  if (selectorIndex != 0) return;
  const int screenW = renderer.getScreenWidth();
  renderer.drawRoundedRect(CP_CARD_MARGIN, CP_CARD_Y, screenW - 2 * CP_CARD_MARGIN, CP_CARD_H, 2, CP_CARD_R, true);
}

// ── CrossPet loop (card layout navigation) ────────────────────────────────────

void HomeActivity::loopCrossPet() {
  const int recentCount = std::max(0, std::min(CP_MAX_RECENT, static_cast<int>(recentBooks.size()) - 1));
  const int barStart = 1 + recentCount;
  const int itemCount = barStart + CP_BOTTOM_ITEMS;

  buttonNavigator.onNext([this, itemCount] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this, itemCount] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, itemCount);
    requestUpdate();
  });

  // Back long-press = sync
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= 800 && !syncTriggered) {
    syncTriggered = true;
    doSync();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) syncTriggered = false;

  if (syncResultMsg && millis() > syncResultExpiry) {
    syncResultMsg = nullptr;
    requestUpdate();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex == 0) {
      if (!recentBooks.empty()) onSelectBook(recentBooks[0].path);
    } else if (selectorIndex <= recentCount) {
      onSelectBook(recentBooks[selectorIndex].path);
    } else {
      switch (selectorIndex - barStart) {
        case 0: onToolsOpen(); break;
        case 1: onFileBrowserOpen(); break;
        case 2: onFileTransferOpen(); break;
        case 3: onSettingsOpen(); break;
      }
    }
  }
}

// ── Button hints (static, drawn once into buffer) ────────────────────────────

void HomeActivity::renderButtonHints() {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  constexpr int hH = BaseMetrics::values.buttonHintsHeight;
  const int hY = sh - hH;
  renderer.fillRect(0, hY, sw, hH, false);
  renderer.drawLine(0, hY, sw - 1, hY);

  constexpr int bw = 106;
  constexpr int pos[] = {25, 130, 245, 350};
  const int cy = hY + hH / 2;
  constexpr int s = 7;

  // Left triangle
  { const int cx = pos[0] + bw / 2;
    for (int dx = -s; dx <= s; dx++) {
      const int h = dx + s + 1;
      renderer.fillRect(cx + dx, cy - h / 2, 1, h);
    }
  }
  // Filled circle
  { const int cx = pos[1] + bw / 2;
    static constexpr int8_t dx[] = {7, 6, 6, 6, 5, 4, 3, 2};
    for (int dy = -s; dy <= s; dy++) {
      const int d = dx[abs(dy)];
      renderer.fillRect(cx - d, cy + dy, 2 * d + 1, 1);
    }
  }
  // Up triangle
  { const int cx = pos[2] + bw / 2;
    for (int dy = -s; dy <= s; dy++) {
      const int w = dy + s + 1;
      renderer.fillRect(cx - w / 2, cy + dy, w, 1);
    }
  }
  // Down triangle
  { const int cx = pos[3] + bw / 2;
    for (int dy = -s; dy <= s; dy++) {
      const int w = s - dy + 1;
      renderer.fillRect(cx - w / 2, cy + dy, w, 1);
    }
  }
}

// ── CrossPet main render ──────────────────────────────────────────────────────

void HomeActivity::renderCrossPet() {
  const int screenW = renderer.getScreenWidth();

  if (!coverRendered) {
    // First render: build full base buffer (covers + static UI elements)
    renderer.clearScreen();
    renderContinueReadingCard();
    renderRecentCovers();
    // Include static elements in buffer to avoid redrawing every frame
    renderReadingStatsBar();
    renderBottomBarIcons();
    renderButtonHints();
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  } else {
    // Fast path: restore cached buffer (covers + static elements)
    restoreCoverBuffer();
  }

  // Dynamic elements (redrawn each frame — lightweight)
  GUI.drawHeader(renderer, Rect{0, 0, screenW, CP_HEADER_H}, nullptr);
  renderHeaderClock();
  renderPetStatusWidget(CP_HEADER_H);

  // Selection highlights only (not full bottom bar redraw)
  renderSelectionHighlight();
  renderRecentSelection();
  renderBottomBarSelection();

  renderer.displayBuffer();

  // Post-render: trigger cover thumbnail loading
  if (!firstRenderDone) {
    firstRenderDone = true;
    bool needsLoad = false;
    for (const auto& b : recentBooks) {
      if (!b.coverBmpPath.empty() &&
          !Storage.exists(UITheme::getCoverThumbPath(b.coverBmpPath, CP_COVER_H).c_str())) {
        needsLoad = true; break;
      }
    }
    if (needsLoad) requestUpdate();
    else recentsLoaded = true;
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(CP_COVER_H);
  }
}
