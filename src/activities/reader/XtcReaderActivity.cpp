/**
 * XtcReaderActivity.cpp
 *
 * XTC ebook reader activity implementation
 * Displays pre-rendered XTC pages on e-ink display
 */

#include "XtcReaderActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "pet/PetManager.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "XtcReaderChapterSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long skipPageMs = 700;
constexpr unsigned long goHomeMs = 1000;
}  // namespace

void XtcReaderActivity::onEnter() {
  Activity::onEnter();
  if (!xtc) {
    return;
  }

  xtc->setupCacheDir();

  // Load saved progress
  loadProgress();

  // Save current XTC as last opened book and add to recent books
  APP_STATE.openEpubPath = xtc->getPath();
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(xtc->getPath(), xtc->getTitle(), xtc->getAuthor(), xtc->getThumbBmpPath());

  // Trigger first update
  requestUpdate();
}

void XtcReaderActivity::onExit() {
  Activity::onExit();

  // Request half refresh for the next screen to clear accumulated reader ghosting
  renderer.requestNextHalfRefresh();

  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  xtc.reset();
}

void XtcReaderActivity::loop() {
  // Enter chapter selection activity
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (xtc && xtc->hasChapters() && !xtc->getChapters().empty()) {
      startActivityForResult(
          std::make_unique<XtcReaderChapterSelectionActivity>(renderer, mappedInput, xtc, currentPage),
          [this](const ActivityResult& result) {
            if (!result.isCancelled) {
              currentPage = std::get<PageResult>(result.data).page;
            }
          });
    }
  }

  // Long press BACK (1s+) goes to file selection
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= goHomeMs) {
    activityManager.goToFileBrowser(xtc ? xtc->getPath() : "");
    return;
  }

  // Short press BACK goes directly to home
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && mappedInput.getHeldTime() < goHomeMs) {
    onGoHome();
    return;
  }

  // When long-press chapter skip is disabled, turn pages on press instead of release.
  const bool usePressForPageTurn = !SETTINGS.longPressChapterSkip;
  const bool prevTriggered = usePressForPageTurn ? (mappedInput.wasPressed(MappedInputManager::Button::PageBack) ||
                                                    mappedInput.wasPressed(MappedInputManager::Button::FrontPageBack))
                                                 : (mappedInput.wasReleased(MappedInputManager::Button::PageBack) ||
                                                    mappedInput.wasReleased(MappedInputManager::Button::FrontPageBack));
  const bool powerPageTurn = SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::PAGE_TURN &&
                             mappedInput.wasReleased(MappedInputManager::Button::Power);
  const bool nextTriggered = usePressForPageTurn
                                 ? (mappedInput.wasPressed(MappedInputManager::Button::PageForward) || powerPageTurn ||
                                    mappedInput.wasPressed(MappedInputManager::Button::FrontPageForward))
                                 : (mappedInput.wasReleased(MappedInputManager::Button::PageForward) || powerPageTurn ||
                                    mappedInput.wasReleased(MappedInputManager::Button::FrontPageForward));

  if (!prevTriggered && !nextTriggered) {
    return;
  }

  // Handle end of book
  if (currentPage >= xtc->getPageCount()) {
    currentPage = xtc->getPageCount() - 1;
    requestUpdate();
    return;
  }

  const bool skipPages = SETTINGS.longPressChapterSkip && mappedInput.getHeldTime() > skipPageMs;
  const int skipAmount = skipPages ? 10 : 1;

  if (prevTriggered) {
    if (currentPage >= static_cast<uint32_t>(skipAmount)) {
      currentPage -= skipAmount;
    } else {
      currentPage = 0;
    }
    PET_MANAGER.onPageTurn();
    requestUpdate();
  } else if (nextTriggered) {
    currentPage += skipAmount;
    if (currentPage >= xtc->getPageCount()) {
      currentPage = xtc->getPageCount();  // Allow showing "End of book"
    }
    PET_MANAGER.onPageTurn();
    requestUpdate();
  }
}

void XtcReaderActivity::render(RenderLock&&) {
  if (!xtc) {
    return;
  }

  // Bounds check
  if (currentPage >= xtc->getPageCount()) {
    // Show end of book screen
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_END_OF_BOOK), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  renderPage();
  saveProgress();
}

namespace {
// Strip width for 2-bit column-major rendering to reduce peak RAM usage.
// Each strip uses STRIP_COLS * colBytes * 2 planes bytes.
// For 800-height display: colBytes=100, STRIP_COLS=48 → 48*100*2 = 9,600 bytes per strip.
constexpr uint16_t STRIP_COLS = 48;
}  // namespace

// Render 2-bit grayscale page using strip-based approach to minimize peak RAM.
// Instead of loading the entire 96KB page buffer, loads STRIP_COLS columns at a time (~9.6KB).
// Requires 4 rendering passes × (width/STRIP_COLS) SD seeks each, but avoids OOM crash.
static bool renderPage2bitStrip(GfxRenderer& renderer, Xtc* xtc, uint32_t currentPage,
                                int& pagesUntilFullRefresh) {
  const uint16_t pageWidth = xtc->getPageWidth();
  const uint16_t pageHeight = xtc->getPageHeight();
  const size_t colBytes = (pageHeight + 7) / 8;  // Bytes per column (100 for 800-height)

  // Strip buffer: STRIP_COLS columns × colBytes × 2 planes
  const uint16_t stripCols = STRIP_COLS;
  const size_t stripBufSize = stripCols * colBytes * 2;

  LOG_DBG("XTR", "2-bit strip render: page=%lu, strip=%u cols, stripBuf=%lu bytes, freeHeap=%lu",
          currentPage, stripCols, stripBufSize, (unsigned long)ESP.getFreeHeap());

  uint8_t* stripBuf = static_cast<uint8_t*>(malloc(stripBufSize));
  if (!stripBuf) {
    LOG_ERR("XTR", "Failed to allocate strip buffer (%lu bytes)", stripBufSize);
    return false;
  }

  // Rendering pass helper: iterate column strips, load data, draw pixels based on pass type.
  // Returns false on SD read error. Does NOT free stripBuf — caller owns the buffer.
  // passFlag: 0=BW(>=1), 1=LSB(==1), 2=MSB(==1||==2), 3=reBW(>=1)
  bool passOk = true;
  auto doPass = [&](uint8_t passFlag, bool drawBlack) {
    for (uint16_t stripStart = 0; stripStart < pageWidth && passOk; stripStart += stripCols) {
      const uint16_t colsThisStrip = (stripStart + stripCols <= pageWidth) ? stripCols : (pageWidth - stripStart);

      // Map screen x-range [stripStart .. stripStart+colsThisStrip) to file column indices.
      // XTH column-major: file col 0 = rightmost screen pixel (x = width-1), col increases as x decreases.
      // File col for screen x: fileCol = pageWidth - 1 - x
      // In ascending file-col order (needed for contiguous read):
      //   fileColStart = pageWidth - 1 - (stripStart + colsThisStrip - 1)
      const size_t fileColStart = pageWidth - 1 - (stripStart + colsThisStrip - 1);

      // Load colsThisStrip consecutive file columns for each plane into stripBuf
      if (!xtc->loadPagePlaneStrip(currentPage, 0, fileColStart, colsThisStrip, colBytes, stripBuf) ||
          !xtc->loadPagePlaneStrip(currentPage, 1, fileColStart, colsThisStrip, colBytes,
                                   stripBuf + colsThisStrip * colBytes)) {
        passOk = false;
        break;
      }

      // Process pixels in this strip (x from stripStart to stripStart+colsThisStrip-1)
      for (uint16_t x = stripStart; x < stripStart + colsThisStrip; x++) {
        const size_t physCol = pageWidth - 1 - x;
        // Local col index within strip buffer (file col order: ascending)
        const size_t localCol = physCol - fileColStart;
        for (uint16_t y = 0; y < pageHeight; y++) {
          const size_t byteInCol = y / 8;
          const size_t bitInByte = 7 - (y % 8);
          const size_t byteOffset = localCol * colBytes + byteInCol;
          const uint8_t bit1 = (stripBuf[byteOffset] >> bitInByte) & 1;
          const uint8_t bit2 = (stripBuf[colsThisStrip * colBytes + byteOffset] >> bitInByte) & 1;
          const uint8_t pv = (bit1 << 1) | bit2;

          bool draw = false;
          switch (passFlag) {
            case 0: draw = (pv >= 1); break;              // BW: all non-white
            case 1: draw = (pv == 1); break;              // LSB: dark grey only
            case 2: draw = (pv == 1 || pv == 2); break;  // MSB: both grays
            case 3: draw = (pv >= 1); break;              // re-BW: same as pass 0
          }
          if (draw) {
            renderer.drawPixel(x, y, drawBlack);
          }
        }
      }
    }
  };

  renderer.clearScreen();

  // Pass 1: BW buffer - draw all non-white pixels as black
  doPass(0, true);
  if (!passOk) { free(stripBuf); return false; }

  // Display BW
  if (pagesUntilFullRefresh <= 1) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
  } else {
    renderer.displayBuffer();
    pagesUntilFullRefresh--;
  }

  // Pass 2: LSB buffer - dark grey only
  renderer.clearScreen(0x00);
  doPass(1, false);
  if (!passOk) { free(stripBuf); return false; }
  renderer.copyGrayscaleLsbBuffers();

  // Pass 3: MSB buffer - both grays
  renderer.clearScreen(0x00);
  doPass(2, false);
  if (!passOk) { free(stripBuf); return false; }
  renderer.copyGrayscaleMsbBuffers();

  // Display grayscale overlay
  renderer.displayGrayBuffer();

  // Pass 4: Re-render BW to framebuffer (restore for next frame)
  renderer.clearScreen();
  doPass(3, true);
  if (!passOk) { free(stripBuf); return false; }
  renderer.cleanupGrayscaleWithFrameBuffer();

  free(stripBuf);
  LOG_DBG("XTR", "Rendered page %lu (2-bit strip, freeHeap=%lu)", currentPage + 1,
          (unsigned long)ESP.getFreeHeap());
  return true;
}

void XtcReaderActivity::renderPage() {
  const uint16_t pageWidth = xtc->getPageWidth();
  const uint16_t pageHeight = xtc->getPageHeight();
  const uint8_t bitDepth = xtc->getBitDepth();

  if (bitDepth == 2) {
    // XTH 2-bit grayscale: use strip-based rendering to avoid 96KB allocation.
    // Strip-based approach uses ~9.6KB instead of 96KB at the cost of more SD seeks.
    LOG_DBG("XTR", "Free heap before 2-bit render: %lu", (unsigned long)ESP.getFreeHeap());
    if (!renderPage2bitStrip(renderer, xtc.get(), currentPage, pagesUntilFullRefresh)) {
      renderer.clearScreen();
      renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_MEMORY_ERROR), true, EpdFontFamily::BOLD);
      renderer.displayBuffer();
    }
    return;
  }

  // 1-bit mode: ((width+7)/8) * height bytes = 48KB for 480x800
  const size_t pageBufferSize = ((pageWidth + 7) / 8) * pageHeight;

  LOG_DBG("XTR", "Free heap before 1-bit alloc: %lu, need: %lu", (unsigned long)ESP.getFreeHeap(),
          (unsigned long)pageBufferSize);

  // Allocate page buffer
  uint8_t* pageBuffer = static_cast<uint8_t*>(malloc(pageBufferSize));
  if (!pageBuffer) {
    LOG_ERR("XTR", "Failed to allocate page buffer (%lu bytes)", pageBufferSize);
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_MEMORY_ERROR), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  // Load page data
  size_t bytesRead = xtc->loadPage(currentPage, pageBuffer, pageBufferSize);
  if (bytesRead == 0) {
    LOG_ERR("XTR", "Failed to load page %lu", currentPage);
    free(pageBuffer);
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_PAGE_LOAD_ERROR), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  // Clear screen first
  renderer.clearScreen();

  // 1-bit mode: 8 pixels per byte, MSB first
  // XTC/XTCH pages are pre-rendered with status bar included, so render full page
  const size_t srcRowBytes = (pageWidth + 7) / 8;  // 60 bytes for 480 width

  for (uint16_t srcY = 0; srcY < pageHeight; srcY++) {
    const size_t srcRowStart = srcY * srcRowBytes;

    for (uint16_t srcX = 0; srcX < pageWidth; srcX++) {
      // Read source pixel (MSB first, bit 7 = leftmost pixel)
      const size_t srcByte = srcRowStart + srcX / 8;
      const size_t srcBit = 7 - (srcX % 8);
      const bool isBlack = !((pageBuffer[srcByte] >> srcBit) & 1);  // XTC: 0 = black, 1 = white

      if (isBlack) {
        renderer.drawPixel(srcX, srcY, true);
      }
    }
  }
  // White pixels are already cleared by clearScreen()

  free(pageBuffer);

  // XTC pages already have status bar pre-rendered, no need to add our own

  // Display with appropriate refresh
  if (pagesUntilFullRefresh <= 1) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
  } else {
    renderer.displayBuffer();
    pagesUntilFullRefresh--;
  }

  LOG_DBG("XTR", "Rendered page %lu/%lu (1-bit)", currentPage + 1, xtc->getPageCount());
}

void XtcReaderActivity::saveProgress() const {
  FsFile f;
  if (Storage.openFileForWrite("XTR", xtc->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    data[0] = currentPage & 0xFF;
    data[1] = (currentPage >> 8) & 0xFF;
    data[2] = (currentPage >> 16) & 0xFF;
    data[3] = (currentPage >> 24) & 0xFF;
    f.write(data, 4);
    f.close();
  }
}

void XtcReaderActivity::loadProgress() {
  FsFile f;
  if (Storage.openFileForRead("XTR", xtc->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    if (f.read(data, 4) == 4) {
      currentPage = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
      LOG_DBG("XTR", "Loaded progress: page %lu", currentPage);

      // Validate page number
      if (currentPage >= xtc->getPageCount()) {
        currentPage = 0;
      }
    }
    f.close();
  }
}
