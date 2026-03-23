#include "EpubReaderActivity.h"

#include <Epub/Page.h>
#include <Epub/blocks/TextBlock.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <memory>

#include "CrossPointSettings.h"
#include "util/PowerButtonClickDetector.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"
#include "CrossPointState.h"
#include "EpubReaderChapterSelectionActivity.h"
#include "EpubReaderFootnotesActivity.h"
#include "EpubReaderPercentSelectionActivity.h"
#include "KOReaderCredentialStore.h"
#include "KOReaderSyncActivity.h"
#include "MappedInputManager.h"
#include "QrDisplayActivity.h"
#include "ReaderUtils.h"
#include "RecentBooksStore.h"
#include "ReadingStats.h"
#include "StarredPagesActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ScreenshotUtil.h"

namespace {
// pagesPerRefresh now comes from SETTINGS.getRefreshFrequency()
constexpr unsigned long skipChapterMs = 700;
constexpr unsigned long goHomeMs = 1000;
// Max pages per minute for auto-turn (setting range 1-20)
constexpr uint8_t AUTO_TURN_MAX_PPM = 20;

int clampPercent(int percent) {
  if (percent < 0) {
    return 0;
  }
  if (percent > 100) {
    return 100;
  }
  return percent;
}

}  // namespace

void EpubReaderActivity::onEnter() {
  Activity::onEnter();
  if (!epub) {
    return;
  }

  // Configure screen orientation based on settings
  // NOTE: This affects layout math and must be applied before any render calls.
  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);

  // Apply text darkness setting for grayscale/AA rendering
  renderer.setTextDarkness(SETTINGS.textDarkness);

  epub->setupCacheDir();

  FsFile f;
  if (Storage.openFileForRead("ERS", epub->getCachePath() + "/progress.bin", f)) {
    uint8_t data[6];
    int dataSize = f.read(data, 6);
    if (dataSize == 4 || dataSize == 6) {
      currentSpineIndex = data[0] + (data[1] << 8);
      nextPageNumber = data[2] + (data[3] << 8);
      cachedSpineIndex = currentSpineIndex;
      LOG_DBG("ERS", "Loaded cache: %d, %d", currentSpineIndex, nextPageNumber);
    }
    if (dataSize == 6) {
      cachedChapterTotalPageCount = data[4] + (data[5] << 8);
    }
    f.close();
  }
  // We may want a better condition to detect if we are opening for the first time.
  // This will trigger if the book is re-opened at Chapter 0.
  if (currentSpineIndex == 0) {
    int textSpineIndex = epub->getSpineIndexForTextReference();
    if (textSpineIndex != 0) {
      currentSpineIndex = textSpineIndex;
      LOG_DBG("ERS", "Opened for first time, navigating to text reference at index %d", textSpineIndex);
    }
  }

  // Load bookmarks for this book
  bookmarkStore.load(epub->getCachePath());

  // Save current epub as last opened epub and add to recent books
  APP_STATE.openEpubPath = epub->getPath();
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(epub->getPath(), epub->getTitle(), epub->getAuthor(), epub->getThumbBmpPath());

  // Begin tracking how long this reading session lasts
  READ_STATS.startSession();
  sessionStartMs = millis();

  // Auto-activate page turn if persistent setting is configured
  if (SETTINGS.autoPageTurnEnabled && SETTINGS.autoPageTurnSpeed > 0) {
    toggleAutoPageTurn(SETTINGS.autoPageTurnSpeed);
  }

  // Trigger first update
  requestUpdate();
}

void EpubReaderActivity::onExit() {
  Activity::onExit();

  // Reset text darkness to normal for UI screens
  renderer.setTextDarkness(0);

  // Request half refresh for the next screen to clear accumulated reader ghosting
  renderer.requestNextHalfRefresh();

  // Accumulate reading time and record book progress before resetting state
  uint8_t progress = 0;
  const char* title = epub ? epub->getTitle().c_str() : nullptr;
  if (epub && epub->getBookSize() > 0 && section && section->pageCount > 0) {
    const float chapterProgress =
        static_cast<float>(section->currentPage) / static_cast<float>(section->pageCount);
    progress = static_cast<uint8_t>(
        clampPercent(static_cast<int>(epub->calculateProgress(currentSpineIndex, chapterProgress) * 100.0f + 0.5f)));
  }
  const char* bookPath = epub ? epub->getPath().c_str() : nullptr;
  READ_STATS.endSession(title, progress, bookPath);

  // Save bookmarks before exit
  bookmarkStore.save();

  // Reset orientation back to portrait for the rest of the UI
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  section.reset();
  epub.reset();
}

void EpubReaderActivity::loop() {
  if (!epub) {
    // Should never happen
    finish();
    return;
  }

  // Dismiss chapter popup after 2s or any button press
  if (showChapterPopup) {
    const bool anyBtn = mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
                         mappedInput.wasReleased(MappedInputManager::Button::Back) ||
                         mappedInput.wasReleased(MappedInputManager::Button::Left) ||
                         mappedInput.wasReleased(MappedInputManager::Button::PageBack);
    if (millis() - chapterPopupTime > 2000 || anyBtn) {
      showChapterPopup = false;
      // Request half refresh to clear popup ghosting on next render
      renderer.requestNextHalfRefresh();
      // Check for milestone that fired during chapter popup
      if (!showMilestoneToast) {
        auto milestone = PET_MANAGER.consumePendingMilestone();
        if (milestone != PetManager::Milestone::NONE) {
          uint16_t val = PET_MANAGER.getLastMilestoneValue();
          switch (milestone) {
            case PetManager::Milestone::DAILY_GOAL:
              snprintf(milestoneText, sizeof(milestoneText), "%s", tr(STR_MILESTONE_DAILY_GOAL)); break;
            case PetManager::Milestone::STREAK_UP:
              snprintf(milestoneText, sizeof(milestoneText), tr(STR_MILESTONE_STREAK_UP), val); break;
            case PetManager::Milestone::PAGE_MILESTONE:
              snprintf(milestoneText, sizeof(milestoneText), tr(STR_MILESTONE_PAGES), (unsigned long)val); break;
            default: break;
          }
          showMilestoneToast = true;
          milestoneToastTime = millis();
        }
      }
      requestUpdate();
    }
    return;  // block other input while popup is shown
  }

  // Dismiss milestone toast after 1.5s (non-blocking — doesn't consume input)
  if (showMilestoneToast && millis() - milestoneToastTime > 1500) {
    showMilestoneToast = false;
    requestUpdate();
  }

  if (automaticPageTurnActive) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
        mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      automaticPageTurnActive = false;
      // Persist cancellation so auto-turn does not re-activate on next session
      SETTINGS.autoPageTurnEnabled = 0;
      SETTINGS.saveToFile();
      // updates chapter title space to indicate page turn disabled
      requestUpdate();
      return;
    }

    if (!section) {
      requestUpdate();
      return;
    }

    // Skips page turn if renderingMutex is busy
    if (RenderLock::peek()) {
      lastPageTurnTime = millis();
      return;
    }

    if ((millis() - lastPageTurnTime) >= pageTurnDuration) {
      pageTurn(true);
      return;
    }
  }
  // Power button multi-click: block front buttons toggle
  if (getPowerClickAction() == CrossPointSettings::SHORT_PWRBTN::BLOCK_FRONT) {
    ignoreFrontButtons = !ignoreFrontButtons;
  }

  // Power button multi-click: toggle auto page turn on/off
  if (getPowerClickAction() == CrossPointSettings::SHORT_PWRBTN::AUTO_PAGE_TURN) {
    SETTINGS.autoPageTurnEnabled = SETTINGS.autoPageTurnEnabled ? 0 : 1;
    if (SETTINGS.autoPageTurnEnabled && SETTINGS.autoPageTurnSpeed == 0) {
      SETTINGS.autoPageTurnSpeed = 1;
    }
    SETTINGS.saveToFile();
    toggleAutoPageTurn(SETTINGS.autoPageTurnEnabled ? SETTINGS.autoPageTurnSpeed : 0);
  }

  // Long-press Confirm = star/unstar current page
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= 700 && !ignoreFrontButtons) {
    if (section && section->currentPage >= 0 && section->currentPage < section->pageCount) {
      const uint16_t si = static_cast<uint16_t>(currentSpineIndex);
      const uint16_t pg = static_cast<uint16_t>(section->currentPage);
      const bool wasStarred = bookmarkStore.has(si, pg);
      // Extract text snippet from current page for bookmark label
      std::string snippet;
      if (!wasStarred) {
        auto page = section->loadPageFromSectionFile();
        if (page) {
          for (const auto& el : page->elements) {
            if (el->getTag() == TAG_PageLine) {
              const auto& line = static_cast<const PageLine&>(*el);
              for (const auto& w : line.getBlock()->getWords()) {
                if (!snippet.empty()) snippet += ' ';
                snippet += w;
                if (snippet.size() >= 60) break;
              }
            }
            if (snippet.size() >= 60) break;
          }
        }
      }
      bookmarkStore.toggle(si, pg, snippet);
      GUI.drawPopup(renderer, wasStarred ? tr(STR_PAGE_UNSTARRED) : tr(STR_PAGE_STARRED));
      renderer.displayBuffer();
      delay(600);
      requestUpdate();
    }
    return;
  }

  // Short-press Confirm = enter reader menu activity.
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && !ignoreFrontButtons) {
    const int currentPage = section ? section->currentPage + 1 : 0;
    const int totalPages = section ? section->pageCount : 0;
    float bookProgress = 0.0f;
    if (epub->getBookSize() > 0 && section && section->pageCount > 0) {
      const float chapterProgress = static_cast<float>(section->currentPage) / static_cast<float>(section->pageCount);
      bookProgress = epub->calculateProgress(currentSpineIndex, chapterProgress) * 100.0f;
    }
    const int bookProgressPercent = clampPercent(static_cast<int>(bookProgress + 0.5f));
    // Resolve current chapter title for menu; fall back to book title if no TOC entry
    std::string menuTitle = epub->getTitle();
    const int menuTocIdx = resolveCurrentTocIndex();
    if (menuTocIdx != -1) {
      const auto menuTocItem = epub->getTocItem(menuTocIdx);
      if (!menuTocItem.title.empty()) {
        menuTitle = menuTocItem.title;
      }
    }
    // Capture font state before opening menu to detect changes requiring re-layout
    const uint8_t prevFontFamily = SETTINGS.fontFamily;
    const uint8_t prevFontSize = SETTINGS.fontSize;
    startActivityForResult(std::make_unique<EpubReaderMenuActivity>(
                               renderer, mappedInput, menuTitle, currentPage, totalPages, bookProgressPercent,
                               SETTINGS.orientation, !currentPageFootnotes.empty(), !bookmarkStore.isEmpty(),
                               SETTINGS.autoPageTurnEnabled ? SETTINGS.autoPageTurnSpeed : 0),
                           [this, prevFontFamily, prevFontSize](const ActivityResult& result) {
                             // Always apply orientation change even if the menu was cancelled
                             const auto& menu = std::get<MenuResult>(result.data);
                             applyOrientation(menu.orientation);
                             // Apply auto page turn based on enabled flag set in menu
                             toggleAutoPageTurn(SETTINGS.autoPageTurnEnabled ? SETTINGS.autoPageTurnSpeed : 0);
                             // Reset section if font changed to force re-layout with new font/size
                             if (SETTINGS.fontFamily != prevFontFamily || SETTINGS.fontSize != prevFontSize) {
                               RenderLock lock(*this);
                               if (section) {
                                 cachedSpineIndex = currentSpineIndex;
                                 cachedChapterTotalPageCount = section->pageCount;
                                 nextPageNumber = section->currentPage;
                               }
                               section.reset();
                             }
                             if (!result.isCancelled) {
                               onReaderMenuConfirm(static_cast<EpubReaderMenuActivity::MenuAction>(menu.action));
                             }
                           });
  }

  // Long press BACK (1s+) goes to file selection
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= ReaderUtils::GO_HOME_MS &&
      !ignoreFrontButtons) {
    activityManager.goToFileBrowser(epub ? epub->getPath() : "");
    return;
  }

  // Short press BACK goes directly to home (or restores position if viewing footnote)
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) &&
      mappedInput.getHeldTime() < ReaderUtils::GO_HOME_MS && !ignoreFrontButtons) {
    if (footnoteDepth > 0) {
      restoreSavedPosition();
      return;
    }
    onGoHome();
    return;
  }

  // Star page toggle via power button multi-click
  if (getPowerClickAction() == CrossPointSettings::SHORT_PWRBTN::STAR_PAGE) {
    if (section && section->currentPage >= 0 && section->currentPage < section->pageCount) {
      const uint16_t si = static_cast<uint16_t>(currentSpineIndex);
      const uint16_t pg = static_cast<uint16_t>(section->currentPage);
      const bool wasStarred = bookmarkStore.has(si, pg);
      std::string snippet;
      if (!wasStarred) {
        auto page = section->loadPageFromSectionFile();
        if (page) {
          for (const auto& el : page->elements) {
            if (el->getTag() == TAG_PageLine) {
              const auto& line = static_cast<const PageLine&>(*el);
              for (const auto& w : line.getBlock()->getWords()) {
                if (!snippet.empty()) snippet += ' ';
                snippet += w;
                if (snippet.size() >= 60) break;
              }
            }
            if (snippet.size() >= 60) break;
          }
        }
      }
      bookmarkStore.toggle(si, pg, snippet);
      GUI.drawPopup(renderer, wasStarred ? tr(STR_PAGE_UNSTARRED) : tr(STR_PAGE_STARRED));
      renderer.displayBuffer();
      delay(600);
      requestUpdate();
    }
    return;
  }

  auto [prevTriggered, nextTriggered] = ReaderUtils::detectPageTurn(mappedInput, ignoreFrontButtons);
  if (!prevTriggered && !nextTriggered) {
    return;
  }

  // any botton press when at end of the book goes back to the last page
  if (currentSpineIndex > 0 && currentSpineIndex >= epub->getSpineItemsCount()) {
    currentSpineIndex = epub->getSpineItemsCount() - 1;
    nextPageNumber = UINT16_MAX;
    requestUpdate();
    return;
  }

  const bool skipChapter = SETTINGS.longPressChapterSkip && mappedInput.getHeldTime() > skipChapterMs;

  if (skipChapter) {
    lastPageTurnTime = millis();
    // We don't want to delete the section mid-render, so grab the semaphore
    {
      RenderLock lock(*this);
      nextPageNumber = 0;
      currentSpineIndex = nextTriggered ? currentSpineIndex + 1 : currentSpineIndex - 1;
      section.reset();
    }
    requestUpdate();
    return;
  }

  // No current section, attempt to rerender the book
  if (!section) {
    requestUpdate();
    return;
  }

  if (prevTriggered) {
    pageTurn(false);
  } else {
    pageTurn(true);
  }
}

// Translate an absolute percent into a spine index plus a normalized position
// within that spine so we can jump after the section is loaded.
void EpubReaderActivity::jumpToPercent(int percent) {
  if (!epub) {
    return;
  }

  const size_t bookSize = epub->getBookSize();
  if (bookSize == 0) {
    return;
  }

  // Normalize input to 0-100 to avoid invalid jumps.
  percent = clampPercent(percent);

  // Convert percent into a byte-like absolute position across the spine sizes.
  // Use an overflow-safe computation: (bookSize / 100) * percent + (bookSize % 100) * percent / 100
  size_t targetSize =
      (bookSize / 100) * static_cast<size_t>(percent) + (bookSize % 100) * static_cast<size_t>(percent) / 100;
  if (percent >= 100) {
    // Ensure the final percent lands inside the last spine item.
    targetSize = bookSize - 1;
  }

  const int spineCount = epub->getSpineItemsCount();
  if (spineCount == 0) {
    return;
  }

  int targetSpineIndex = spineCount - 1;
  size_t prevCumulative = 0;

  for (int i = 0; i < spineCount; i++) {
    const size_t cumulative = epub->getCumulativeSpineItemSize(i);
    if (targetSize <= cumulative) {
      // Found the spine item containing the absolute position.
      targetSpineIndex = i;
      prevCumulative = (i > 0) ? epub->getCumulativeSpineItemSize(i - 1) : 0;
      break;
    }
  }

  const size_t cumulative = epub->getCumulativeSpineItemSize(targetSpineIndex);
  const size_t spineSize = (cumulative > prevCumulative) ? (cumulative - prevCumulative) : 0;
  // Store a normalized position within the spine so it can be applied once loaded.
  pendingSpineProgress =
      (spineSize == 0) ? 0.0f : static_cast<float>(targetSize - prevCumulative) / static_cast<float>(spineSize);
  if (pendingSpineProgress < 0.0f) {
    pendingSpineProgress = 0.0f;
  } else if (pendingSpineProgress > 1.0f) {
    pendingSpineProgress = 1.0f;
  }

  // Reset state so render() reloads and repositions on the target spine.
  {
    RenderLock lock(*this);
    currentSpineIndex = targetSpineIndex;
    nextPageNumber = 0;
    pendingPercentJump = true;
    section.reset();
  }
}

void EpubReaderActivity::onReaderMenuConfirm(EpubReaderMenuActivity::MenuAction action) {
  switch (action) {
    case EpubReaderMenuActivity::MenuAction::SELECT_CHAPTER: {
      const int spineIdx = currentSpineIndex;
      const std::string path = epub->getPath();
      startActivityForResult(
          std::make_unique<EpubReaderChapterSelectionActivity>(renderer, mappedInput, epub, path, spineIdx),
          [this](const ActivityResult& result) {
            if (!result.isCancelled) {
              const auto& chapterResult = std::get<ChapterResult>(result.data);
              // Navigate even if spine index is same — anchor may differ (e.g., same HTML file, different chapter)
              if (currentSpineIndex != chapterResult.spineIndex || !chapterResult.anchor.empty()) {
                RenderLock lock(*this);
                currentSpineIndex = chapterResult.spineIndex;
                pendingAnchor = chapterResult.anchor;
                nextPageNumber = 0;
                section.reset();
              }
            }
          });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::FOOTNOTES: {
      startActivityForResult(std::make_unique<EpubReaderFootnotesActivity>(renderer, mappedInput, currentPageFootnotes),
                             [this](const ActivityResult& result) {
                               if (!result.isCancelled) {
                                 const auto& footnoteResult = std::get<FootnoteResult>(result.data);
                                 navigateToHref(footnoteResult.href, true);
                               }
                               requestUpdate();
                             });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::GO_TO_PERCENT: {
      float bookProgress = 0.0f;
      if (epub && epub->getBookSize() > 0 && section && section->pageCount > 0) {
        const float chapterProgress = static_cast<float>(section->currentPage) / static_cast<float>(section->pageCount);
        bookProgress = epub->calculateProgress(currentSpineIndex, chapterProgress) * 100.0f;
      }
      const int initialPercent = clampPercent(static_cast<int>(bookProgress + 0.5f));
      startActivityForResult(
          std::make_unique<EpubReaderPercentSelectionActivity>(renderer, mappedInput, initialPercent),
          [this](const ActivityResult& result) {
            if (!result.isCancelled) {
              jumpToPercent(std::get<PercentResult>(result.data).percent);
            }
          });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::DISPLAY_QR: {
      if (section && section->currentPage >= 0 && section->currentPage < section->pageCount) {
        auto p = section->loadPageFromSectionFile();
        if (p) {
          std::string fullText;
          for (const auto& el : p->elements) {
            if (el->getTag() == TAG_PageLine) {
              const auto& line = static_cast<const PageLine&>(*el);
              if (line.getBlock()) {
                const auto& words = line.getBlock()->getWords();
                for (const auto& w : words) {
                  if (!fullText.empty()) fullText += " ";
                  fullText += w;
                }
              }
            }
          }
          if (!fullText.empty()) {
            startActivityForResult(std::make_unique<QrDisplayActivity>(renderer, mappedInput, fullText),
                                   [this](const ActivityResult& result) {});
            break;
          }
        }
      }
      // If no text or page loading failed, just close menu
      requestUpdate();
      break;
    }
    case EpubReaderMenuActivity::MenuAction::STARRED_PAGES: {
      startActivityForResult(
          std::make_unique<StarredPagesActivity>(renderer, mappedInput, bookmarkStore.getAll(), epub),
          [this](const ActivityResult& result) {
            if (!result.isCancelled) {
              const auto& starred = std::get<StarredPageResult>(result.data);
              if (currentSpineIndex != starred.spineIndex || !section || section->currentPage != starred.pageNumber) {
                RenderLock lock(*this);
                currentSpineIndex = starred.spineIndex;
                nextPageNumber = starred.pageNumber;
                section.reset();
              }
            }
          });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::GO_HOME: {
      onGoHome();
      return;
    }
    case EpubReaderMenuActivity::MenuAction::DELETE_CACHE: {
      {
        RenderLock lock(*this);
        if (epub && section) {
          uint16_t backupSpine = currentSpineIndex;
          uint16_t backupPage = section->currentPage;
          uint16_t backupPageCount = section->pageCount;
          section.reset();
          epub->clearCache();
          epub->setupCacheDir();
          saveProgress(backupSpine, backupPage, backupPageCount);
          if (!bookmarkStore.isEmpty()) {
            bookmarkStore.markDirty();
            bookmarkStore.save();
          }
        }
      }
      onGoHome();
      return;
    }
    case EpubReaderMenuActivity::MenuAction::SCREENSHOT: {
      {
        RenderLock lock(*this);
        pendingScreenshot = true;
      }
      requestUpdate();
      break;
    }
    case EpubReaderMenuActivity::MenuAction::SYNC: {
      if (KOREADER_STORE.hasCredentials()) {
        const int currentPage = section ? section->currentPage : 0;
        const int totalPages = section ? section->pageCount : 0;
        startActivityForResult(
            std::make_unique<KOReaderSyncActivity>(renderer, mappedInput, epub, epub->getPath(), currentSpineIndex,
                                                   currentPage, totalPages),
            [this](const ActivityResult& result) {
              if (!result.isCancelled) {
                const auto& sync = std::get<SyncResult>(result.data);
                if (currentSpineIndex != sync.spineIndex || (section && section->currentPage != sync.page)) {
                  RenderLock lock(*this);
                  currentSpineIndex = sync.spineIndex;
                  nextPageNumber = sync.page;
                  section.reset();
                }
              }
            });
      }
      break;
    }
  }
}

void EpubReaderActivity::applyOrientation(const uint8_t orientation) {
  // No-op if the selected orientation matches current settings.
  if (SETTINGS.orientation == orientation) {
    return;
  }

  // Preserve current reading position so we can restore after reflow.
  {
    RenderLock lock(*this);
    if (section) {
      cachedSpineIndex = currentSpineIndex;
      cachedChapterTotalPageCount = section->pageCount;
      nextPageNumber = section->currentPage;
    }

    // Persist the selection so the reader keeps the new orientation on next launch.
    SETTINGS.orientation = orientation;
    SETTINGS.saveToFile();

    // Update renderer orientation to match the new logical coordinate system.
    ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);

    // Reset section to force re-layout in the new orientation.
    section.reset();
  }
}

void EpubReaderActivity::toggleAutoPageTurn(const uint8_t selectedPageTurnOption) {
  // selectedPageTurnOption is now a direct pages-per-minute value (0=off, 1-20=ppm)
  if (selectedPageTurnOption == 0 || selectedPageTurnOption > AUTO_TURN_MAX_PPM) {
    automaticPageTurnActive = false;
    return;
  }

  lastPageTurnTime = millis();
  // Calculate milliseconds per page from pages-per-minute
  pageTurnDuration = (1UL * 60 * 1000) / selectedPageTurnOption;
  automaticPageTurnActive = true;

  const uint8_t statusBarHeight = UITheme::getInstance().getStatusBarHeight();
  // resets cached section so that space is reserved for auto page turn indicator when None or progress bar only
  if (statusBarHeight == 0 || statusBarHeight == UITheme::getInstance().getProgressBarHeight()) {
    // Preserve current reading position so we can restore after reflow.
    RenderLock lock(*this);
    if (section) {
      cachedSpineIndex = currentSpineIndex;
      cachedChapterTotalPageCount = section->pageCount;
      nextPageNumber = section->currentPage;
    }
    section.reset();
  }
}

void EpubReaderActivity::pageTurn(bool isForwardTurn) {
  // Record manual page turn duration for reading time estimate (skip auto-turn)
  if (!automaticPageTurnActive && lastUserPageTurnMs > 0) {
    const unsigned long duration = millis() - lastUserPageTurnMs;
    // Only record plausible durations (1s-10min per page)
    if (duration >= 1000 && duration <= 600000) {
      pageTurnTimes[pageTurnTimesIdx] = duration;
      pageTurnTimesIdx = (pageTurnTimesIdx + 1) % 10;
      if (pageTurnTimesCount < 10) pageTurnTimesCount++;
    }
  }
  if (!automaticPageTurnActive) {
    lastUserPageTurnMs = millis();
  }

  if (isForwardTurn) {
    if (section->currentPage < section->pageCount - 1) {
      section->currentPage++;
    } else {
      // We don't want to delete the section mid-render, so grab the semaphore
      {
        RenderLock lock(*this);
        nextPageNumber = 0;
        currentSpineIndex++;
        section.reset();
      }
      // Chapter completion: always give +5 happiness, show popup every 3rd chapter
      if (PET_MANAGER.exists() && PET_MANAGER.isAlive() &&
          currentSpineIndex > lastCompletedSpineIndex &&
          currentSpineIndex < epub->getSpineItemsCount()) {
        lastCompletedSpineIndex = currentSpineIndex;
        chapterCompleteCount++;
        PET_MANAGER.onChapterComplete();
        if (chapterCompleteCount % 3 == 0) {
          showChapterPopup = true;
          chapterPopupTime = millis();
        }
      }
    }
  } else {
    if (section->currentPage > 0) {
      section->currentPage--;
    } else if (currentSpineIndex > 0) {
      // We don't want to delete the section mid-render, so grab the semaphore
      {
        RenderLock lock(*this);
        nextPageNumber = UINT16_MAX;
        currentSpineIndex--;
        section.reset();
      }
    }
  }
  // Clear any showing milestone toast on page turn (one page of visibility is enough)
  showMilestoneToast = false;

  PET_MANAGER.onPageTurn();

  // Check for milestone toast (only if no chapter popup is showing)
  if (!showChapterPopup) {
    auto milestone = PET_MANAGER.consumePendingMilestone();
    if (milestone != PetManager::Milestone::NONE) {
      uint16_t val = PET_MANAGER.getLastMilestoneValue();
      switch (milestone) {
        case PetManager::Milestone::DAILY_GOAL:
          snprintf(milestoneText, sizeof(milestoneText), "%s", tr(STR_MILESTONE_DAILY_GOAL));
          break;
        case PetManager::Milestone::STREAK_UP:
          snprintf(milestoneText, sizeof(milestoneText), tr(STR_MILESTONE_STREAK_UP), val);
          break;
        case PetManager::Milestone::PAGE_MILESTONE:
          snprintf(milestoneText, sizeof(milestoneText), tr(STR_MILESTONE_PAGES), (unsigned long)val);
          break;
        default: break;
      }
      showMilestoneToast = true;
      milestoneToastTime = millis();
    }
  }

  lastPageTurnTime = millis();
  requestUpdate();
}

// TODO: Failure handling
void EpubReaderActivity::render(RenderLock&& lock) {
  if (!epub) {
    return;
  }

  // edge case handling for sub-zero spine index
  if (currentSpineIndex < 0) {
    currentSpineIndex = 0;
  }
  // based bounds of book, show end of book screen
  if (currentSpineIndex > epub->getSpineItemsCount()) {
    currentSpineIndex = epub->getSpineItemsCount();
  }

  // Show end of book screen
  if (currentSpineIndex == epub->getSpineItemsCount()) {
    if (!bookCompletionNotified && PET_MANAGER.exists() && PET_MANAGER.isAlive()) {
      PET_MANAGER.onBookFinished();
      bookCompletionNotified = true;
    }
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_END_OF_BOOK), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    automaticPageTurnActive = false;
    return;
  }

  // Chapter completion popup — overlay on existing page content (no clearScreen to avoid e-ink flash)
  if (showChapterPopup) {
    const int screenW = renderer.getScreenWidth();
    const int screenH = renderer.getScreenHeight();
    const int popupW = 260, popupH = 130;
    const int px = (screenW - popupW) / 2;
    const int py = (screenH - popupH) / 2;
    renderer.fillRect(px, py, popupW, popupH, false);
    renderer.drawRect(px, py, popupW, popupH);
    renderer.drawRect(px + 2, py + 2, popupW - 4, popupH - 4);  // double border

    // Full-size pet sprite (48x48) centered at top of popup
    if (PET_MANAGER.exists()) {
      const auto& ps = PET_MANAGER.getState();
      PetSpriteRenderer::drawPet(renderer, px + (popupW - PetSpriteRenderer::SPRITE_W) / 2, py + 10,
                                 ps.stage, PET_MANAGER.getMood(), 1, ps.evolutionVariant, ps.petType);
    }

    // "Chapter Complete!" text below pet
    const int txtW = renderer.getTextWidth(SMALL_FONT_ID, tr(STR_CHAPTER_COMPLETE), EpdFontFamily::BOLD);
    renderer.drawText(SMALL_FONT_ID, px + (popupW - txtW) / 2, py + 68,
                      tr(STR_CHAPTER_COMPLETE), true, EpdFontFamily::BOLD);

    // "+5 happy" indicator
    char happyBuf[16];
    snprintf(happyBuf, sizeof(happyBuf), "+%d :)", PetConfig::CHAPTER_COMPLETE_HAPPINESS);
    const int happyW = renderer.getTextWidth(SMALL_FONT_ID, happyBuf);
    renderer.drawText(SMALL_FONT_ID, px + (popupW - happyW) / 2, py + 90, happyBuf, true);

    renderer.displayBuffer();
    return;
  }

  // Apply screen viewable areas and additional padding
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  orientedMarginTop += SETTINGS.screenMargin;
  orientedMarginLeft += SETTINGS.screenMargin;
  orientedMarginRight += SETTINGS.screenMargin;

  const uint8_t statusBarHeight = UITheme::getInstance().getStatusBarHeight();

  // reserves space for automatic page turn indicator when no status bar or progress bar only
  if (automaticPageTurnActive &&
      (statusBarHeight == 0 || statusBarHeight == UITheme::getInstance().getProgressBarHeight())) {
    orientedMarginBottom +=
        std::max(SETTINGS.screenMargin,
                 static_cast<uint8_t>(statusBarHeight + UITheme::getInstance().getMetrics().statusBarVerticalMargin));
  } else {
    orientedMarginBottom += std::max(SETTINGS.screenMargin, statusBarHeight);
  }

  if (!section) {
    const auto filepath = epub->getSpineItem(currentSpineIndex).href;
    LOG_DBG("ERS", "Loading file: %s, index: %d", filepath.c_str(), currentSpineIndex);
    section = std::unique_ptr<Section>(new Section(epub, currentSpineIndex, renderer));

    const uint16_t viewportWidth = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;
    const uint16_t viewportHeight = renderer.getScreenHeight() - orientedMarginTop - orientedMarginBottom;

    if (!section->loadSectionFile(SETTINGS.getReaderFontId(), SETTINGS.getReaderLineCompression(),
                                  SETTINGS.extraParagraphSpacing, SETTINGS.paragraphAlignment, viewportWidth,
                                  viewportHeight, SETTINGS.hyphenationEnabled, SETTINGS.embeddedStyle,
                                  SETTINGS.imageRendering)) {
      LOG_DBG("ERS", "Cache not found, building...");

      const auto popupFn = [this]() { GUI.drawPopup(renderer, tr(STR_INDEXING)); };

      if (!section->createSectionFile(SETTINGS.getReaderFontId(), SETTINGS.getReaderLineCompression(),
                                      SETTINGS.extraParagraphSpacing, SETTINGS.paragraphAlignment, viewportWidth,
                                      viewportHeight, SETTINGS.hyphenationEnabled, SETTINGS.embeddedStyle,
                                      SETTINGS.imageRendering, popupFn)) {
        LOG_ERR("ERS", "Failed to persist page data to SD");
        section.reset();
        return;
      }
    } else {
      LOG_DBG("ERS", "Cache found, skipping build...");
    }

    if (nextPageNumber == UINT16_MAX) {
      section->currentPage = section->pageCount - 1;
    } else {
      section->currentPage = nextPageNumber;
    }

    if (!pendingAnchor.empty()) {
      if (const auto page = section->getPageForAnchor(pendingAnchor)) {
        section->currentPage = *page;
        LOG_DBG("ERS", "Resolved anchor '%s' to page %d", pendingAnchor.c_str(), *page);
      } else {
        LOG_DBG("ERS", "Anchor '%s' not found in section %d", pendingAnchor.c_str(), currentSpineIndex);
      }
      pendingAnchor.clear();
    }

    // handles changes in reader settings and reset to approximate position based on cached progress
    if (cachedChapterTotalPageCount > 0) {
      // only goes to relative position if spine index matches cached value
      if (currentSpineIndex == cachedSpineIndex && section->pageCount != cachedChapterTotalPageCount) {
        float progress = static_cast<float>(section->currentPage) / static_cast<float>(cachedChapterTotalPageCount);
        int newPage = static_cast<int>(progress * section->pageCount);
        section->currentPage = newPage;
      }
      cachedChapterTotalPageCount = 0;  // resets to 0 to prevent reading cached progress again
    }

    if (pendingPercentJump && section->pageCount > 0) {
      // Apply the pending percent jump now that we know the new section's page count.
      int newPage = static_cast<int>(pendingSpineProgress * static_cast<float>(section->pageCount));
      if (newPage >= section->pageCount) {
        newPage = section->pageCount - 1;
      }
      section->currentPage = newPage;
      pendingPercentJump = false;
    }
  }

  renderer.clearScreen();

  if (section->pageCount == 0) {
    LOG_DBG("ERS", "No pages to render");
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_EMPTY_CHAPTER), true, EpdFontFamily::BOLD);
    renderStatusBar();
    renderer.displayBuffer();
    automaticPageTurnActive = false;
    return;
  }

  if (section->currentPage < 0 || section->currentPage >= section->pageCount) {
    LOG_DBG("ERS", "Page out of bounds: %d (max %d)", section->currentPage, section->pageCount);
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_OUT_OF_BOUNDS), true, EpdFontFamily::BOLD);
    renderStatusBar();
    renderer.displayBuffer();
    automaticPageTurnActive = false;
    return;
  }

  {
    auto p = section->loadPageFromSectionFile();
    if (!p) {
      LOG_ERR("ERS", "Failed to load page from SD - clearing section cache");
      section->clearCache();
      section.reset();
      requestUpdate();  // Try again after clearing cache
                        // TODO: prevent infinite loop if the page keeps failing to load for some reason
      automaticPageTurnActive = false;
      return;
    }

    // Collect footnotes from the loaded page
    currentPageFootnotes = std::move(p->footnotes);

    const auto start = millis();
    renderContents(std::move(p), orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft);
    LOG_DBG("ERS", "Rendered page in %dms", millis() - start);
    renderer.clearFontCache();
  }
  saveProgress(currentSpineIndex, section->currentPage, section->pageCount);

  if (pendingScreenshot) {
    pendingScreenshot = false;
    ScreenshotUtil::takeScreenshot(renderer);
  }
}

void EpubReaderActivity::saveProgress(int spineIndex, int currentPage, int pageCount) {
  // Guard: don't save out-of-bounds spine index (end-of-book state).
  // Otherwise reopening the book would jump to the "finished" screen.
  if (epub && spineIndex >= static_cast<int>(epub->getSpineItemsCount())) {
    spineIndex = epub->getSpineItemsCount() - 1;
    // Clamp to last page of last chapter
    currentPage = (pageCount > 0) ? pageCount - 1 : 0;
  }

  FsFile f;
  if (Storage.openFileForWrite("ERS", epub->getCachePath() + "/progress.bin", f)) {
    uint8_t data[6];
    data[0] = spineIndex & 0xFF;
    data[1] = (spineIndex >> 8) & 0xFF;
    data[2] = currentPage & 0xFF;
    data[3] = (currentPage >> 8) & 0xFF;
    data[4] = pageCount & 0xFF;
    data[5] = (pageCount >> 8) & 0xFF;
    f.write(data, 6);
    f.close();
    LOG_DBG("ERS", "Progress saved: Chapter %d, Page %d", spineIndex, currentPage);
  } else {
    LOG_ERR("ERS", "Could not save progress!");
  }

  // Update progress percent in recent books store for home screen display
  const float chapterProg = (pageCount > 0) ? static_cast<float>(currentPage) / pageCount : 0.0f;
  const float bookProg = epub->calculateProgress(currentSpineIndex, chapterProg) * 100.0f;
  const uint8_t percent = static_cast<uint8_t>(clampPercent(static_cast<int>(bookProg + 0.5f)));
  RECENT_BOOKS.updateBookProgress(epub->getPath(), percent);
}
void EpubReaderActivity::renderContents(std::unique_ptr<Page> page, const int orientedMarginTop,
                                        const int orientedMarginRight, const int orientedMarginBottom,
                                        const int orientedMarginLeft) {
  // Force special handling for pages with images when anti-aliasing is on
  bool imagePageWithAA = page->hasImages() && SETTINGS.textAntiAliasing;

  page->render(renderer, SETTINGS.getReaderFontId(), orientedMarginLeft, orientedMarginTop);
  renderStatusBar();

  // Milestone toast — small banner at bottom of page content area
  if (showMilestoneToast && milestoneText[0]) {
    const int screenW = renderer.getScreenWidth();
    const int toastW = renderer.getTextWidth(SMALL_FONT_ID, milestoneText) + 16;
    const int toastH = renderer.getLineHeight(SMALL_FONT_ID) + 8;
    const int tx = (screenW - toastW) / 2;
    const int ty = orientedMarginTop + 4;
    renderer.fillRect(tx, ty, toastW, toastH, false);
    renderer.drawRect(tx, ty, toastW, toastH);
    renderer.drawText(SMALL_FONT_ID, tx + 8, ty + 4, milestoneText, true, EpdFontFamily::BOLD);
  }

  if (imagePageWithAA) {
    // Double FAST_REFRESH with selective image blanking (pablohc's technique):
    // HALF_REFRESH sets particles too firmly for the grayscale LUT to adjust.
    // Instead, blank only the image area and do two fast refreshes.
    // Step 1: Display page with image area blanked (text appears, image area white)
    // Step 2: Re-render with images and display again (images appear clean)
    int16_t imgX, imgY, imgW, imgH;
    if (page->getImageBoundingBox(imgX, imgY, imgW, imgH)) {
      renderer.fillRect(imgX + orientedMarginLeft, imgY + orientedMarginTop, imgW, imgH, false);
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);

      // Re-render page content to restore images into the blanked area
      page->render(renderer, SETTINGS.getReaderFontId(), orientedMarginLeft, orientedMarginTop);
      renderStatusBar();
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    } else {
      // Fallback when bounding box unavailable: AA needs FAST_REFRESH to avoid ghosting
      if (SETTINGS.textAntiAliasing) {
        renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      } else {
        renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      }
    }
    // Double FAST_REFRESH handles ghosting for image pages; don't count toward full refresh cadence
  } else if (SETTINGS.textAntiAliasing) {
    // Anti-aliasing on: always use FAST_REFRESH for BW pass.
    // HALF_REFRESH sets e-ink particles too firmly, preventing the subsequent
    // grayscale LUT from adjusting them — causing ghost artifacts on next page.
    // Periodic full clear: flash blank screen to reset accumulated ghosting,
    // then render the page with FAST_REFRESH so grayscale LUT works correctly.
    pagesUntilFullRefresh--;
    if (pagesUntilFullRefresh <= 0) {
      pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
      renderer.clearScreen();
      renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      // Re-render page content into framebuffer after the blank flash
      renderer.clearScreen();
      page->render(renderer, SETTINGS.getReaderFontId(), orientedMarginLeft, orientedMarginTop);
      renderStatusBar();
    }
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  } else {
    ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);
  }

  // Save bw buffer to reset buffer state after grayscale data sync
  renderer.storeBwBuffer();

  // grayscale rendering
  // TODO: Only do this if font supports it
  if (SETTINGS.textAntiAliasing) {
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    page->render(renderer, SETTINGS.getReaderFontId(), orientedMarginLeft, orientedMarginTop);
    renderer.copyGrayscaleLsbBuffers();

    // Render and copy to MSB buffer
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    page->render(renderer, SETTINGS.getReaderFontId(), orientedMarginLeft, orientedMarginTop);
    renderer.copyGrayscaleMsbBuffers();

    // display grayscale part
    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  }

  // restore the bw data
  renderer.restoreBwBuffer();
}

void EpubReaderActivity::renderStatusBar() const {
  // Calculate progress in book
  const int currentPage = section->currentPage + 1;
  const float pageCount = section->pageCount;
  const float sectionChapterProg = (pageCount > 0) ? (static_cast<float>(currentPage) / pageCount) : 0;
  const float bookProgress = epub->calculateProgress(currentSpineIndex, sectionChapterProg) * 100;

  std::string title;

  int textYOffset = 0;

  if (automaticPageTurnActive) {
    title = tr(STR_AUTO_TURN_ENABLED) + std::to_string(60 * 1000 / pageTurnDuration);

    // calculates textYOffset when rendering title in status bar
    const uint8_t statusBarHeight = UITheme::getInstance().getStatusBarHeight();

    // offsets text if no status bar or progress bar only
    if (statusBarHeight == 0 || statusBarHeight == UITheme::getInstance().getProgressBarHeight()) {
      textYOffset += UITheme::getInstance().getMetrics().statusBarVerticalMargin;
    }

  } else if (SETTINGS.statusBarTitle == CrossPointSettings::STATUS_BAR_TITLE::CHAPTER_TITLE) {
    title = tr(STR_UNNAMED);
    const int tocIndex = resolveCurrentTocIndex();
    if (tocIndex != -1) {
      const auto tocItem = epub->getTocItem(tocIndex);
      title = tocItem.title;
    }

  } else if (SETTINGS.statusBarTitle == CrossPointSettings::STATUS_BAR_TITLE::BOOK_TITLE) {
    title = epub->getTitle();
  }

  const bool isStarred = section && bookmarkStore.has(static_cast<uint16_t>(currentSpineIndex),
                                                      static_cast<uint16_t>(section->currentPage));
  GUI.drawStatusBar(renderer, bookProgress, currentPage, pageCount, title, 0, textYOffset, isStarred);

  // Draw session timer and/or time estimate inside the status bar area
  if (SETTINGS.statusBarSessionTimer || SETTINGS.statusBarTimeEstimate) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
    renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                     &orientedMarginLeft);
    const int barH = UITheme::getInstance().getStatusBarHeight();
    // Position text inside the status bar: bar starts at (screenH - barH - marginBottom)
    const int textY = renderer.getScreenHeight() - orientedMarginBottom - barH + 2;

    // Session timer — rendered on the right side, left of progress text
    if (SETTINGS.statusBarSessionTimer && sessionStartMs > 0) {
      const uint32_t elapsedSec = (millis() - sessionStartMs) / 1000;
      char sessBuf[12];
      const uint32_t h = elapsedSec / 3600;
      const uint32_t m = (elapsedSec % 3600) / 60;
      if (h > 0) {
        snprintf(sessBuf, sizeof(sessBuf), "%uh%um", (unsigned)h, (unsigned)m);
      } else {
        snprintf(sessBuf, sizeof(sessBuf), "%um", (unsigned)m);
      }
      const int sessW = renderer.getTextWidth(SMALL_FONT_ID, sessBuf);
      renderer.drawText(SMALL_FONT_ID,
                        renderer.getScreenWidth() - metrics.statusBarHorizontalMargin - orientedMarginRight - sessW,
                        textY, sessBuf);
    }

    // Time estimate — rendered on the left side, after clock/battery gap
    if (SETTINGS.statusBarTimeEstimate) {
      const int minsLeft = getEstimatedMinutesLeft();
      if (minsLeft > 0) {
        char estimBuf[16];
        if (minsLeft >= 60) {
          snprintf(estimBuf, sizeof(estimBuf), "~%uh%um", (unsigned)(minsLeft / 60), (unsigned)(minsLeft % 60));
        } else {
          snprintf(estimBuf, sizeof(estimBuf), "~%um", (unsigned)minsLeft);
        }
        // Position after the left content (clock + battery) — use a conservative offset
        const int leftOffset = metrics.statusBarHorizontalMargin + orientedMarginLeft +
                               (SETTINGS.statusBarClock ? 36 : 0) +
                               (SETTINGS.statusBarBattery ? 46 : 0);
        renderer.drawText(SMALL_FONT_ID, leftOffset, textY, estimBuf);
      }
    }
  }
}

int EpubReaderActivity::getEstimatedMinutesLeft() const {
  if (pageTurnTimesCount == 0 || !section) return 0;

  // Average page turn duration from circular buffer
  unsigned long total = 0;
  for (int i = 0; i < pageTurnTimesCount; i++) {
    total += pageTurnTimes[i];
  }
  const unsigned long avgMs = total / pageTurnTimesCount;

  // Remaining pages: rest of current chapter
  const int pagesLeft = section->pageCount - section->currentPage - 1;
  if (pagesLeft <= 0) return 0;

  const unsigned long totalMs = avgMs * static_cast<unsigned long>(pagesLeft);
  return static_cast<int>(totalMs / 60000);
}

void EpubReaderActivity::navigateToHref(const std::string& hrefStr, const bool savePosition) {
  if (!epub) return;

  // Push current position onto saved stack
  if (savePosition && section && footnoteDepth < MAX_FOOTNOTE_DEPTH) {
    savedPositions[footnoteDepth] = {currentSpineIndex, section->currentPage};
    footnoteDepth++;
    LOG_DBG("ERS", "Saved position [%d]: spine %d, page %d", footnoteDepth, currentSpineIndex, section->currentPage);
  }

  // Extract fragment anchor (e.g. "#note1" or "chapter2.xhtml#note1")
  std::string anchor;
  const auto hashPos = hrefStr.find('#');
  if (hashPos != std::string::npos && hashPos + 1 < hrefStr.size()) {
    anchor = hrefStr.substr(hashPos + 1);
  }

  // Check for same-file anchor reference (#anchor only)
  bool sameFile = !hrefStr.empty() && hrefStr[0] == '#';

  int targetSpineIndex;
  if (sameFile) {
    targetSpineIndex = currentSpineIndex;
  } else {
    targetSpineIndex = epub->resolveHrefToSpineIndex(hrefStr);
  }

  if (targetSpineIndex < 0) {
    LOG_DBG("ERS", "Could not resolve href: %s", hrefStr.c_str());
    if (savePosition && footnoteDepth > 0) footnoteDepth--;  // undo push
    return;
  }

  {
    RenderLock lock(*this);
    pendingAnchor = std::move(anchor);
    currentSpineIndex = targetSpineIndex;
    nextPageNumber = 0;
    section.reset();
  }
  requestUpdate();
  LOG_DBG("ERS", "Navigated to spine %d for href: %s", targetSpineIndex, hrefStr.c_str());
}

void EpubReaderActivity::restoreSavedPosition() {
  if (footnoteDepth <= 0) return;
  footnoteDepth--;
  const auto& pos = savedPositions[footnoteDepth];
  LOG_DBG("ERS", "Restoring position [%d]: spine %d, page %d", footnoteDepth, pos.spineIndex, pos.pageNumber);

  {
    RenderLock lock(*this);
    currentSpineIndex = pos.spineIndex;
    nextPageNumber = pos.pageNumber;
    section.reset();
  }
  requestUpdate();
}

int EpubReaderActivity::resolveCurrentTocIndex() const {
  const int baseTocIdx = epub->getTocIndexForSpineIndex(currentSpineIndex);
  if (baseTocIdx == -1 || !section) return baseTocIdx;

  // Scan TOC entries sharing same spine index. Find last one whose anchor page <= currentPage.
  int bestTocIdx = baseTocIdx;
  const int baseSpineIdx = epub->getTocItem(baseTocIdx).spineIndex;
  const int tocCount = epub->getTocItemsCount();

  for (int i = baseTocIdx + 1; i < tocCount; i++) {
    const auto entry = epub->getTocItem(i);
    if (entry.spineIndex != baseSpineIdx) break;

    if (entry.anchor.empty()) continue;

    const auto anchorPage = section->getPageForAnchor(entry.anchor);
    if (anchorPage && *anchorPage <= static_cast<uint16_t>(section->currentPage)) {
      bestTocIdx = i;
    }
  }

  return bestTocIdx;
}

bool EpubReaderActivity::drawCurrentPageToBuffer(const std::string& filePath, GfxRenderer& renderer) {
  auto epub = std::make_shared<Epub>(filePath, "/.crosspoint");
  // Load CSS when embeddedStyle is enabled, as createSectionFile may need it to rebuild the cache.
  if (!epub->load(true, SETTINGS.embeddedStyle == 0)) {
    LOG_DBG("SLP", "EPUB: failed to load %s", filePath.c_str());
    return false;
  }

  epub->setupCacheDir();

  // Load saved spine index and page number
  int spineIndex = 0, pageNumber = 0;
  FsFile f;
  if (Storage.openFileForRead("SLP", epub->getCachePath() + "/progress.bin", f)) {
    uint8_t data[6];
    if (f.read(data, 6) == 6) {
      spineIndex = (int)((uint32_t)data[0] | ((uint32_t)data[1] << 8));
      pageNumber = (int)((uint32_t)data[2] | ((uint32_t)data[3] << 8));
    }
    f.close();
  }
  if (spineIndex < 0 || spineIndex >= epub->getSpineItemsCount()) spineIndex = 0;

  // Apply the reader orientation so margins match what the reader would produce
  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);

  // Compute margins exactly as render() does
  int marginTop, marginRight, marginBottom, marginLeft;
  renderer.getOrientedViewableTRBL(&marginTop, &marginRight, &marginBottom, &marginLeft);
  marginTop += SETTINGS.screenMargin;
  marginLeft += SETTINGS.screenMargin;
  marginRight += SETTINGS.screenMargin;
  const uint8_t statusBarHeight = UITheme::getInstance().getStatusBarHeight();
  marginBottom += std::max(SETTINGS.screenMargin, statusBarHeight);

  const uint16_t viewportWidth = renderer.getScreenWidth() - marginLeft - marginRight;
  const uint16_t viewportHeight = renderer.getScreenHeight() - marginTop - marginBottom;

  // Load or rebuild the section cache. Rebuilding is needed when the cache is missing or stale
  // (e.g. after a firmware update). A no-op popup callback avoids any UI during sleep preparation.
  auto section = std::make_unique<Section>(epub, spineIndex, renderer);
  if (!section->loadSectionFile(SETTINGS.getReaderFontId(), SETTINGS.getReaderLineCompression(),
                                SETTINGS.extraParagraphSpacing, SETTINGS.paragraphAlignment, viewportWidth,
                                viewportHeight, SETTINGS.hyphenationEnabled, SETTINGS.embeddedStyle,
                                SETTINGS.imageRendering)) {
    LOG_DBG("SLP", "EPUB: section cache not found for spine %d, rebuilding", spineIndex);
    if (!section->createSectionFile(SETTINGS.getReaderFontId(), SETTINGS.getReaderLineCompression(),
                                    SETTINGS.extraParagraphSpacing, SETTINGS.paragraphAlignment, viewportWidth,
                                    viewportHeight, SETTINGS.hyphenationEnabled, SETTINGS.embeddedStyle,
                                    SETTINGS.imageRendering, []() {})) {
      LOG_ERR("SLP", "EPUB: failed to rebuild section cache for spine %d", spineIndex);
      return false;
    }
  }

  if (pageNumber < 0 || pageNumber >= section->pageCount) pageNumber = 0;
  section->currentPage = pageNumber;

  auto page = section->loadPageFromSectionFile();
  if (!page) {
    LOG_DBG("SLP", "EPUB: failed to load page %d", pageNumber);
    return false;
  }

  renderer.clearScreen();
  page->render(renderer, SETTINGS.getReaderFontId(), marginLeft, marginTop);
  // No displayBuffer call — caller (SleepActivity) handles that after compositing the overlay
  return true;
}
