#include "DailyQuoteActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <esp_random.h>

#include <cstring>
#include <memory>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* QUOTES_FILE = "/quotes.txt";
constexpr size_t MAX_FILE_SIZE = 4096;
constexpr int MAX_QUOTES = 50;
}  // namespace

void DailyQuoteActivity::parseQuotes(const char* data, size_t len) {
  quotes.clear();
  quotes.reserve(MAX_QUOTES);
  Quote current;
  const char* lineStart = data;
  const char* end = data + len;

  while (lineStart < end && quotes.size() < MAX_QUOTES) {
    // Find end of line
    const char* lineEnd = lineStart;
    while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') lineEnd++;

    std::string line(lineStart, lineEnd - lineStart);

    // Skip past \r\n
    lineStart = lineEnd;
    if (lineStart < end && *lineStart == '\r') lineStart++;
    if (lineStart < end && *lineStart == '\n') lineStart++;

    // Check for separator
    if (line == "---") {
      if (!current.body.empty()) {
        quotes.push_back(std::move(current));
        current = Quote();
      }
      continue;
    }

    // Check for attribution (starts with -- or em dash)
    if ((line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] != '-') ||
        (line.size() >= 3 && (uint8_t)line[0] == 0xE2 && (uint8_t)line[1] == 0x80 && (uint8_t)line[2] == 0x94)) {
      // Strip "-- " or "— " prefix
      size_t start = 2;
      if ((uint8_t)line[0] == 0xE2) start = 3;
      while (start < line.size() && line[start] == ' ') start++;
      current.attribution = line.substr(start);
      continue;
    }

    // Regular line — append to body
    if (!current.body.empty()) current.body += '\n';
    current.body += line;
  }

  // Don't forget the last entry
  if (!current.body.empty() && quotes.size() < MAX_QUOTES) {
    quotes.push_back(std::move(current));
  }
}

void DailyQuoteActivity::onEnter() {
  Activity::onEnter();

  if (Storage.exists(QUOTES_FILE)) {
    auto buffer = std::make_unique<char[]>(MAX_FILE_SIZE);
    size_t bytesRead = Storage.readFileToBuffer(QUOTES_FILE, buffer.get(), MAX_FILE_SIZE);
    if (bytesRead > 0) {
      parseQuotes(buffer.get(), bytesRead);
      fileLoaded = true;
    }
  }

  if (!quotes.empty()) {
    currentIndex = esp_random() % quotes.size();
  }

  requestUpdate();
}

void DailyQuoteActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!quotes.empty()) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      currentIndex = (currentIndex + 1) % quotes.size();
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      currentIndex = (currentIndex + quotes.size() - 1) % quotes.size();
      requestUpdate();
    }
  }
}

void DailyQuoteActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DAILY_QUOTE));

  if (quotes.empty()) {
    const int y = (pageHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_PLACE_QUOTES_TXT));
  } else {
    const Quote& q = quotes[currentIndex];
    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
    const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
    const int maxWidth = pageWidth - metrics.contentSidePadding * 2;
    const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int reserveBottom = q.attribution.empty() ? 0 : lineHeight + 10;

    // Word-wrap: render line by line directly to minimize stack/heap usage
    int y = contentTop;
    const char* ptr = q.body.c_str();

    while (*ptr && y + lineHeight < contentBottom - reserveBottom) {
      // Build one line of text
      char line[128] = {0};
      int lineLen = 0;

      while (*ptr) {
        const char* nextSpace = ptr;
        while (*nextSpace && *nextSpace != ' ' && *nextSpace != '\n') nextSpace++;

        // Test if adding this word would exceed width
        char candidate[128] = {0};
        if (lineLen > 0) {
          memcpy(candidate, line, lineLen);
          candidate[lineLen] = ' ';
          memcpy(candidate + lineLen + 1, ptr, nextSpace - ptr);
          candidate[lineLen + 1 + (nextSpace - ptr)] = '\0';
        } else {
          int wordLen = nextSpace - ptr;
          if (wordLen >= (int)sizeof(candidate)) wordLen = sizeof(candidate) - 1;
          memcpy(candidate, ptr, wordLen);
          candidate[wordLen] = '\0';
        }

        if (lineLen > 0 && renderer.getTextWidth(UI_10_FONT_ID, candidate) > maxWidth) {
          break;
        }

        int candidateLen = strlen(candidate);
        if (candidateLen >= (int)sizeof(line)) candidateLen = sizeof(line) - 1;
        memcpy(line, candidate, candidateLen);
        line[candidateLen] = '\0';
        lineLen = candidateLen;

        ptr = nextSpace;
        if (*ptr == '\n') {
          ptr++;
          break;
        }
        if (*ptr == ' ') ptr++;
      }

      if (lineLen == 0) break;
      renderer.drawCenteredText(UI_10_FONT_ID, y, line);
      y += lineHeight;
    }

    // Attribution
    if (!q.attribution.empty()) {
      y += 10;
      char attr[140];
      snprintf(attr, sizeof(attr), "- %s", q.attribution.c_str());
      renderer.drawCenteredText(SMALL_FONT_ID, y, attr);
    }
  }

  const char* btn4 = quotes.size() > 1 ? tr(STR_DIR_RIGHT) : "";
  const char* btn3 = quotes.size() > 1 ? tr(STR_DIR_LEFT) : "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", btn3, btn4);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
