#pragma once
#include <Epub.h>
#include <I18n.h>

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class EpubReaderMenuActivity final : public Activity {
 public:
  // Menu actions available from the reader menu.
  enum class MenuAction {
    SELECT_CHAPTER,
    FOOTNOTES,
    GO_TO_PERCENT,
    FONT_FAMILY,
    FONT_SIZE,
    AUTO_PAGE_TURN,
    AUTO_PAGE_TURN_SPEED,
    ROTATE_SCREEN,
    SCREENSHOT,
    DISPLAY_QR,
    GO_HOME,
    SYNC,
    STARRED_PAGES,
    DELETE_CACHE
#ifdef ENABLE_BLE
    , BLUETOOTH
#endif
  };

  explicit EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                  const int currentPage, const int totalPages, const int bookProgressPercent,
                                  const uint8_t currentOrientation, const bool hasFootnotes,
                                  const bool hasStarredPages, const uint8_t currentPageTurnOption = 0);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct MenuItem {
    MenuAction action;
    StrId labelId;
  };

  // Section header with index range into menuItems
  struct SectionInfo {
    const char* label;   // Section header text (uppercase)
    int startIndex;      // First menu item index in this section
    int count;           // Number of items in this section
  };

  static std::vector<MenuItem> buildMenuItems(bool hasFootnotes, bool hasStarredPages);
  static std::vector<SectionInfo> buildSections(bool hasFootnotes, bool hasStarredPages);

  // Fixed menu layout built at construction time
  const std::vector<MenuItem> menuItems;
  const std::vector<SectionInfo> sections;

  int selectedIndex = 0;

  ButtonNavigator buttonNavigator;
  std::string title = "Reader Menu";
  uint8_t pendingOrientation = 0;
  uint8_t selectedPageTurnOption = 0;
  const std::vector<StrId> orientationLabels = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW, StrId::STR_INVERTED,
                                                StrId::STR_LANDSCAPE_CCW};
  static constexpr uint8_t AUTO_TURN_MAX_PPM = 20;
  mutable char pageTurnValueBuf[8] = {};
  int currentPage = 0;
  int totalPages = 0;
  int bookProgressPercent = 0;
};
