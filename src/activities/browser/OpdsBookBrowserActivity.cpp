#include "OpdsBookBrowserActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <OpdsStream.h>
#include <WiFi.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "util/StringUtils.h"
#include "util/UrlUtils.h"

namespace {
constexpr int PAGE_ITEMS = 23;
}  // namespace

void OpdsBookBrowserActivity::onEnter() {
  Activity::onEnter();

  state = BrowserState::CHECK_WIFI;
  entries.clear();
  navigationHistory.clear();
  currentPath = "";  // Root path - user provides full URL in settings
  selectorIndex = 0;
  errorMessage.clear();
  statusMessage = tr(STR_CHECKING_WIFI);
  searchUrl_.clear();
  nextPageUrl_.clear();
  prevPageUrl_.clear();
  requestUpdate();

  // Check WiFi and connect if needed, then fetch feed
  checkAndConnectWifi();
}

void OpdsBookBrowserActivity::onExit() {
  Activity::onExit();

  // Turn off WiFi when exiting
  WiFi.mode(WIFI_OFF);

  entries.clear();
  navigationHistory.clear();
  searchUrl_.clear();
  nextPageUrl_.clear();
  prevPageUrl_.clear();
}

void OpdsBookBrowserActivity::loop() {
  // Handle WiFi selection subactivity
  if (state == BrowserState::WIFI_SELECTION) {
    // Should already handled by the WifiSelectionActivity
    return;
  }

  // Handle error state - Confirm retries, Back goes back or home
  if (state == BrowserState::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      // Check if WiFi is still connected
      if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        // WiFi connected - just retry fetching the feed
        LOG_DBG("OPDS", "Retry: WiFi connected, retrying fetch");
        state = BrowserState::LOADING;
        statusMessage = tr(STR_LOADING);
        requestUpdate();
        fetchFeed(currentPath);
      } else {
        // WiFi not connected - launch WiFi selection
        LOG_DBG("OPDS", "Retry: WiFi not connected, launching selection");
        launchWifiSelection();
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      navigateBack();
    }
    return;
  }

  // Handle WiFi check state - only Back works
  if (state == BrowserState::CHECK_WIFI) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }
    return;
  }

  // Handle loading state - only Back works
  if (state == BrowserState::LOADING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      navigateBack();
    }
    return;
  }

  // Handle downloading state - no input allowed
  if (state == BrowserState::DOWNLOADING) {
    return;
  }

  // Handle browsing state
  if (state == BrowserState::BROWSING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (!entries.empty()) {
        const auto& entry = entries[selectorIndex];
        if (entry.id == "__opds_search__") {
          launchSearch();
        } else if (entry.type == OpdsEntryType::BOOK) {
          downloadBook(entry);
        } else {
          navigateToEntry(entry);
        }
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      navigateBack();
    }

    // Handle navigation
    if (!entries.empty()) {
      buttonNavigator.onNextRelease([this] {
        selectorIndex = ButtonNavigator::nextIndex(selectorIndex, entries.size());
        requestUpdate();
      });

      buttonNavigator.onPreviousRelease([this] {
        selectorIndex = ButtonNavigator::previousIndex(selectorIndex, entries.size());
        requestUpdate();
      });

      buttonNavigator.onNextContinuous([this] {
        selectorIndex = ButtonNavigator::nextPageIndex(selectorIndex, entries.size(), PAGE_ITEMS);
        requestUpdate();
      });

      buttonNavigator.onPreviousContinuous([this] {
        selectorIndex = ButtonNavigator::previousPageIndex(selectorIndex, entries.size(), PAGE_ITEMS);
        requestUpdate();
      });
    }
  }
}

void OpdsBookBrowserActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.drawCenteredText(UI_12_FONT_ID, 15, tr(STR_OPDS_BROWSER), true, EpdFontFamily::BOLD);

  if (state == BrowserState::CHECK_WIFI) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == BrowserState::LOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == BrowserState::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_ERROR_MSG));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, errorMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == BrowserState::DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, tr(STR_DOWNLOADING));
    const auto maxWidth = pageWidth - 40;
    // Trim long titles to keep them within the screen bounds.
    auto title = renderer.truncatedText(UI_10_FONT_ID, statusMessage.c_str(), maxWidth);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, title.c_str());
    if (downloadTotal > 0) {
      const int barWidth = pageWidth - 100;
      constexpr int barHeight = 20;
      constexpr int barX = 50;
      const int barY = pageHeight / 2 + 20;
      GUI.drawProgressBar(renderer, Rect{barX, barY, barWidth, barHeight}, downloadProgress, downloadTotal);
    }
    renderer.displayBuffer();
    return;
  }

  // Browsing state
  // Show appropriate button hint based on selected entry type
  const char* confirmLabel = tr(STR_OPEN);
  if (!entries.empty()) {
    const auto& sel = entries[selectorIndex];
    if (sel.type == OpdsEntryType::BOOK) {
      confirmLabel = tr(STR_DOWNLOAD);
    } else if (sel.id == "__opds_search__") {
      confirmLabel = tr(STR_OPDS_SEARCH);
    }
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (entries.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_NO_ENTRIES));
    renderer.displayBuffer();
    return;
  }

  const auto pageStartIndex = selectorIndex / PAGE_ITEMS * PAGE_ITEMS;
  renderer.fillRect(0, 60 + (selectorIndex % PAGE_ITEMS) * 30 - 2, pageWidth - 1, 30);

  for (size_t i = pageStartIndex; i < entries.size() && i < static_cast<size_t>(pageStartIndex + PAGE_ITEMS); i++) {
    const auto& entry = entries[i];

    // Format display text with type indicator
    std::string displayText;
    if (entry.type == OpdsEntryType::NAVIGATION) {
      displayText = "> " + entry.title;  // Folder/navigation indicator
    } else {
      // Book: "Title - Author" or just "Title"
      displayText = entry.title;
      if (!entry.author.empty()) {
        displayText += " - " + entry.author;
      }
    }

    auto item = renderer.truncatedText(UI_10_FONT_ID, displayText.c_str(), renderer.getScreenWidth() - 40);
    renderer.drawText(UI_10_FONT_ID, 20, 60 + (i % PAGE_ITEMS) * 30, item.c_str(),
                      i != static_cast<size_t>(selectorIndex));
  }

  renderer.displayBuffer();
}

void OpdsBookBrowserActivity::fetchFeed(const std::string& path) {
  const char* serverUrl = SETTINGS.opdsServerUrl;
  if (strlen(serverUrl) == 0) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_NO_SERVER_URL);
    requestUpdate();
    return;
  }

  std::string url = UrlUtils::buildUrl(serverUrl, path);
  LOG_DBG("OPDS", "Fetching: %s", url.c_str());

  OpdsParser parser;
  parser.setBaseUrl(url);

  {
    OpdsParserStream stream{parser};
    if (!HttpDownloader::fetchUrl(url, stream)) {
      state = BrowserState::ERROR;
      errorMessage = tr(STR_FETCH_FEED_FAILED);
      requestUpdate();
      return;
    }
  }

  if (!parser) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_PARSE_FEED_FAILED);
    requestUpdate();
    return;
  }

  // Store feed-level navigation URLs
  searchUrl_ = parser.getSearchUrl();
  nextPageUrl_ = parser.getNextPageUrl();
  prevPageUrl_ = parser.getPrevPageUrl();

  entries = std::move(parser).getEntries();
  LOG_DBG("OPDS", "Found %d entries (search=%s next=%s prev=%s)", entries.size(),
          searchUrl_.empty() ? "no" : "yes", nextPageUrl_.empty() ? "no" : "yes",
          prevPageUrl_.empty() ? "no" : "yes");

  // Inject virtual "Previous page" entry at the top of the list
  if (!prevPageUrl_.empty()) {
    OpdsEntry prevEntry;
    prevEntry.type = OpdsEntryType::NAVIGATION;
    prevEntry.title = tr(STR_OPDS_PREV_PAGE);
    prevEntry.href = prevPageUrl_;
    prevEntry.id = "__opds_prev__";
    entries.insert(entries.begin(), prevEntry);
  }

  // Inject virtual "Search" entry at the top of the list
  if (!searchUrl_.empty()) {
    OpdsEntry searchEntry;
    searchEntry.type = OpdsEntryType::NAVIGATION;
    searchEntry.title = tr(STR_OPDS_SEARCH);
    searchEntry.href = searchUrl_;
    searchEntry.id = "__opds_search__";
    entries.insert(entries.begin(), searchEntry);
  }

  // Inject virtual "Next page" entry at the bottom of the list
  if (!nextPageUrl_.empty()) {
    OpdsEntry nextEntry;
    nextEntry.type = OpdsEntryType::NAVIGATION;
    nextEntry.title = tr(STR_OPDS_NEXT_PAGE);
    nextEntry.href = nextPageUrl_;
    nextEntry.id = "__opds_next__";
    entries.push_back(nextEntry);
  }

  selectorIndex = 0;

  if (entries.empty()) {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_NO_ENTRIES);
    requestUpdate();
    return;
  }

  state = BrowserState::BROWSING;
  requestUpdate();
}

void OpdsBookBrowserActivity::navigateToEntry(const OpdsEntry& entry) {
  // Push current path to history before navigating
  navigationHistory.push_back(currentPath);
  currentPath = entry.href;

  state = BrowserState::LOADING;
  statusMessage = tr(STR_LOADING);
  entries.clear();
  selectorIndex = 0;
  requestUpdate(true);  // Force update to show loading state immediately before fetch

  fetchFeed(currentPath);
}

void OpdsBookBrowserActivity::navigateBack() {
  if (navigationHistory.empty()) {
    // At root, go home
    onGoHome();
  } else {
    // Go back to previous catalog
    currentPath = navigationHistory.back();
    navigationHistory.pop_back();

    state = BrowserState::LOADING;
    statusMessage = tr(STR_LOADING);
    entries.clear();
    selectorIndex = 0;
    requestUpdate();

    fetchFeed(currentPath);
  }
}

void OpdsBookBrowserActivity::downloadBook(const OpdsEntry& book) {
  state = BrowserState::DOWNLOADING;
  statusMessage = book.title;
  downloadProgress = 0;
  downloadTotal = 0;
  requestUpdate(true);

  // Build full download URL
  std::string downloadUrl = UrlUtils::buildUrl(SETTINGS.opdsServerUrl, book.href);

  // Create sanitized filename: "Title - Author.epub" or just "Title.epub" if no author
  std::string baseName = book.title;
  if (!book.author.empty()) {
    baseName += " - " + book.author;
  }
  std::string filename = "/" + StringUtils::sanitizeFilename(baseName) + ".epub";

  LOG_DBG("OPDS", "Downloading: %s -> %s", downloadUrl.c_str(), filename.c_str());

  const auto result =
      HttpDownloader::downloadToFile(downloadUrl, filename, [this](const size_t downloaded, const size_t total) {
        downloadProgress = downloaded;
        downloadTotal = total;
        requestUpdate(true);  // Force update to refresh progress bar
      });

  if (result == HttpDownloader::OK) {
    LOG_DBG("OPDS", "Download complete: %s", filename.c_str());

    // Invalidate any existing cache for this file to prevent stale metadata issues
    Epub epub(filename, "/.crosspoint");
    epub.clearCache();
    LOG_DBG("OPDS", "Cleared cache for: %s", filename.c_str());

    state = BrowserState::BROWSING;
    requestUpdate();
  } else {
    state = BrowserState::ERROR;
    errorMessage = tr(STR_DOWNLOAD_FAILED);
    requestUpdate();
  }
}

void OpdsBookBrowserActivity::launchSearch() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_OPDS_SEARCH), "", 0, false),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& text = std::get<KeyboardResult>(result.data).text;
          if (!text.empty()) {
            onSearchComplete(text);
          }
        }
      });
}

void OpdsBookBrowserActivity::onSearchComplete(const std::string& query) {
  // Replace {searchTerms} in the OpenSearch template URL
  std::string searchFeedUrl = searchUrl_;
  const std::string placeholder = "{searchTerms}";
  // URL-encode the query: replace spaces with +, encode special chars per RFC 3986
  std::string encoded;
  for (const char c : query) {
    if (c == ' ') {
      encoded += '+';
    } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
      encoded += buf;
    }
  }

  // Replace all occurrences of {searchTerms} in the template URL
  size_t pos = 0;
  while ((pos = searchFeedUrl.find(placeholder, pos)) != std::string::npos) {
    searchFeedUrl.replace(pos, placeholder.size(), encoded);
    pos += encoded.size();
  }

  LOG_DBG("OPDS", "Search URL: %s", searchFeedUrl.c_str());

  // Navigate to search results — push current path to history
  navigationHistory.push_back(currentPath);
  currentPath = searchFeedUrl;

  state = BrowserState::LOADING;
  statusMessage = tr(STR_LOADING);
  entries.clear();
  selectorIndex = 0;
  requestUpdate(true);

  fetchFeed(currentPath);
}

void OpdsBookBrowserActivity::checkAndConnectWifi() {
  // Already connected? Verify connection is valid by checking IP
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    state = BrowserState::LOADING;
    statusMessage = tr(STR_LOADING);
    requestUpdate();
    fetchFeed(currentPath);
    return;
  }

  // Not connected - launch WiFi selection screen directly
  launchWifiSelection();
}

void OpdsBookBrowserActivity::launchWifiSelection() {
  state = BrowserState::WIFI_SELECTION;
  requestUpdate();

  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void OpdsBookBrowserActivity::onWifiSelectionComplete(const bool connected) {
  if (connected) {
    LOG_DBG("OPDS", "WiFi connected via selection, fetching feed");
    state = BrowserState::LOADING;
    statusMessage = tr(STR_LOADING);
    requestUpdate(true);  // Force update to show loading state immediately before fetch
    fetchFeed(currentPath);
  } else {
    LOG_DBG("OPDS", "WiFi selection cancelled/failed");
    // Force disconnect to ensure clean state for next retry
    // This prevents stale connection status from interfering
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    state = BrowserState::ERROR;
    errorMessage = tr(STR_WIFI_CONN_FAILED);
    requestUpdate();
  }
}
