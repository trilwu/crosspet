#pragma once
#include "../Activity.h"

#include <string>

// Maximum number of articles to hold in memory (RAM budget ~380KB)
static constexpr int NEWS_MAX_ARTICLES = 20;
// Maximum title length per article
static constexpr int NEWS_TITLE_MAX = 96;
// Maximum URL path length per article (relative path only)
static constexpr int NEWS_URL_MAX = 128;

struct NewsArticle {
  char title[NEWS_TITLE_MAX];
  char urlPath[NEWS_URL_MAX];  // relative path, e.g. "article-slug-12345.html"
};

/**
 * NewsReaderActivity — fetches VnExpress headlines via defuddle.md and shows
 * a scrollable list. Selecting an article launches NewsArticleActivity.
 */
class NewsReaderActivity final : public Activity {
 public:
  enum State { WIFI_CONNECTING, FETCHING, DISPLAYING, FETCH_ERROR };

  explicit NewsReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("News", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  State state = WIFI_CONNECTING;
  NewsArticle articles[NEWS_MAX_ARTICLES];
  int articleCount = 0;
  int selectorIndex = 0;
  int scrollTop = 0;
  std::string statusMessage;

  void onWifiConnected();
  void fetchFeed();
  void parseFeed(const std::string& markdown);
  // Adjust scrollTop so selectorIndex is visible
  void ensureVisible(int visibleRows);
};
