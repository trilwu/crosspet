#pragma once
#include <string>
#include <vector>

#include "../Activity.h"

struct Quote {
  std::string body;
  std::string attribution;
};

class DailyQuoteActivity final : public Activity {
  std::vector<Quote> quotes;
  int currentIndex = 0;
  bool fileLoaded = false;

  void parseQuotes(const char* data, size_t len);

 public:
  explicit DailyQuoteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DailyQuote", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  // No preventAutoSleep — static display, device should sleep normally
};
