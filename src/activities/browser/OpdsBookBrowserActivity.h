#pragma once
#include <OpdsParser.h>

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

/**
 * Activity for browsing and downloading books from an OPDS server.
 * Supports navigation through catalog hierarchy and downloading EPUBs.
 * When WiFi connection fails, launches WiFi selection to let user connect.
 */
class OpdsBookBrowserActivity final : public Activity {
 public:
  enum class BrowserState {
    CHECK_WIFI,      // Checking WiFi connection
    WIFI_SELECTION,  // WiFi selection subactivity is active
    LOADING,         // Fetching OPDS feed
    BROWSING,        // Displaying entries (navigation or books)
    DOWNLOADING,     // Downloading selected EPUB
    ERROR            // Error state with message
  };

  explicit OpdsBookBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("OpdsBookBrowser", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  BrowserState state = BrowserState::LOADING;
  std::vector<OpdsEntry> entries;
  std::vector<std::string> navigationHistory;  // Stack of previous feed paths for back navigation
  std::string currentPath;                     // Current feed path being displayed
  int selectorIndex = 0;
  std::string errorMessage;
  std::string statusMessage;
  size_t downloadProgress = 0;
  size_t downloadTotal = 0;

  // Feed-level navigation URLs from OPDS parser
  std::string searchUrl_;   // OpenSearch template URL (contains {searchTerms})
  std::string nextPageUrl_; // URL for the next page of results
  std::string prevPageUrl_; // URL for the previous page of results

  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void fetchFeed(const std::string& path);
  void navigateToEntry(const OpdsEntry& entry);
  void navigateBack();
  void downloadBook(const OpdsEntry& book);
  void launchSearch();
  void onSearchComplete(const std::string& query);
  bool preventAutoSleep() override { return true; }
};
