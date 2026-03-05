#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

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
#include "activities/tools/WeatherActivity.h"
#include "ble/BleRemoteManager.h"
#include "pet/PetEvolution.h"
#include "pet/PetManager.h"
#include "util/StringUtils.h"

extern BleRemoteManager bleManager;

// ── Layout constants ─────────────────────────────────────────────────────────
namespace {
constexpr int HEADER_H    = 56;   // homeTopPadding (LyraMetrics)
constexpr int DIVIDER_X   = 240;  // left/right split
constexpr int DIVIDER_Y   = 408;  // top section / grid split
constexpr int GRID_ROW2_Y = 525;  // grid row-1 / row-2 boundary
constexpr int GRID_ROW3_Y = 642;  // grid row-2 / row-3 boundary
constexpr int GRID_BOTTOM = 760;  // grid bottom (button hints start)
constexpr int COVER_H     = 280;  // max cover height in left panel
constexpr int PAD         = 12;
constexpr int PROGRESS_BAR_H = 5;
constexpr int GRID_ICON_SIZE = 32;
constexpr int ITEM_COUNT  = 7;    // selectorIndex 0..6  (0=cover, 1-6=grid)
}  // namespace

// ── Buffer management ─────────────────────────────────────────────────────────

bool HomeActivity::storeCoverBuffer() {
  uint8_t* fb = renderer.getFrameBuffer();
  if (!fb) return false;
  freeCoverBuffer();
  coverBuffer = static_cast<uint8_t*>(malloc(GfxRenderer::getBufferSize()));
  if (!coverBuffer) return false;
  memcpy(coverBuffer, fb, GfxRenderer::getBufferSize());
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer) return false;
  uint8_t* fb = renderer.getFrameBuffer();
  if (!fb) return false;
  memcpy(fb, coverBuffer, GfxRenderer::getBufferSize());
  return true;
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) { free(coverBuffer); coverBuffer = nullptr; }
  coverBufferStored = false;
}

// ── Book loading ──────────────────────────────────────────────────────────────

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  for (const RecentBook& b : RECENT_BOOKS.getBooks()) {
    if (static_cast<int>(recentBooks.size()) >= maxBooks) break;
    if (Storage.exists(b.path.c_str())) recentBooks.push_back(b);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  Rect popup;
  bool showingLoading = false;
  bool anyChanged = false;
  int progress = 0;

  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string thumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(thumbPath.c_str())) {
        bool generated = false;
        if (StringUtils::checkFileExtension(book.path, ".epub")) {
          Epub epub(book.path, "/.crosspoint");
          epub.load(false, true);
          if (!showingLoading) { showingLoading = true; popup = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP)); }
          GUI.fillPopupProgress(renderer, popup, 10 + progress * (90 / (int)recentBooks.size()));
          generated = epub.generateThumbBmp(coverHeight);
          if (!generated) { RECENT_BOOKS.updateBook(book.path, book.title, book.author, ""); book.coverBmpPath = ""; }
        } else if (StringUtils::checkFileExtension(book.path, ".xtch") ||
                   StringUtils::checkFileExtension(book.path, ".xtc")) {
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            if (!showingLoading) { showingLoading = true; popup = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP)); }
            GUI.fillPopupProgress(renderer, popup, 10 + progress * (90 / (int)recentBooks.size()));
            generated = xtc.generateThumbBmp(coverHeight);
            if (!generated) { RECENT_BOOKS.updateBook(book.path, book.title, book.author, ""); book.coverBmpPath = ""; }
          }
        }
        if (generated) anyChanged = true;
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
  // Only trigger re-render if new thumbnails were generated
  if (anyChanged) {
    coverRendered = false;
    requestUpdate();
  }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void HomeActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  coverRendered = false;
  firstRenderDone = false;
  recentsLoaded = false;
  recentsLoading = false;
  loadRecentBooks(4);
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();
  freeCoverBuffer();
}

// ── Input ─────────────────────────────────────────────────────────────────────

void HomeActivity::loop() {
  buttonNavigator.onNext([this] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, ITEM_COUNT);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, ITEM_COUNT);
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

// ── Render helpers ────────────────────────────────────────────────────────────

void HomeActivity::renderCoverPanel(int panelX, int panelY, int panelW, int panelH, int coverH) {
  if (recentBooks.empty()) {
    renderer.drawText(UI_12_FONT_ID, panelX + PAD,
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

void HomeActivity::renderProgressPanel(int panelX, int panelY, int panelW, int panelH) {
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  renderer.drawText(UI_12_FONT_ID, panelX + PAD, panelY + PAD, tr(STR_RECENTS), true, EpdFontFamily::BOLD);

  const int booksY = panelY + PAD + renderer.getLineHeight(UI_12_FONT_ID) + 8;
  const int maxBooks = std::min(static_cast<int>(recentBooks.size()), 4);
  if (maxBooks == 0) return;
  const int rowH = (panelH - (booksY - panelY)) / maxBooks;
  const int barW = panelW - PAD * 2;

  for (int i = 0; i < maxBooks; i++) {
    const RecentBook& b = recentBooks[i];
    const int rowY = booksY + i * rowH;
    auto title = renderer.truncatedText(SMALL_FONT_ID, b.title.c_str(), barW - 28);
    renderer.drawText(SMALL_FONT_ID, panelX + PAD, rowY, title.c_str(), true);
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", b.progressPercent);
    const int pctW = renderer.getTextWidth(SMALL_FONT_ID, pct);
    renderer.drawText(SMALL_FONT_ID, panelX + panelW - PAD - pctW, rowY, pct, true);
    const int barY = rowY + lineH + 3;
    renderer.drawRect(panelX + PAD, barY, barW, PROGRESS_BAR_H);
    const int fillW = barW * b.progressPercent / 100;
    if (fillW > 2) renderer.fillRect(panelX + PAD + 1, barY + 1, fillW - 2, PROGRESS_BAR_H - 2);
  }
}

void HomeActivity::renderGridCell(int cellX, int cellY, int cellW, int cellH,
                                  int gridIdx, const uint8_t* icon, const char* label) {
  const bool selected = (selectorIndex == gridIdx + 1);
  if (selected) renderer.fillRoundedRect(cellX + 2, cellY + 2, cellW - 4, cellH - 4, 8, Color::LightGray);
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int totalH = (icon ? GRID_ICON_SIZE + 6 : 0) + lineH;
  const int startY = cellY + (cellH - totalH) / 2;
  if (icon) renderer.drawIcon(icon, cellX + (cellW - GRID_ICON_SIZE) / 2, startY, GRID_ICON_SIZE, GRID_ICON_SIZE);
  const int lblW = renderer.getTextWidth(UI_12_FONT_ID, label);
  renderer.drawText(UI_12_FONT_ID, cellX + (cellW - lblW) / 2,
                    startY + (icon ? GRID_ICON_SIZE + 6 : 0), label, true);
}

void HomeActivity::renderSelectionHighlight(int panelX, int panelY, int panelW, int panelH) {
  if (selectorIndex != 0) return;
  renderer.drawRoundedRect(panelX + 2, panelY + 2, panelW - 4, panelH - 4, 2, 8, true);
}

// ── Pet status widget (drawn over header left side) ──────────────────────

void HomeActivity::renderPetStatusWidget(int headerH) {
  if (!PET_MANAGER.exists() || !PET_MANAGER.isAlive()) return;

  const auto& ps = PET_MANAGER.getState();
  const PetMood mood = PET_MANAGER.getMood();
  const int screenW = renderer.getScreenWidth();

  // Context-aware motivational status message (i18n)
  const char* name = ps.petName[0] ? ps.petName : PetEvolution::variantStageName(ps.stage, ps.evolutionVariant);
  char petBuf[64];

  if (ps.hunger < 30) {
    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_HUNGRY), name);
  } else if (mood == PetMood::SAD) {
    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_SAD), name);
  } else if (mood == PetMood::SICK) {
    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_SICK), name);
  } else if (mood == PetMood::NEEDY) {
    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_NEEDY), name);
  } else if (mood == PetMood::SLEEPING) {
    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_SLEEPING), name);
  } else if (ps.currentStreak > 7) {
    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_STREAK), name, ps.currentStreak);
  } else if (mood == PetMood::HAPPY) {
    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_HAPPY), name);
  } else {
    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_DEFAULT), name);
  }
  const int textW = renderer.getTextWidth(SMALL_FONT_ID, petBuf);
  // Calculate right margin: battery icon (15) + right padding (12) + optional percentage text
  int rightMargin = 12 + BaseMetrics::values.batteryWidth + 4;
  if (SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS) {
    // Reserve space for "100%" text (worst case width)
    rightMargin += renderer.getTextWidth(SMALL_FONT_ID, "100%") + 4;
  }
  // Clamp text to available space
  const int availW = screenW - rightMargin - 4;
  auto truncated = renderer.truncatedText(SMALL_FONT_ID, petBuf, availW);
  const int finalW = renderer.getTextWidth(SMALL_FONT_ID, truncated.c_str());
  const int x = screenW - rightMargin - finalW;
  const int y = 5;  // same y offset as battery
  renderer.drawText(SMALL_FONT_ID, x, y, truncated.c_str(), true);
}

// ── Header clock ─────────────────────────────────────────────────────────────

void HomeActivity::renderHeaderClock() {
  if (!SETTINGS.statusBarClock) return;
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[8];
  if (timeinfo.tm_year >= 125) {
    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    snprintf(buf, sizeof(buf), "--:--");
  }
  const int clockW = renderer.getTextWidth(SMALL_FONT_ID, buf);
  renderer.drawText(SMALL_FONT_ID, 10, 5, buf);

  // Weather temp next to clock
  WeatherData wData;
  uint8_t wCity = 0;
  char wTime[8] = "";
  if (WeatherActivity::loadWeatherCache(wData, wCity, wTime, sizeof(wTime))) {
    char wBuf[16];
    snprintf(wBuf, sizeof(wBuf), "%.0f°C", wData.temperature);
    renderer.drawText(SMALL_FONT_ID, 10 + clockW + 6, 5, wBuf);
  }
}

// ── Main render ───────────────────────────────────────────────────────────────

void HomeActivity::render(RenderLock&&) {
  const int pageWidth = renderer.getScreenWidth();

  if (!coverRendered) {
    renderer.clearScreen();
    GUI.drawHeader(renderer, Rect{0, 0, pageWidth, HEADER_H}, nullptr);
    renderHeaderClock();
    renderPetStatusWidget(HEADER_H);
    renderer.drawLine(DIVIDER_X, HEADER_H,  DIVIDER_X, DIVIDER_Y,   true);  // top vertical
    renderer.drawLine(0,         DIVIDER_Y, pageWidth,  DIVIDER_Y,   true);  // mid horizontal
    renderer.drawLine(DIVIDER_X, DIVIDER_Y, DIVIDER_X, GRID_BOTTOM, true);  // grid vertical
    renderer.drawLine(0,         GRID_ROW2_Y, pageWidth, GRID_ROW2_Y, true); // grid row 2
    renderer.drawLine(0,         GRID_ROW3_Y, pageWidth, GRID_ROW3_Y, true); // grid row 3
    renderCoverPanel(0, HEADER_H, DIVIDER_X, DIVIDER_Y - HEADER_H, COVER_H);
    renderProgressPanel(DIVIDER_X, HEADER_H, DIVIDER_X, DIVIDER_Y - HEADER_H);
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  } else {
    restoreCoverBuffer();
    GUI.drawHeader(renderer, Rect{0, 0, pageWidth, HEADER_H}, nullptr);
    renderHeaderClock();
    renderPetStatusWidget(HEADER_H);
  }

  // Grid cells (redrawn each frame for selection state)
  const int cw = DIVIDER_X;
  const int rh1 = GRID_ROW2_Y - DIVIDER_Y;
  const int rh2 = GRID_ROW3_Y - GRID_ROW2_Y;
  const int rh3 = GRID_BOTTOM  - GRID_ROW3_Y;
  renderGridCell(0,         DIVIDER_Y,   cw, rh1, 0, ToolsIcon,     tr(STR_TOOLS));
  renderGridCell(DIVIDER_X, DIVIDER_Y,   cw, rh1, 1, PetIcon,       tr(STR_VIRTUAL_PET));
  renderGridCell(0,         GRID_ROW2_Y, cw, rh2, 2, LibraryIcon,   tr(STR_BROWSE_FILES));
  renderGridCell(DIVIDER_X, GRID_ROW2_Y, cw, rh2, 3, RecentIcon,    tr(STR_MENU_RECENT_BOOKS));
  renderGridCell(0,         GRID_ROW3_Y, cw, rh3, 4, TransferIcon,  tr(STR_FILE_TRANSFER));
  renderGridCell(DIVIDER_X, GRID_ROW3_Y, cw, rh3, 5, Settings2Icon, tr(STR_SETTINGS_TITLE));

  renderSelectionHighlight(0, HEADER_H, DIVIDER_X, DIVIDER_Y - HEADER_H);

  const auto labels = mappedInput.mapLabels("", tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();

  if (!firstRenderDone) {
    firstRenderDone = true;
    // Only trigger cover loading if thumbnails are missing
    bool needsLoad = false;
    for (const auto& b : recentBooks) {
      if (!b.coverBmpPath.empty() &&
          !Storage.exists(UITheme::getCoverThumbPath(b.coverBmpPath, COVER_H).c_str())) {
        needsLoad = true; break;
      }
    }
    if (needsLoad) {
      requestUpdate();  // will trigger loadRecentCovers on next render
    } else {
      recentsLoaded = true;  // thumbnails already exist, skip loading
    }
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(COVER_H);
  }
}

// ── Actions ───────────────────────────────────────────────────────────────────

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }
void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }
void HomeActivity::onRecentBooksOpen() { activityManager.goToRecentBooks(); }
void HomeActivity::onVirtualPetOpen()  { activityManager.goToVirtualPet(); }
void HomeActivity::onFileTransferOpen(){ activityManager.goToFileTransfer(); }
void HomeActivity::onSettingsOpen()    { activityManager.goToSettings(); }
void HomeActivity::onToolsOpen()       { activityManager.goToTools(); }
