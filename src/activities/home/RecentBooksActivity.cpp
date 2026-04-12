#include "RecentBooksActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Must match the home screen's CP_COVER_H so we reuse the same cached thumbnails
constexpr int THUMB_H = 188;
}

void RecentBooksActivity::loadRecentBooks() {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(books.size());
  for (const auto& book : books) {
    if (!Storage.exists(book.path.c_str())) continue;
    recentBooks.push_back(book);
  }
}

int RecentBooksActivity::totalPages() const {
  if (recentBooks.empty()) return 1;
  return (static_cast<int>(recentBooks.size()) + itemsPerPage - 1) / itemsPerPage;
}

void RecentBooksActivity::ensurePageForIndex() {
  if (itemsPerPage <= 0) return;
  const int page = selectorIndex / itemsPerPage;
  pageOffset = page * itemsPerPage;
}

void RecentBooksActivity::onEnter() {
  Activity::onEnter();
  loadRecentBooks();
  selectorIndex = 0;
  pageOffset = 0;
  requestUpdate();
}

void RecentBooksActivity::onExit() {
  Activity::onExit();
  recentBooks.clear();
}

void RecentBooksActivity::loop() {
  const int total = static_cast<int>(recentBooks.size());
  if (total == 0) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }
    return;
  }

  // D-pad navigation: left/right within row, up/down between rows
  buttonNavigator.onNext([this, total] {
    // Next = move right, wrap to next row
    selectorIndex = (selectorIndex + 1) % total;
    ensurePageForIndex();
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, total] {
    selectorIndex = (selectorIndex - 1 + total) % total;
    ensurePageForIndex();
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex < total) {
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }
}

void RecentBooksActivity::renderCover(int bookIdx, int gridCol, int gridRow,
                                       int cardW, int cardH, int startX, int startY, bool selected) {
  const int cx = startX + gridCol * (cardW + COVER_GAP);
  const int cy = startY + gridRow * (cardH + 30);  // 30 = space for title below

  // Cover border — thicker when selected
  if (selected) {
    renderer.drawRoundedRect(cx - 1, cy - 1, cardW + 2, cardH + 2, 3, COVER_R + 1, true);
  } else {
    renderer.drawRoundedRect(cx, cy, cardW, cardH, 1, COVER_R, true);
  }

  // Cover thumbnail
  const RecentBook& book = recentBooks[bookIdx];
  if (!book.coverBmpPath.empty()) {
    const std::string thumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, THUMB_H);
    FsFile f;
    if (Storage.openFileForRead("RBA", thumbPath, f)) {
      Bitmap bmp(f);
      if (bmp.parseHeaders() == BmpReaderError::Ok) {
        int bmpW = std::min((int)bmp.getWidth(), cardW - 4);
        int bmpH = std::min((int)bmp.getHeight(), cardH - 4);
        renderer.drawBitmap(bmp, cx + (cardW - bmpW) / 2, cy + 2, bmpW, bmpH);
      }
      f.close();
    }
  }

  // Progress bar at bottom of card
  const int barY = cy + cardH - 5;
  const int barX = cx + 4;
  const int barW = cardW - 8;
  renderer.fillRect(barX, barY, barW, 4, false);
  const int fillW = barW * book.progressPercent / 100;
  if (fillW > 1) renderer.fillRect(barX, barY, fillW, 4);

  // Title below card (truncated, centered, bold when selected)
  const int titleY = cy + cardH + 4;
  const auto titleStyle = selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  auto title = renderer.truncatedText(SMALL_FONT_ID, book.title.c_str(), cardW - 2, titleStyle);
  const int titleW = renderer.getTextWidth(SMALL_FONT_ID, title.c_str(), titleStyle);
  renderer.drawText(SMALL_FONT_ID, cx + (cardW - titleW) / 2, titleY, title.c_str(), true, titleStyle);
}

void RecentBooksActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MENU_RECENT_BOOKS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (recentBooks.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_RECENT_BOOKS));
  } else {
    // Calculate grid layout
    const int sidePad = 14;
    const int areaW = pageWidth - 2 * sidePad;
    const int totalGapW = COVER_GAP * (COLS - 1);
    const int cardW = (areaW - totalGapW) / COLS;
    const int cardH = (int)(cardW * 1.4f);
    const int rowH = cardH + 30;  // card + title space
    const int rows = contentHeight / rowH;
    itemsPerPage = std::max(1, rows * COLS);

    const int total = static_cast<int>(recentBooks.size());
    const int endIdx = std::min(pageOffset + itemsPerPage, total);

    for (int i = pageOffset; i < endIdx; i++) {
      const int localIdx = i - pageOffset;
      const int col = localIdx % COLS;
      const int row = localIdx / COLS;
      renderCover(i, col, row, cardW, cardH, sidePad, contentTop + 4, i == selectorIndex);
    }

    // Page indicator if multiple pages
    if (totalPages() > 1) {
      char pageStr[16];
      snprintf(pageStr, sizeof(pageStr), "%d/%d", (selectorIndex / itemsPerPage) + 1, totalPages());
      const int pw = renderer.getTextWidth(SMALL_FONT_ID, pageStr);
      renderer.drawText(SMALL_FONT_ID, pageWidth - pw - 10,
                        pageHeight - metrics.buttonHintsHeight - 16, pageStr, true);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
