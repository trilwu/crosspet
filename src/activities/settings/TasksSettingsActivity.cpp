#include "TasksSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "tasks/TasksConfig.h"
#include "tasks/TaskProvider.h"

namespace {

constexpr int kIdxProvider = 0;
constexpr int kIdxDesignMode = 1;
constexpr int kIdxDateFilter = 2;
constexpr int kIdxOverdueFilter = 3;
constexpr int kIdxDateFormat = 4;
constexpr int kIdxOrientation = 5;
constexpr int kIdxForget = 6;

const char* designModeLabel(tasks::DesignMode m) {
  switch (m) {
    case tasks::DesignMode::Minimal: return tr(STR_TASKS_MINIMAL);
    case tasks::DesignMode::Daily:   return tr(STR_TASKS_DAILY);
  }
  return tr(STR_TASKS_MINIMAL);
}

const char* dateFilterLabel(tasks::DateFilter f) {
  switch (f) {
    case tasks::DateFilter::None:      return tr(STR_TASKS_FILTER_NONE);
    case tasks::DateFilter::Today:     return tr(STR_TASKS_FILTER_TODAY);
    case tasks::DateFilter::ThisWeek:  return tr(STR_TASKS_FILTER_THIS_WEEK);
    case tasks::DateFilter::ThisMonth: return tr(STR_TASKS_FILTER_THIS_MONTH);
  }
  return tr(STR_TASKS_FILTER_TODAY);
}

const char* overdueFilterLabel(tasks::OverdueFilter f) {
  switch (f) {
    case tasks::OverdueFilter::None:      return tr(STR_TASKS_FILTER_NONE);
    case tasks::OverdueFilter::Last7Days: return tr(STR_TASKS_FILTER_LAST_7);
    case tasks::OverdueFilter::All:       return tr(STR_TASKS_FILTER_ALL);
  }
  return tr(STR_TASKS_FILTER_LAST_7);
}

const char* orientationLabel(GfxRenderer::Orientation o) {
  switch (o) {
    case GfxRenderer::Orientation::Portrait:                  return tr(STR_PORTRAIT);
    case GfxRenderer::Orientation::PortraitInverted:          return tr(STR_INVERTED);
    case GfxRenderer::Orientation::LandscapeClockwise:        return tr(STR_LANDSCAPE_CW);
    case GfxRenderer::Orientation::LandscapeCounterClockwise: return tr(STR_LANDSCAPE_CCW);
  }
  return tr(STR_PORTRAIT);
}

GfxRenderer::Orientation nextOrientation(GfxRenderer::Orientation o) {
  switch (o) {
    case GfxRenderer::Orientation::Portrait:                  return GfxRenderer::Orientation::PortraitInverted;
    case GfxRenderer::Orientation::PortraitInverted:          return GfxRenderer::Orientation::LandscapeClockwise;
    case GfxRenderer::Orientation::LandscapeClockwise:        return GfxRenderer::Orientation::LandscapeCounterClockwise;
    case GfxRenderer::Orientation::LandscapeCounterClockwise: return GfxRenderer::Orientation::Portrait;
  }
  return GfxRenderer::Orientation::Portrait;
}

StrId menuLabelStrId(int idx) {
  switch (idx) {
    case kIdxProvider:       return StrId::STR_TASKS_PROVIDER;
    case kIdxDesignMode:     return StrId::STR_TASKS_DESIGN_MODE;
    case kIdxDateFilter:     return StrId::STR_TASKS_DATE_FILTER;
    case kIdxOverdueFilter:  return StrId::STR_TASKS_OVERDUE_FILTER;
    case kIdxDateFormat:     return StrId::STR_TASKS_DATE_FORMAT;
    case kIdxOrientation:    return StrId::STR_TASKS_ORIENTATION;
    case kIdxForget:         return StrId::STR_TASKS_FORGET;
  }
  return StrId::STR_TASKS;
}

}  // namespace

void TasksSettingsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void TasksSettingsActivity::onExit() {
  Activity::onExit();
}

void TasksSettingsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }
  buttonNavigator.onNext([this] {
    selectedIndex = (selectedIndex + 1) % kItemCount;
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selectedIndex = (selectedIndex + kItemCount - 1) % kItemCount;
    requestUpdate();
  });
}

void TasksSettingsActivity::handleSelection() {
  switch (static_cast<int>(selectedIndex)) {
    case kIdxProvider:
      // Only one provider exists today. The row stays for forward
      // compatibility; cycling becomes a real action when a 2nd lands.
      break;
    case kIdxDesignMode: {
      tasks::DesignMode next = (TASKS_CONFIG.getDesignMode() == tasks::DesignMode::Minimal)
                                   ? tasks::DesignMode::Daily
                                   : tasks::DesignMode::Minimal;
      TASKS_CONFIG.setDesignMode(next);
      break;
    }
    case kIdxDateFilter: {
      using DF = tasks::DateFilter;
      DF cur = TASKS_CONFIG.getDateFilter();
      DF next = (cur == DF::None) ? DF::Today
              : (cur == DF::Today) ? DF::ThisWeek
              : (cur == DF::ThisWeek) ? DF::ThisMonth
                                       : DF::None;
      TASKS_CONFIG.setDateFilter(next);
      break;
    }
    case kIdxOverdueFilter: {
      using OF = tasks::OverdueFilter;
      OF cur = TASKS_CONFIG.getOverdueFilter();
      OF next = (cur == OF::None) ? OF::Last7Days
              : (cur == OF::Last7Days) ? OF::All
                                        : OF::None;
      TASKS_CONFIG.setOverdueFilter(next);
      break;
    }
    case kIdxDateFormat: {
      uint8_t raw = static_cast<uint8_t>(TASKS_CONFIG.getDateFormat());
      raw = (raw + 1) % 6;
      TASKS_CONFIG.setDateFormat(static_cast<tasks::DateFormat>(raw));
      break;
    }
    case kIdxOrientation: {
      auto next = nextOrientation(TASKS_CONFIG.getActivityOrientation());
      TASKS_CONFIG.setActivityOrientation(next);
      break;
    }
    case kIdxForget: {
      startActivityForResult(
          std::make_unique<ConfirmationActivity>(renderer, mappedInput,
                                                 tr(STR_TASKS_FORGET),
                                                 tr(STR_TASKS_FORGET_PROMPT)),
          [this](const ActivityResult& result) {
            if (!result.isCancelled) {
              TASKS_CONFIG.forget();
              // Close settings so the user lands directly on the setup screen.
              finish();
              return;
            }
            requestUpdate();
          });
      return;
    }
  }
  requestUpdate();
}

void TasksSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  // In landscape, the hint strip lands on a side edge (left for CW, right for
  // CCW) instead of the bottom — so reserve horizontal space rather than
  // trimming vertical content.
  const auto orientation = renderer.getOrientation();
  const bool isLandscape = (orientation == GfxRenderer::LandscapeClockwise ||
                            orientation == GfxRenderer::LandscapeCounterClockwise);
  const bool hintOnLeft = (orientation == GfxRenderer::LandscapeClockwise);
  const int hintReserve = metrics.buttonHintsHeight + metrics.verticalSpacing * 2;
  const int hintLeftReserve = (isLandscape && hintOnLeft) ? hintReserve : 0;
  const int hintRightReserve = (isLandscape && !hintOnLeft) ? hintReserve : 0;
  const int contentWidth = pageWidth - hintLeftReserve - hintRightReserve;

  GUI.drawHeader(renderer, Rect{hintLeftReserve, metrics.topPadding, contentWidth, metrics.headerHeight}, tr(STR_TASKS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - (isLandscape ? 0 : (metrics.buttonHintsHeight + metrics.verticalSpacing * 2));

  GUI.drawList(
      renderer, Rect{hintLeftReserve, contentTop, contentWidth, contentHeight}, kItemCount,
      static_cast<int>(selectedIndex),
      [](int index) {
        return std::string(I18N.get(menuLabelStrId(index)));
      },
      nullptr, nullptr,
      [](int index) -> std::string {
        switch (index) {
          case kIdxProvider:       return std::string(TASKS_CONFIG.getActiveProvider().displayName());
          case kIdxDesignMode:     return std::string(designModeLabel(TASKS_CONFIG.getDesignMode()));
          case kIdxDateFilter:     return std::string(dateFilterLabel(TASKS_CONFIG.getDateFilter()));
          case kIdxOverdueFilter:  return std::string(overdueFilterLabel(TASKS_CONFIG.getOverdueFilter()));
          case kIdxDateFormat:     return std::string(tasks::dateFormatToString(TASKS_CONFIG.getDateFormat()));
          case kIdxOrientation:    return std::string(orientationLabel(TASKS_CONFIG.getActivityOrientation()));
          case kIdxForget:         return std::string();  // action — no value column
        }
        return std::string();
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
