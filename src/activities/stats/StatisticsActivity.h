#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Displays reading statistics: recent books list with basic info.
// Will be extended when ReadingStats singleton is available.
class StatisticsActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int scrollOffset = 0;
  int totalContentHeight = 0;
  int viewportHeight = 0;

  struct BookStat {
    std::string title;
    std::string author;
  };
  std::vector<BookStat> bookStats;

 public:
  explicit StatisticsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Statistics", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
