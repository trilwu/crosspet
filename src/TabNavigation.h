#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "components/themes/BaseTheme.h"

class Activity;
class GfxRenderer;
class MappedInputManager;

// Global tab identifiers for top-level navigation
enum Tab : uint8_t { TAB_HOME = 0, TAB_LIBRARY = 1, TAB_STATS = 2, TAB_SETTINGS = 3, TAB_COUNT = 4 };

// Create the activity for a given tab
std::unique_ptr<Activity> createTabActivity(Tab tab, GfxRenderer& renderer, MappedInputManager& mappedInput);

// Build tab info array for rendering the tab bar
std::vector<TabInfo> getGlobalTabs(Tab currentTab);

// Check L/R input and return next tab, or -1 if no change
int handleTabInput(MappedInputManager& mappedInput, Tab currentTab);
