#include "ToolsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "ClockActivity.h"
#include "PomodoroActivity.h"
#include "TwentyFortyEightActivity.h"
#include "MinesweeperActivity.h"
#include "SudokuActivity.h"
#include "CaroActivity.h"
#include "ChessActivity.h"
#include "VirtualPetActivity.h"
#include "WeatherActivity.h"
#include "ReadingStatsActivity.h"
#include "SleepImagePickerActivity.h"
#include "activities/browser/OpdsBookBrowserActivity.h"
#include "../flashcard/FlashcardDeckListActivity.h"
#include "components/UITheme.h"
#include "CrossPetSettings.h"
#include "CrossPointSettings.h"
#include "fontIds.h"

void ToolsActivity::buildMenu() {
  menuEntries.clear();

  // File Transfer is always visible
  menuEntries.push_back({StrId::STR_FILE_TRANSFER, [this] { activityManager.goToFileTransfer(); }});

  // Apps — filtered by CrossPetSettings toggles
  if (PET_SETTINGS.appClock)
    menuEntries.push_back({StrId::STR_CLOCK, [this] {
      activityManager.pushActivity(std::make_unique<ClockActivity>(renderer, mappedInput));
    }});
  if (PET_SETTINGS.appWeather)
    menuEntries.push_back({StrId::STR_WEATHER, [this] {
      activityManager.pushActivity(std::make_unique<WeatherActivity>(renderer, mappedInput));
    }});
  if (PET_SETTINGS.appPomodoro)
    menuEntries.push_back({StrId::STR_POMODORO, [this] {
      activityManager.pushActivity(std::make_unique<PomodoroActivity>(renderer, mappedInput));
    }});
  if (PET_SETTINGS.appVirtualPet)
    menuEntries.push_back({StrId::STR_VIRTUAL_PET, [this] {
      activityManager.pushActivity(std::make_unique<VirtualPetActivity>(renderer, mappedInput));
    }});
  if (PET_SETTINGS.appReadingStats)
    menuEntries.push_back({StrId::STR_READING_STATS_APP, [this] {
      activityManager.pushActivity(std::make_unique<ReadingStatsActivity>(renderer, mappedInput));
    }});
  if (PET_SETTINGS.appSleepImagePicker)
    menuEntries.push_back({StrId::STR_SLEEP_IMAGE_PICKER, [this] {
      activityManager.pushActivity(std::make_unique<SleepImagePickerActivity>(renderer, mappedInput));
    }});

  if (PET_SETTINGS.appFlashcard)
    menuEntries.push_back({StrId::STR_FLASHCARD, [this] {
      activityManager.pushActivity(std::make_unique<FlashcardDeckListActivity>(renderer, mappedInput));
    }});

  // OPDS browser (if configured)
  if (SETTINGS.opdsServerUrl[0])
    menuEntries.push_back({StrId::STR_OPDS_BROWSER, [this] {
      activityManager.pushActivity(std::make_unique<OpdsBookBrowserActivity>(renderer, mappedInput));
    }});

  // Games — single master toggle for all games
  if (PET_SETTINGS.appGames) {
    menuEntries.push_back({StrId::STR_CHESS, [this] {
      activityManager.pushActivity(std::make_unique<ChessActivity>(renderer, mappedInput));
    }});
    menuEntries.push_back({StrId::STR_CARO, [this] {
      activityManager.pushActivity(std::make_unique<CaroActivity>(renderer, mappedInput));
    }});
    menuEntries.push_back({StrId::STR_SUDOKU, [this] {
      activityManager.pushActivity(std::make_unique<SudokuActivity>(renderer, mappedInput));
    }});
    menuEntries.push_back({StrId::STR_MINESWEEPER, [this] {
      activityManager.pushActivity(std::make_unique<MinesweeperActivity>(renderer, mappedInput));
    }});
    menuEntries.push_back({StrId::STR_2048, [this] {
      activityManager.pushActivity(std::make_unique<TwentyFortyEightActivity>(renderer, mappedInput));
    }});
  }
}

void ToolsActivity::onEnter() {
  Activity::onEnter();
  buildMenu();
  selectorIndex = 0;
  requestUpdate();
}

void ToolsActivity::loop() {
  const int menuCount = static_cast<int>(menuEntries.size());

  buttonNavigator.onNext([this, menuCount] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuCount] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex >= 0 && selectorIndex < menuCount) {
      menuEntries[selectorIndex].launch();
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void ToolsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int menuCount = static_cast<int>(menuEntries.size());

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TOOLS));

  const int menuTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int menuHeight = pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(renderer, Rect{0, menuTop, pageWidth, menuHeight}, menuCount, selectorIndex,
               [this](int index) -> std::string {
                 return I18N.get(menuEntries[index].labelId);
               });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
