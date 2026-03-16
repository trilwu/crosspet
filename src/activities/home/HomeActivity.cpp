#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "CrossPetSettings.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/tools/WeatherActivity.h"
#include "WifiCredentialStore.h"
#include <WiFi.h>
#include "pet/PetEvolution.h"
#include "pet/PetManager.h"
#include "util/StringUtils.h"

// ── Buffer management ─────────────────────────────────────────────────────────

bool HomeActivity::storeCoverBuffer() {
  uint8_t* fb = renderer.getFrameBuffer();
  if (!fb) return false;
  freeCoverBuffer();
  coverBuffer = static_cast<uint8_t*>(malloc(GfxRenderer::getBufferSize()));
  if (!coverBuffer) return false;
  memcpy(coverBuffer, fb, GfxRenderer::getBufferSize());
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer) return false;
  uint8_t* fb = renderer.getFrameBuffer();
  if (!fb) return false;
  memcpy(fb, coverBuffer, GfxRenderer::getBufferSize());
  return true;
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) { free(coverBuffer); coverBuffer = nullptr; }
  coverBufferStored = false;
}

// ── Book loading ──────────────────────────────────────────────────────────────

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  for (const RecentBook& b : RECENT_BOOKS.getBooks()) {
    if (static_cast<int>(recentBooks.size()) >= maxBooks) break;
    if (Storage.exists(b.path.c_str())) recentBooks.push_back(b);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  Rect popup;
  bool showingLoading = false;
  bool anyChanged = false;
  int progress = 0;

  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(coverPath.c_str())) {
        bool generated = false;
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) { showingLoading = true; popup = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP)); }
          GUI.fillPopupProgress(renderer, popup, 10 + progress * (90 / (int)recentBooks.size()));
          generated = epub.generateThumbBmp(coverHeight);
          if (!generated) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            if (!showingLoading) { showingLoading = true; popup = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP)); }
            GUI.fillPopupProgress(renderer, popup, 10 + progress * (90 / (int)recentBooks.size()));
            generated = xtc.generateThumbBmp(coverHeight);
            if (!generated) { RECENT_BOOKS.updateBook(book.path, book.title, book.author, ""); book.coverBmpPath = ""; }
          }
        }
        if (generated) anyChanged = true;
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
  if (anyChanged) {
    coverRendered = false;
    requestUpdate();
  }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void HomeActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  coverRendered = false;
  firstRenderDone = false;
  recentsLoaded = false;
  recentsLoading = false;
  loadRecentBooks(4);
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();
  freeCoverBuffer();
}

// ── Input / Render dispatchers ────────────────────────────────────────────────

void HomeActivity::loop() {
  if (SETTINGS.uiTheme <= CrossPointSettings::LYRA_3_COVERS)
    loopOriginal();
  else if (SETTINGS.uiTheme == CrossPointSettings::CROSSPET_CLASSIC)
    loopClassic();
  else
    loopCrossPet();
}

void HomeActivity::render(RenderLock&&) {
  if (SETTINGS.uiTheme <= CrossPointSettings::LYRA_3_COVERS)
    renderOriginal();
  else if (SETTINGS.uiTheme == CrossPointSettings::CROSSPET_CLASSIC)
    renderClassic();
  else
    renderCrossPet();
}

// ── Shared render helpers ─────────────────────────────────────────────────────

void HomeActivity::renderPetStatusWidget(int headerH) {
  if (!PET_SETTINGS.homeShowPetStatus) return;
  if (!PET_MANAGER.exists() || !PET_MANAGER.isAlive()) return;

  const auto& ps = PET_MANAGER.getState();
  const PetMood mood = PET_MANAGER.getMood();
  const int screenW = renderer.getScreenWidth();

  const char* name = ps.petName[0] ? ps.petName : PetEvolution::variantStageName(ps.stage, ps.evolutionVariant);
  char petBuf[64];

  if (ps.hunger < 30)                 snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_HUNGRY), name);
  else if (mood == PetMood::SAD)      snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_SAD), name);
  else if (mood == PetMood::SICK)     snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_SICK), name);
  else if (mood == PetMood::NEEDY)    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_NEEDY), name);
  else if (mood == PetMood::SLEEPING) snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_SLEEPING), name);
  else if (ps.currentStreak > 7)      snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_STREAK), name, ps.currentStreak);
  else if (mood == PetMood::HAPPY)    snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_HAPPY), name);
  else                                snprintf(petBuf, sizeof(petBuf), tr(STR_PET_HOME_DEFAULT), name);

  int rightMargin = 12 + BaseMetrics::values.batteryWidth + 4;
  if (SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS) {
    rightMargin += renderer.getTextWidth(SMALL_FONT_ID, "100%") + 4;
  }
  const int availW = screenW - rightMargin - 4;
  auto truncated = renderer.truncatedText(SMALL_FONT_ID, petBuf, availW);
  const int finalW = renderer.getTextWidth(SMALL_FONT_ID, truncated.c_str());
  const int x = screenW - rightMargin - finalW;
  renderer.drawText(SMALL_FONT_ID, x, 5, truncated.c_str(), true);
}

void HomeActivity::renderHeaderClock() {
  int nextX = 10;

  if (!PET_SETTINGS.homeShowClock) {
    // Show heap even without clock
    if (SETTINGS.showFreeHeap) {
      char heapBuf[12];
      snprintf(heapBuf, sizeof(heapBuf), "%dKB", ESP.getFreeHeap() / 1024);
      renderer.drawText(SMALL_FONT_ID, nextX, 5, heapBuf, true);
    }
    return;
  }

  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[8];
  if (timeinfo.tm_year >= 125)
    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  else
    snprintf(buf, sizeof(buf), "--:--");

  const int clockW = renderer.getTextWidth(SMALL_FONT_ID, buf);
  renderer.drawText(SMALL_FONT_ID, nextX, 5, buf);
  nextX += clockW + 6;

  // Developer: show free heap after clock
  if (SETTINGS.showFreeHeap) {
    char heapBuf[12];
    snprintf(heapBuf, sizeof(heapBuf), "%dKB", ESP.getFreeHeap() / 1024);
    renderer.drawText(SMALL_FONT_ID, nextX, 5, heapBuf, true);
    nextX += renderer.getTextWidth(SMALL_FONT_ID, heapBuf) + 6;
  }

  if (!PET_SETTINGS.homeShowWeather) return;

  // Weather temp or sync status next to clock
  const int weatherX = nextX;
  if (weatherRefreshing) {
    renderer.drawText(SMALL_FONT_ID, weatherX, 5, "...");
  } else if (syncResultMsg) {
    renderer.drawText(SMALL_FONT_ID, weatherX, 5, syncResultMsg);
  } else {
    // Cache weather string in memory — avoid SD read on every frame
    static char cachedWeather[16] = "";
    static unsigned long lastWeatherLoad = 0;
    if (millis() - lastWeatherLoad > 60000 || cachedWeather[0] == '\0') {
      WeatherData wData;
      uint8_t wCity = 0;
      char wTime[8] = "";
      if (WeatherActivity::loadWeatherCache(wData, wCity, wTime, sizeof(wTime))) {
        snprintf(cachedWeather, sizeof(cachedWeather), "%.0f%s", WeatherActivity::convertTemp(wData.temperature), WeatherActivity::tempUnitSuffix());
      }
      lastWeatherLoad = millis();
    }
    if (cachedWeather[0]) {
      renderer.drawText(SMALL_FONT_ID, weatherX, 5, cachedWeather);
    }
  }
}

// ── Sync ──────────────────────────────────────────────────────────────────────

void HomeActivity::performSyncAfterWifi() {
  static char syncBuf[24];
  weatherRefreshing = true;
  requestUpdateAndWait();

  if (WiFi.status() != WL_CONNECTED) {
    const auto& ssid = WIFI_STORE.getLastConnectedSsid();
    const auto* cred = ssid.empty() ? nullptr : WIFI_STORE.findCredential(ssid);
    if (cred) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
      const unsigned long connectStart = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - connectStart < 8000) {
        delay(100);
      }
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(false);
        WiFi.mode(WIFI_OFF);
        weatherRefreshing = false;
        snprintf(syncBuf, sizeof(syncBuf), "%s", tr(STR_WIFI_CONN_FAILED));
        syncResultMsg = syncBuf;
        syncResultExpiry = millis() + 3000;
        requestUpdate();
        return;
      }
    }
  }

  int rc = WeatherActivity::silentRefresh();
  weatherRefreshing = false;
  if (rc == 0)      snprintf(syncBuf, sizeof(syncBuf), "%s", tr(STR_SYNC_OK));
  else if (rc == 2) snprintf(syncBuf, sizeof(syncBuf), "%s", tr(STR_WIFI_TIMEOUT));
  else              snprintf(syncBuf, sizeof(syncBuf), tr(STR_API_ERROR), rc);
  syncResultMsg = syncBuf;
  syncResultExpiry = millis() + 3000;
  coverRendered = false;
  requestUpdate();
}

void HomeActivity::doSync() {
  static char syncBuf2[24];
  if (WIFI_STORE.getLastConnectedSsid().empty()) {
    snprintf(syncBuf2, sizeof(syncBuf2), "%s", tr(STR_WIFI_CONN_FAILED));
    syncResultMsg = syncBuf2;
    syncResultExpiry = millis() + 3000;
    requestUpdate();
    return;
  }
  performSyncAfterWifi();
}

// ── Actions ───────────────────────────────────────────────────────────────────

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }
void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }
void HomeActivity::onRecentBooksOpen() { activityManager.goToRecentBooks(); }
void HomeActivity::onVirtualPetOpen()  { activityManager.goToVirtualPet(); }
void HomeActivity::onFileTransferOpen(){ activityManager.goToFileTransfer(); }
void HomeActivity::onSettingsOpen()    { activityManager.goToSettings(); }
void HomeActivity::onToolsOpen()       { activityManager.goToTools(); }
void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }
