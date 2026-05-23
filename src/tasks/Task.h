#pragma once

#include <cstddef>
#include <cstdint>

namespace tasks {

// Fixed-size POD so a std::vector<Task> reserves a contiguous block without
// any per-task heap allocation. Title is hard-truncated; the active view
// ellipsises further when rendering.
struct Task {
  static constexpr size_t kTitleCapacity = 96;
  static constexpr size_t kDueDateCapacity = 11;  // "YYYY-MM-DD\0"
  static constexpr size_t kDueTimeCapacity = 6;   // "HH:MM\0"

  char title[kTitleCapacity];
  char dueDate[kDueDateCapacity];  // empty string if no due date
  char dueTime[kDueTimeCapacity];  // empty string if no time
  uint8_t priority;                // 1 (lowest) to 4 (highest), 0 = unknown
  bool overdue;
};

}  // namespace tasks
