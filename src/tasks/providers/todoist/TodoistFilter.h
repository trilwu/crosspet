#pragma once

#include <string>

#include "tasks/TasksFilter.h"

namespace tasks::todoist {

// Translate a generic two-axis TasksFilter into a Todoist query DSL string.
// Calendar boundaries (ThisWeek / ThisMonth) are resolved against
// `time(nullptr)` in the device's current timezone, so the caller must
// ensure NTP has set the clock before calling.
//
// Returns the *unencoded* query — caller is responsible for URL encoding
// before substituting into the endpoint URL.
std::string buildQuery(DateFilter dateF, OverdueFilter overdueF);

// Percent-encode an unreserved-only string for the URL query value.
std::string urlEncode(const std::string& s);

}  // namespace tasks::todoist
