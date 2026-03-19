#pragma once
#include <functional>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

struct RecentBook;
struct Rect;

class HomeActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  bool firstRenderDone = false;
  bool weatherRefreshing = false;  // Show "refreshing" status on screen
  const char* syncResultMsg = nullptr;  // "OK" or "Failed" after sync
  unsigned long syncResultExpiry = 0;   // millis() when to clear message
  bool syncTriggered = false;      // Guard against re-triggering sync while held
  bool coverRendered = false;      // Track if cover has been rendered once
  bool coverBufferStored = false;  // Track if cover buffer is stored
  uint8_t* coverBuffer = nullptr;  // HomeActivity's own buffer for cover image
  std::vector<RecentBook> recentBooks;
  void onSelectBook(const std::string& path);
  void onFileBrowserOpen();
  void onRecentBooksOpen();
  void onVirtualPetOpen();
  void onFileTransferOpen();
  void onSettingsOpen();
  void onToolsOpen();
  void onOpdsBrowserOpen();

  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void loadRecentBooks(int maxBooks);
  void loadRecentCovers(int coverHeight);

  // Original upstream layout (CLASSIC/LYRA/LYRA_3_COVERS)
  int getMenuItemCount() const;
  void renderOriginal();
  void loopOriginal();

  // CrossPet (new card layout) render helpers
  void renderContinueReadingCard();
  void renderRecentCovers();       // Draw cover thumbnails (cached in buffer)
  void renderRecentSelection();    // Selection highlight for recent covers
  void renderReadingStatsBar();    // Pet widget or reading stats in gap
  void renderBottomBarIcons();      // Static icons + labels (cached in buffer)
  void renderBottomBarSelection();  // Selection highlight only (per-frame)
  void renderButtonHints();         // Static button hint shapes (cached in buffer)
  void renderSelectionHighlight();
  void renderFocusCard();           // Focus mode: large single-book card

  // CrossPet Classic (v1.6.8 grid layout) render helpers
  void renderCoverPanel(int panelX, int panelY, int panelW, int panelH, int coverH);
  void renderProgressPanel(int panelX, int panelY, int panelW, int panelH);
  void renderGridCell(int cellX, int cellY, int cellW, int cellH,
                      int gridIdx, const uint8_t* icon, const char* label);
  void renderClassicSelectionHighlight(int panelX, int panelY, int panelW, int panelH);

  // Shared render helpers
  void renderPetStatusWidget(int headerH);
  void renderHeaderClock();
  void doSync();
  void performSyncAfterWifi();

  // Theme-specific render/loop dispatchers
  void renderCrossPet();
  void renderClassic();
  void loopCrossPet();
  void loopClassic();

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Home", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
