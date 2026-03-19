#include "HomeActivity.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/library.h"
#include "components/icons/pet.h"
#include "components/icons/recent.h"
#include "components/icons/settings2.h"
#include "components/icons/tools.h"
#include "components/icons/transfer.h"
#include "fontIds.h"

// ── Classic layout constants (v1.6.8 split left/right + 3-row grid) ──────────
namespace {
constexpr int CL_HEADER_H    = 56;
constexpr int CL_DIVIDER_X   = 240;
constexpr int CL_DIVIDER_Y   = 408;
constexpr int CL_GRID_ROW2_Y = 525;
constexpr int CL_GRID_ROW3_Y = 642;
constexpr int CL_GRID_BOTTOM = 760;
constexpr int CL_COVER_H     = 280;
constexpr int CL_PAD          = 12;
constexpr int CL_PROGRESS_BAR_H = 5;
constexpr int CL_GRID_ICON_SIZE = 32;
constexpr int CL_ITEM_COUNT  = 7;  // 0=cover, 1-6=grid cells
}  // namespace

// ── Cover panel (left side) ───────────────────────────────────────────────────

void HomeActivity::renderCoverPanel(int panelX, int panelY, int panelW, int panelH, int coverH) {
  if (recentBooks.empty()) {
    renderer.drawText(UI_12_FONT_ID, panelX + CL_PAD,
                      panelY + (panelH - renderer.getLineHeight(UI_12_FONT_ID)) / 2,
                      tr(STR_NO_RECENT_BOOKS), true);
    return;
  }
  const RecentBook& book = recentBooks[0];
  if (!book.coverBmpPath.empty()) {
    const std::string thumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverH);
    FsFile f;
    if (Storage.openFileForRead("HOME", thumbPath, f)) {
      Bitmap bmp(f);
      if (bmp.parseHeaders() == BmpReaderError::Ok) {
        const int bmpW = bmp.getWidth();
        const int availH = panelH - 28;
        renderer.drawBitmap(bmp, panelX + (panelW - bmpW) / 2,
                            panelY + (availH - coverH) / 2, bmpW, coverH);
      }
      f.close();
    }
  }
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int lblW = renderer.getTextWidth(UI_12_FONT_ID, tr(STR_CONTINUE_READING), EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, panelX + (panelW - lblW) / 2,
                    panelY + panelH - lineH - 4, tr(STR_CONTINUE_READING), true, EpdFontFamily::BOLD);
}

// ── Progress panel (right side — recent books with progress bars) ─────────────

void HomeActivity::renderProgressPanel(int panelX, int panelY, int panelW, int panelH) {
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  renderer.drawText(UI_12_FONT_ID, panelX + CL_PAD, panelY + CL_PAD, tr(STR_RECENTS), true, EpdFontFamily::BOLD);

  const int booksY = panelY + CL_PAD + renderer.getLineHeight(UI_12_FONT_ID) + 8;
  const int maxBooks = std::min(static_cast<int>(recentBooks.size()), 4);
  if (maxBooks == 0) return;
  const int rowH = (panelH - (booksY - panelY)) / maxBooks;
  const int barW = panelW - CL_PAD * 2;

  for (int i = 0; i < maxBooks; i++) {
    const RecentBook& b = recentBooks[i];
    const int rowY = booksY + i * rowH;
    auto title = renderer.truncatedText(SMALL_FONT_ID, b.title.c_str(), barW - 28);
    renderer.drawText(SMALL_FONT_ID, panelX + CL_PAD, rowY, title.c_str(), true);
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", b.progressPercent);
    const int pctW = renderer.getTextWidth(SMALL_FONT_ID, pct);
    renderer.drawText(SMALL_FONT_ID, panelX + panelW - CL_PAD - pctW, rowY, pct, true);
    const int barY = rowY + lineH + 3;
    renderer.drawRect(panelX + CL_PAD, barY, barW, CL_PROGRESS_BAR_H);
    const int fillW = barW * b.progressPercent / 100;
    if (fillW > 2) renderer.fillRect(panelX + CL_PAD + 1, barY + 1, fillW - 2, CL_PROGRESS_BAR_H - 2);
  }
}

// ── Grid cell ─────────────────────────────────────────────────────────────────

void HomeActivity::renderGridCell(int cellX, int cellY, int cellW, int cellH,
                                  int gridIdx, const uint8_t* icon, const char* label) {
  const bool selected = (selectorIndex == gridIdx + 1);
  if (selected) renderer.fillRoundedRect(cellX + 2, cellY + 2, cellW - 4, cellH - 4, 8, Color::LightGray);
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int totalH = (icon ? CL_GRID_ICON_SIZE + 6 : 0) + lineH;
  const int startY = cellY + (cellH - totalH) / 2;
  if (icon) renderer.drawIcon(icon, cellX + (cellW - CL_GRID_ICON_SIZE) / 2, startY, CL_GRID_ICON_SIZE, CL_GRID_ICON_SIZE);
  const int lblW = renderer.getTextWidth(UI_12_FONT_ID, label);
  renderer.drawText(UI_12_FONT_ID, cellX + (cellW - lblW) / 2,
                    startY + (icon ? CL_GRID_ICON_SIZE + 6 : 0), label, true);
}

// ── Cover selection highlight ─────────────────────────────────────────────────

void HomeActivity::renderClassicSelectionHighlight(int panelX, int panelY, int panelW, int panelH) {
  if (selectorIndex != 0) return;
  renderer.drawRoundedRect(panelX + 2, panelY + 2, panelW - 4, panelH - 4, 2, 8, true);
}

// ── Classic loop (7-item grid navigation) ─────────────────────────────────────

void HomeActivity::loopClassic() {
  buttonNavigator.onNext([this] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, CL_ITEM_COUNT);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, CL_ITEM_COUNT);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (selectorIndex) {
      case 0: if (!recentBooks.empty()) onSelectBook(recentBooks[0].path); break;
      case 1: onToolsOpen(); break;
      case 2: onVirtualPetOpen(); break;
      case 3: onFileBrowserOpen(); break;
      case 4: onRecentBooksOpen(); break;
      case 5: onFileTransferOpen(); break;
      case 6: onSettingsOpen(); break;
    }
  }
}

// ── Classic main render ───────────────────────────────────────────────────────

void HomeActivity::renderClassic() {
  const int pageWidth = renderer.getScreenWidth();

  if (!coverRendered) {
    renderer.clearScreen();
    GUI.drawHeader(renderer, Rect{0, 0, pageWidth, CL_HEADER_H}, nullptr);
    renderPetStatusWidget(CL_HEADER_H);
    // Grid lines
    renderer.drawLine(CL_DIVIDER_X, CL_HEADER_H,  CL_DIVIDER_X, CL_DIVIDER_Y,   true);
    renderer.drawLine(0,            CL_DIVIDER_Y,  pageWidth,     CL_DIVIDER_Y,   true);
    renderer.drawLine(CL_DIVIDER_X, CL_DIVIDER_Y,  CL_DIVIDER_X, CL_GRID_BOTTOM, true);
    renderer.drawLine(0,            CL_GRID_ROW2_Y, pageWidth,    CL_GRID_ROW2_Y, true);
    renderer.drawLine(0,            CL_GRID_ROW3_Y, pageWidth,    CL_GRID_ROW3_Y, true);
    // Cover and progress panels
    renderCoverPanel(0, CL_HEADER_H, CL_DIVIDER_X, CL_DIVIDER_Y - CL_HEADER_H, CL_COVER_H);
    renderProgressPanel(CL_DIVIDER_X, CL_HEADER_H, CL_DIVIDER_X, CL_DIVIDER_Y - CL_HEADER_H);
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  } else {
    restoreCoverBuffer();
    GUI.drawHeader(renderer, Rect{0, 0, pageWidth, CL_HEADER_H}, nullptr);
    renderPetStatusWidget(CL_HEADER_H);
  }

  // Grid cells (redrawn each frame for selection state)
  const int cw = CL_DIVIDER_X;
  const int rh1 = CL_GRID_ROW2_Y - CL_DIVIDER_Y;
  const int rh2 = CL_GRID_ROW3_Y - CL_GRID_ROW2_Y;
  const int rh3 = CL_GRID_BOTTOM  - CL_GRID_ROW3_Y;
  renderGridCell(0,            CL_DIVIDER_Y,   cw, rh1, 0, ToolsIcon,     tr(STR_TOOLS));
  renderGridCell(CL_DIVIDER_X, CL_DIVIDER_Y,   cw, rh1, 1, PetIcon,       tr(STR_VIRTUAL_PET));
  renderGridCell(0,            CL_GRID_ROW2_Y, cw, rh2, 2, LibraryIcon,   tr(STR_BROWSE_FILES));
  renderGridCell(CL_DIVIDER_X, CL_GRID_ROW2_Y, cw, rh2, 3, RecentIcon,    tr(STR_MENU_RECENT_BOOKS));
  renderGridCell(0,            CL_GRID_ROW3_Y, cw, rh3, 4, TransferIcon,  tr(STR_FILE_TRANSFER));
  renderGridCell(CL_DIVIDER_X, CL_GRID_ROW3_Y, cw, rh3, 5, Settings2Icon, tr(STR_SETTINGS_TITLE));

  renderClassicSelectionHighlight(0, CL_HEADER_H, CL_DIVIDER_X, CL_DIVIDER_Y - CL_HEADER_H);

  const auto labels = mappedInput.mapLabels("", tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();

  // Post-render: trigger cover loading
  if (!firstRenderDone) {
    firstRenderDone = true;
    bool needsLoad = false;
    for (const auto& b : recentBooks) {
      if (!b.coverBmpPath.empty() &&
          !Storage.exists(UITheme::getCoverThumbPath(b.coverBmpPath, CL_COVER_H).c_str())) {
        needsLoad = true; break;
      }
    }
    if (needsLoad) requestUpdate();
    else recentsLoaded = true;
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(CL_COVER_H);
  }
}
