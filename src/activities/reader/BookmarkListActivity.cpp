#include "BookmarkListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BookmarkListActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void BookmarkListActivity::onExit() { Activity::onExit(); }

void BookmarkListActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && !bookmarks.empty()) {
    const auto& bm = bookmarks[selectedIndex];
    setResult(BookmarkResult{static_cast<int>(bm.spineIndex), static_cast<int>(bm.pageIndex)});
    finish();
    return;
  }

  int listSize = static_cast<int>(bookmarks.size());
  buttonNavigator.onNextRelease([this, listSize] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, listSize);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, listSize] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, listSize);
    requestUpdate();
  });
}

void BookmarkListActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BOOKMARKS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (bookmarks.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_BOOKMARKS));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, bookmarks.size(), selectedIndex,
        [this](int index) {
          const auto& bm = bookmarks[index];
          char label[80];
          // Show "Ch X, P Y — snippet..."
          if (bm.snippet[0] != '\0') {
            snprintf(label, sizeof(label), "Ch %u, P %u - %.50s", bm.spineIndex + 1, bm.pageIndex + 1, bm.snippet);
          } else {
            snprintf(label, sizeof(label), "Ch %u, Page %u", bm.spineIndex + 1, bm.pageIndex + 1);
          }
          return std::string(label);
        },
        nullptr, nullptr);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), bookmarks.empty() ? "" : tr(STR_SELECT), tr(STR_DIR_UP),
                                             tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
