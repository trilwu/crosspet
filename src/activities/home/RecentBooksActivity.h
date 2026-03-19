#pragma once
#include <I18n.h>

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "RecentBooksStore.h"
#include "util/ButtonNavigator.h"

// Shows recent books as a cover grid (3 columns) with title below each cover.
// Navigate with D-pad, Confirm opens book, Back goes home.
class RecentBooksActivity final : public Activity {
 private:
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  std::vector<RecentBook> recentBooks;

  static constexpr int COLS = 3;
  static constexpr int COVER_GAP = 12;
  static constexpr int COVER_R = 8;

  // Pagination
  int pageOffset = 0;    // first book index on current page
  int itemsPerPage = 6;  // calculated from screen height

  void loadRecentBooks();
  int totalPages() const;
  void ensurePageForIndex();

  // Render a single cover card at grid position
  void renderCover(int bookIdx, int gridCol, int gridRow, int cardW, int cardH, int startX, int startY, bool selected);

 public:
  explicit RecentBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RecentBooks", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
