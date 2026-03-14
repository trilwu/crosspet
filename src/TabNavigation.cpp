#include "TabNavigation.h"

#include <I18n.h>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/home/FileBrowserActivity.h"
#include "activities/home/HomeActivity.h"
#include "activities/settings/SettingsActivity.h"
#include "activities/stats/StatisticsActivity.h"

std::unique_ptr<Activity> createTabActivity(Tab tab, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  switch (tab) {
    case TAB_HOME:
      return std::make_unique<HomeActivity>(renderer, mappedInput);
    case TAB_LIBRARY:
      return std::make_unique<FileBrowserActivity>(renderer, mappedInput);
    case TAB_STATS:
      return std::make_unique<StatisticsActivity>(renderer, mappedInput);
    case TAB_SETTINGS:
      return std::make_unique<SettingsActivity>(renderer, mappedInput);
    default:
      return std::make_unique<HomeActivity>(renderer, mappedInput);
  }
}

std::vector<TabInfo> getGlobalTabs(Tab currentTab) {
  std::vector<TabInfo> tabs;
  tabs.reserve(TAB_COUNT);
  TabInfo t;
  t.label = I18N.get(StrId::STR_HOME);
  t.selected = (currentTab == TAB_HOME);
  tabs.push_back(t);
  t.label = I18N.get(StrId::STR_TAB_LIBRARY);
  t.selected = (currentTab == TAB_LIBRARY);
  tabs.push_back(t);
  t.label = I18N.get(StrId::STR_STATISTICS);
  t.selected = (currentTab == TAB_STATS);
  tabs.push_back(t);
  t.label = I18N.get(StrId::STR_SETTINGS_TITLE);
  t.selected = (currentTab == TAB_SETTINGS);
  tabs.push_back(t);
  return tabs;
}

int handleTabInput(MappedInputManager& mappedInput, Tab currentTab) {
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    return currentTab > 0 ? currentTab - 1 : TAB_COUNT - 1;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    return currentTab < TAB_COUNT - 1 ? currentTab + 1 : 0;
  }
  return -1;
}
