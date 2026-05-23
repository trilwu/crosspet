#pragma once

#include <cstdint>

namespace tasks {

// Date upper bound for fetched tasks. Calendar boundaries (ThisWeek /
// ThisMonth) are resolved against `time(nullptr)` at fetch time. Defaults
// to Today.
enum class DateFilter : uint8_t {
  None = 0,
  Today = 1,
  ThisWeek = 2,
  ThisMonth = 3,
};

// Overdue lower bound. Defaults to Last7Days.
enum class OverdueFilter : uint8_t {
  None = 0,
  Last7Days = 1,
  All = 2,
};

struct TasksFilter {
  DateFilter date = DateFilter::Today;
  OverdueFilter overdue = OverdueFilter::Last7Days;
};

}  // namespace tasks
