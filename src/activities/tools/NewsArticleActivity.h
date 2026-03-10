#pragma once
#include "../Activity.h"

#include <string>
#include <vector>

// Maximum article text to hold in RAM (~10KB)
static constexpr size_t ARTICLE_TEXT_MAX = 10240;
// Approximate lines per page at UI_10 font on 480px-tall screen
static constexpr int ARTICLE_LINES_PER_PAGE = 12;

/**
 * NewsArticleActivity — fetches a single article via defuddle.md and
 * displays paginated plain text. Left/Right navigate pages, Back returns.
 */
class NewsArticleActivity final : public Activity {
 public:
  enum State { FETCHING, DISPLAYING, FETCH_ERROR };

  explicit NewsArticleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                               std::string articleUrl)
      : Activity("NewsArticle", renderer, mappedInput), articleUrl(std::move(articleUrl)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  std::string articleUrl;
  State state = FETCHING;
  std::string statusMessage;

  // Rendered text lines (after markdown stripping + word-wrap)
  std::vector<std::string> lines;
  int currentPage = 0;
  int linesPerPage = ARTICLE_LINES_PER_PAGE;
  int totalPages = 1;

  void fetchArticle();
  // Strip markdown syntax from raw text and populate `lines` with word-wrapped output
  void processMarkdown(const std::string& raw);
  // Word-wrap a single paragraph into lines fitting pageWidth
  void wrapParagraph(const std::string& para, int maxWidth);
  // Strip markdown inline syntax: headers, bold, links → plain text
  static std::string stripInline(const std::string& line);
};
