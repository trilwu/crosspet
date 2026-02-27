#include "ToolsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "ClockActivity.h"
#include "ConferenceBadgeActivity.h"
#include "DailyQuoteActivity.h"
#include "PomodoroActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void ToolsActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  requestUpdate();
}

void ToolsActivity::loop() {
  buttonNavigator.onNext([this] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, MENU_COUNT);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, MENU_COUNT);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (selectorIndex) {
      case 0:
        activityManager.pushActivity(std::make_unique<ClockActivity>(renderer, mappedInput));
        break;
      case 1:
        activityManager.pushActivity(std::make_unique<PomodoroActivity>(renderer, mappedInput));
        break;
      case 2:
        activityManager.pushActivity(std::make_unique<DailyQuoteActivity>(renderer, mappedInput));
        break;
      case 3:
        activityManager.pushActivity(std::make_unique<ConferenceBadgeActivity>(renderer, mappedInput));
        break;
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

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TOOLS));

  const char* menuLabels[] = {tr(STR_CLOCK), tr(STR_POMODORO), tr(STR_DAILY_QUOTE), tr(STR_CONFERENCE_BADGE)};

  const int menuTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int menuHeight = pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(renderer, Rect{0, menuTop, pageWidth, menuHeight}, MENU_COUNT, selectorIndex,
               [&menuLabels](int index) { return std::string(menuLabels[index]); });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
