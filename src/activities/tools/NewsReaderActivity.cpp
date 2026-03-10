#include "NewsReaderActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include "NewsArticleActivity.h"
#include "WifiCredentialStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

static constexpr const char* FEED_URL = "https://defuddle.md/vnexpress.net";
// Visible rows in the article list
static constexpr int LIST_VISIBLE_ROWS = 10;

// Try silent WiFi connect using saved credentials (no UI)
static bool newsSilentWifiConnect() {
  const auto& ssid = WIFI_STORE.getLastConnectedSsid();
  if (ssid.empty()) return false;
  const auto* cred = WIFI_STORE.findCredential(ssid);
  if (!cred) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
  for (int i = 0; i < 100; i++) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(100);
  }
  WiFi.disconnect(false);
  return false;
}

void NewsReaderActivity::onEnter() {
  Activity::onEnter();
  articleCount = 0;
  selectorIndex = 0;
  scrollTop = 0;
  state = WIFI_CONNECTING;
  statusMessage = tr(STR_FETCHING_NEWS);
  requestUpdate(true);

  if (WiFi.status() == WL_CONNECTED) {
    onWifiConnected();
  } else if (newsSilentWifiConnect()) {
    onWifiConnected();
  } else {
    startActivityForResult(
        std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
        [this](const ActivityResult& r) {
          if (r.isCancelled) {
            finish();
            return;
          }
          onWifiConnected();
        });
  }
}

void NewsReaderActivity::onWifiConnected() {
  state = FETCHING;
  statusMessage = tr(STR_FETCHING_NEWS);
  requestUpdate(true);
  fetchFeed();
}

void NewsReaderActivity::fetchFeed() {
  std::string response;
  if (!HttpDownloader::fetchUrl(FEED_URL, response)) {
    state = FETCH_ERROR;
    statusMessage = tr(STR_NEWS_ERROR);
    requestUpdate(true);
    return;
  }

  // Limit response size to avoid memory exhaustion (~15KB)
  if (response.size() > 15000) {
    response.resize(15000);
  }

  parseFeed(response);

  if (articleCount == 0) {
    state = FETCH_ERROR;
    statusMessage = tr(STR_NO_ARTICLES);
    requestUpdate(true);
    return;
  }

  state = DISPLAYING;
  requestUpdate(true);
}

// Parse defuddle.md markdown output.
// Format: ### Title\n[![Title](img)](https://vnexpress.net/path.html "Title")\n[Summary](url)
// We extract the heading as title and the article URL from the image-link line below.
void NewsReaderActivity::parseFeed(const std::string& markdown) {
  articleCount = 0;
  const char* p = markdown.c_str();

  while (*p && articleCount < NEWS_MAX_ARTICLES) {
    // Find line starting with "### " (plain heading, not "### [")
    if (strncmp(p, "### ", 4) != 0) {
      while (*p && *p != '\n') p++;
      if (*p == '\n') p++;
      continue;
    }
    p += 4;  // skip "### "

    // Extract title to end of line
    const char* titleStart = p;
    while (*p && *p != '\n') p++;
    int titleLen = (int)(p - titleStart);
    if (*p == '\n') p++;
    if (titleLen <= 0) continue;

    // Look for article URL in next few lines: pattern ](https://... or ](http://...
    // Scan up to 5 lines for the URL
    const char* articleUrl = nullptr;
    int articleUrlLen = 0;
    for (int line = 0; line < 5 && *p; line++) {
      // Search for "](https://" or "](http://" in this line
      const char* lineStart = p;
      while (*p && *p != '\n') {
        if (*p == ']' && *(p + 1) == '(') {
          const char* urlStart = p + 2;
          // Find closing ')' or ' ' (URL may have "title" after space)
          const char* urlEnd = urlStart;
          while (*urlEnd && *urlEnd != ')' && *urlEnd != ' ' && *urlEnd != '"' && *urlEnd != '\n') urlEnd++;
          int uLen = (int)(urlEnd - urlStart);
          if (uLen > 10 && (strncmp(urlStart, "https://", 8) == 0 || strncmp(urlStart, "http://", 7) == 0)) {
            articleUrl = urlStart;
            articleUrlLen = uLen;
            break;
          }
        }
        p++;
      }
      if (articleUrl) break;
      if (*p == '\n') p++;
    }

    if (!articleUrl || articleUrlLen <= 0) continue;

    // Store full URL (not just path) for fetching via defuddle.md
    auto& a = articles[articleCount];
    int tLen = titleLen < (NEWS_TITLE_MAX - 1) ? titleLen : (NEWS_TITLE_MAX - 1);
    memcpy(a.title, titleStart, tLen);
    a.title[tLen] = '\0';

    int uLen = articleUrlLen < (NEWS_URL_MAX - 1) ? articleUrlLen : (NEWS_URL_MAX - 1);
    memcpy(a.urlPath, articleUrl, uLen);
    a.urlPath[uLen] = '\0';

    if (a.urlPath[0] == '\0' || a.title[0] == '\0') continue;

    articleCount++;
  }

  LOG_DBG("NEWS", "Parsed %d articles", articleCount);
}

void NewsReaderActivity::ensureVisible(int visibleRows) {
  if (selectorIndex < scrollTop) scrollTop = selectorIndex;
  if (selectorIndex >= scrollTop + visibleRows) scrollTop = selectorIndex - visibleRows + 1;
}

void NewsReaderActivity::onExit() {
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  Activity::onExit();
}

void NewsReaderActivity::loop() {
  auto back = mappedInput.wasReleased(MappedInputManager::Button::Back);
  auto confirm = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  auto down = mappedInput.wasReleased(MappedInputManager::Button::Down);
  auto up = mappedInput.wasReleased(MappedInputManager::Button::Up);
  auto right = mappedInput.wasReleased(MappedInputManager::Button::Right);
  auto left = mappedInput.wasReleased(MappedInputManager::Button::Left);

  if (back) {
    finish();
    return;
  }

  if (state != DISPLAYING) return;

  if (down || right) {
    selectorIndex = (selectorIndex + 1) % articleCount;
    ensureVisible(LIST_VISIBLE_ROWS);
    requestUpdate();
  }
  if (up || left) {
    selectorIndex = (selectorIndex + articleCount - 1) % articleCount;
    ensureVisible(LIST_VISIBLE_ROWS);
    requestUpdate();
  }
  if (confirm && articleCount > 0) {
    // Build defuddle URL from full article URL stored in urlPath
    // urlPath contains e.g. "https://vnexpress.net/article.html"
    // We need: "https://defuddle.md/vnexpress.net/article.html"
    const char* raw = articles[selectorIndex].urlPath;
    std::string articleUrl = "https://defuddle.md/";
    if (strncmp(raw, "https://", 8) == 0) {
      articleUrl += (raw + 8);
    } else if (strncmp(raw, "http://", 7) == 0) {
      articleUrl += (raw + 7);
    } else {
      articleUrl += raw;
    }
    activityManager.pushActivity(
        std::make_unique<NewsArticleActivity>(renderer, mappedInput, articleUrl));
  }
}

void NewsReaderActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_NEWS));

  const int listTop = metrics.topPadding + metrics.headerHeight + 4;

  if (state == FETCHING || state == WIFI_CONNECTING || state == FETCH_ERROR) {
    int y = listTop + 20;
    renderer.drawCenteredText(UI_10_FONT_ID, y, statusMessage.c_str());
  } else {
    // Scrollable article list
    const int lineH = renderer.getLineHeight(UI_10_FONT_ID) + 4;
    const int listHeight = pageHeight - listTop - metrics.buttonHintsHeight - 4;
    const int visRows = listHeight / lineH;

    for (int i = 0; i < visRows && (scrollTop + i) < articleCount; i++) {
      int idx = scrollTop + i;
      int y = listTop + i * lineH;

      if (idx == selectorIndex) {
        renderer.fillRect(2, y, pageWidth - 4, lineH, true);
        renderer.drawText(UI_10_FONT_ID, 8, y + 1, articles[idx].title, false);
      } else {
        renderer.drawText(UI_10_FONT_ID, 8, y + 1, articles[idx].title, true);
      }
    }

    // Scroll indicators
    if (scrollTop > 0) {
      renderer.drawCenteredText(SMALL_FONT_ID, listTop - 12, "▲");
    }
    if (scrollTop + visRows < articleCount) {
      int y = listTop + visRows * lineH + 2;
      renderer.drawCenteredText(SMALL_FONT_ID, y, "▼");
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
