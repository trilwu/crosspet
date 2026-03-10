#include "NewsArticleActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

void NewsArticleActivity::onEnter() {
  Activity::onEnter();
  state = FETCHING;
  statusMessage = tr(STR_FETCHING_NEWS);
  currentPage = 0;
  lines.clear();
  requestUpdate(true);
  fetchArticle();
}

void NewsArticleActivity::fetchArticle() {
  std::string response;
  if (!HttpDownloader::fetchUrl(articleUrl, response)) {
    state = FETCH_ERROR;
    statusMessage = tr(STR_NEWS_ERROR);
    requestUpdate(true);
    return;
  }

  // Truncate to budget
  if (response.size() > ARTICLE_TEXT_MAX) {
    response.resize(ARTICLE_TEXT_MAX);
  }

  processMarkdown(response);

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID) + 3;
  const int contentTop = metrics.topPadding + metrics.headerHeight + 4;
  const int availH = pageHeight - contentTop - metrics.buttonHintsHeight - 4;
  linesPerPage = std::max(1, availH / lineH);

  totalPages = ((int)lines.size() + linesPerPage - 1) / linesPerPage;
  if (totalPages < 1) totalPages = 1;

  state = DISPLAYING;
  requestUpdate(true);
}

// Strip inline markdown: **bold**, *italic*, [text](url), # headers, --- separators
std::string NewsArticleActivity::stripInline(const std::string& src) {
  std::string out;
  out.reserve(src.size());
  const char* p = src.c_str();

  // Skip leading '#' header markers and whitespace
  while (*p == '#') p++;
  while (*p == ' ') p++;

  while (*p) {
    if (*p == '*' && *(p + 1) == '*') {
      // Bold: **text** — skip markers
      p += 2;
      while (*p && !(*p == '*' && *(p + 1) == '*')) {
        out += *p++;
      }
      if (*p == '*') p += 2;
    } else if (*p == '*') {
      // Italic: *text*
      p++;
      while (*p && *p != '*') out += *p++;
      if (*p == '*') p++;
    } else if (*p == '!' && *(p + 1) == '[') {
      // Image: ![alt](url) — skip entirely
      p += 2;
      while (*p && *p != ']') p++;
      if (*p == ']') p++;
      if (*p == '(') {
        p++;
        while (*p && *p != ')') p++;
        if (*p == ')') p++;
      }
    } else if (*p == '[') {
      // Link: [text](url) — keep text, discard url
      p++;
      while (*p && *p != ']') out += *p++;
      if (*p == ']') p++;
      if (*p == '(') {
        p++;
        while (*p && *p != ')') p++;
        if (*p == ')') p++;
      }
    } else if (*p == '`') {
      // Inline code: skip backtick
      p++;
    } else {
      out += *p++;
    }
  }
  return out;
}

// Word-wrap a single paragraph into screen-width lines and append to `lines`
void NewsArticleActivity::wrapParagraph(const std::string& para, int maxWidth) {
  if (para.empty()) {
    lines.emplace_back("");
    return;
  }

  std::string currentLine;
  const char* p = para.c_str();

  while (*p) {
    // Find next token
    const char* tokenStart = p;
    while (*p && *p != ' ') p++;
    std::string token(tokenStart, p);
    while (*p == ' ') p++;

    if (currentLine.empty()) {
      currentLine = token;
    } else {
      std::string candidate = currentLine + " " + token;
      int candidateW = renderer.getTextWidth(UI_10_FONT_ID, candidate.c_str());
      if (candidateW <= maxWidth) {
        currentLine = candidate;
      } else {
        lines.push_back(currentLine);
        currentLine = token;
      }
    }
  }
  if (!currentLine.empty()) lines.push_back(currentLine);
}

void NewsArticleActivity::processMarkdown(const std::string& raw) {
  lines.clear();

  const int pageWidth = renderer.getScreenWidth();
  const int contentPad = 12;
  const int maxWidth = pageWidth - contentPad * 2;

  // Skip YAML frontmatter (--- ... ---)
  const char* p = raw.c_str();
  if (strncmp(p, "---", 3) == 0) {
    p += 3;
    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;
    // Find closing ---
    const char* closingFm = strstr(p, "\n---");
    if (closingFm) {
      p = closingFm + 4;
      while (*p && *p != '\n') p++;
      if (*p == '\n') p++;
    }
  }

  // Process line by line
  while (*p) {
    // Extract one line
    const char* lineStart = p;
    while (*p && *p != '\n') p++;
    std::string rawLine(lineStart, p);
    if (*p == '\n') p++;

    // Skip horizontal rules
    if (rawLine.size() >= 3 && rawLine.find_first_not_of("-* ") == std::string::npos) {
      continue;
    }

    // Strip inline markdown
    std::string clean = stripInline(rawLine);

    // Trim trailing whitespace
    while (!clean.empty() && (clean.back() == ' ' || clean.back() == '\r')) {
      clean.pop_back();
    }

    // Empty line = paragraph break (add blank separator)
    if (clean.empty()) {
      if (!lines.empty() && !lines.back().empty()) {
        lines.emplace_back("");
      }
      continue;
    }

    wrapParagraph(clean, maxWidth);
  }

  LOG_DBG("NEWS", "Article: %d lines", (int)lines.size());
}

void NewsArticleActivity::loop() {
  auto back = mappedInput.wasReleased(MappedInputManager::Button::Back);
  auto right = mappedInput.wasReleased(MappedInputManager::Button::Right);
  auto left = mappedInput.wasReleased(MappedInputManager::Button::Left);
  auto confirm = mappedInput.wasReleased(MappedInputManager::Button::Confirm);

  if (back) {
    finish();
    return;
  }

  if (state != DISPLAYING) return;

  if ((right || confirm) && currentPage < totalPages - 1) {
    currentPage++;
    requestUpdate();
  }
  if (left && currentPage > 0) {
    currentPage--;
    requestUpdate();
  }
}

void NewsArticleActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_NEWS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + 4;
  const int contentPad = 12;

  if (state != DISPLAYING) {
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop + 20, statusMessage.c_str());
  } else {
    const int lineH = renderer.getLineHeight(UI_10_FONT_ID) + 3;
    const int firstLine = currentPage * linesPerPage;

    for (int i = 0; i < linesPerPage; i++) {
      int idx = firstLine + i;
      if (idx >= (int)lines.size()) break;
      int y = contentTop + i * lineH;
      renderer.drawText(UI_10_FONT_ID, contentPad, y, lines[idx].c_str(), true);
    }

    // Page indicator bottom-right above button hints
    char pageBuf[16];
    snprintf(pageBuf, sizeof(pageBuf), "%d/%d", currentPage + 1, totalPages);
    int tw = renderer.getTextWidth(SMALL_FONT_ID, pageBuf);
    int pageIndY = metrics.topPadding + renderer.getScreenHeight() - metrics.buttonHintsHeight - 14;
    renderer.drawText(SMALL_FONT_ID, pageWidth - tw - 8, pageIndY, pageBuf, true);
  }

  // Button hints: Back | Next page | Prev page
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
