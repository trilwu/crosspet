#pragma once

#include <vector>

#include "../Activity.h"
#include "BookmarkStore.h"
#include "util/ButtonNavigator.h"

// Shows a list of bookmarks for the current book.
// Returns ChapterResult with spineIndex + pageIndex on selection.
class BookmarkListActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  const std::vector<Bookmark>& bookmarks;

 public:
  BookmarkListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::vector<Bookmark>& bookmarks)
      : Activity("BookmarkList", renderer, mappedInput), bookmarks(bookmarks) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
