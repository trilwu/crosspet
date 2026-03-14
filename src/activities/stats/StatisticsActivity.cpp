#include "StatisticsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "TabNavigation.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int SCROLL_STEP = 40;
constexpr int ROW_HEIGHT = 28;
constexpr int SECTION_GAP = 16;
}  // namespace

void StatisticsActivity::onEnter() {
  Activity::onEnter();

  bookStats.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  bookStats.reserve(books.size());
  for (const auto& b : books) {
    if (!b.title.empty()) {
      bookStats.push_back({b.title, b.author});
    }
  }

  scrollOffset = 0;
  requestUpdate();
}

void StatisticsActivity::onExit() {
  Activity::onExit();
  bookStats.clear();
}

void StatisticsActivity::loop() {
  // Tab switching via L/R
  int nextTab = handleTabInput(mappedInput, static_cast<Tab>(APP_STATE.currentTab));
  if (nextTab >= 0) {
    APP_STATE.currentTab = static_cast<uint8_t>(nextTab);
    APP_STATE.saveToFile();
    activityManager.replaceActivity(createTabActivity(static_cast<Tab>(nextTab), renderer, mappedInput));
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // Go to Home tab on Back
    APP_STATE.currentTab = TAB_HOME;
    APP_STATE.saveToFile();
    activityManager.replaceActivity(createTabActivity(TAB_HOME, renderer, mappedInput));
    return;
  }

  // Scroll down
  buttonNavigator.onNextRelease([this] {
    int maxScroll = totalContentHeight - viewportHeight;
    if (maxScroll < 0) maxScroll = 0;
    if (scrollOffset < maxScroll) {
      scrollOffset += SCROLL_STEP;
      if (scrollOffset > maxScroll) scrollOffset = maxScroll;
      requestUpdate();
    }
  });

  // Scroll up
  buttonNavigator.onPreviousRelease([this] {
    if (scrollOffset > 0) {
      scrollOffset -= SCROLL_STEP;
      if (scrollOffset < 0) scrollOffset = 0;
      requestUpdate();
    }
  });
}

void StatisticsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  // Header
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_STATISTICS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  viewportHeight = contentBottom - contentTop;
  const int sidePad = metrics.contentSidePadding;

  int y = contentTop - scrollOffset;

  // --- Summary Section ---
  if (y + ROW_HEIGHT > contentTop && y < contentBottom) {
    renderer.drawText(UI_12_FONT_ID, sidePad, y, tr(STR_STATISTICS), true, EpdFontFamily::BOLD);
  }
  y += ROW_HEIGHT + 4;

  // Book count
  char buf[64];
  snprintf(buf, sizeof(buf), "%s: %d", tr(STR_BOOKS_READ), static_cast<int>(bookStats.size()));
  if (y + ROW_HEIGHT > contentTop && y < contentBottom) {
    renderer.drawText(UI_10_FONT_ID, sidePad + 8, y, buf, true);
  }
  y += ROW_HEIGHT;
  y += SECTION_GAP;

  // --- Recent Books Section ---
  if (!bookStats.empty()) {
    if (y + ROW_HEIGHT > contentTop && y < contentBottom) {
      renderer.drawText(UI_12_FONT_ID, sidePad, y, tr(STR_RECENT_BOOKS), true, EpdFontFamily::BOLD);
    }
    y += ROW_HEIGHT + 4;

    // Separator line
    if (y > contentTop && y < contentBottom) {
      renderer.drawLine(sidePad, y, pageWidth - sidePad, y, true);
    }
    y += 4;

    for (size_t i = 0; i < bookStats.size(); i++) {
      if (y + ROW_HEIGHT > contentTop && y < contentBottom) {
        // Title (truncated to fit)
        std::string label = std::to_string(i + 1) + ". " + bookStats[i].title;
        std::string truncated = renderer.truncatedText(UI_10_FONT_ID, label.c_str(), pageWidth - sidePad * 2 - 8);
        renderer.drawText(UI_10_FONT_ID, sidePad + 8, y, truncated.c_str(), true);
      }
      y += ROW_HEIGHT;

      // Author (smaller, indented)
      if (!bookStats[i].author.empty() && y + ROW_HEIGHT > contentTop && y < contentBottom) {
        std::string authorLine = "  " + bookStats[i].author;
        std::string truncAuth = renderer.truncatedText(SMALL_FONT_ID, authorLine.c_str(), pageWidth - sidePad * 2 - 16);
        renderer.drawText(SMALL_FONT_ID, sidePad + 16, y, truncAuth.c_str(), true);
      }
      y += ROW_HEIGHT - 6;
    }
  } else {
    if (y + ROW_HEIGHT > contentTop && y < contentBottom) {
      renderer.drawText(UI_10_FONT_ID, sidePad, y, tr(STR_NO_BOOKS_FOUND), true);
    }
    y += ROW_HEIGHT;
  }

  totalContentHeight = (y + scrollOffset) - contentTop;

  // Scroll indicator
  if (totalContentHeight > viewportHeight) {
    int barX = pageWidth - 4;
    int barH = viewportHeight * viewportHeight / totalContentHeight;
    if (barH < 10) barH = 10;
    int barY = contentTop + (scrollOffset * (viewportHeight - barH)) / (totalContentHeight - viewportHeight);
    renderer.fillRect(barX, barY, 3, barH, true);
  }

  // Tab bar at bottom
  auto tabs = getGlobalTabs(static_cast<Tab>(APP_STATE.currentTab));
  int tabBarY = pageHeight - metrics.tabBarHeight;
  GUI.drawTabBar(renderer, Rect{0, tabBarY, pageWidth, metrics.tabBarHeight}, tabs, true);

  renderer.displayBuffer();
}
