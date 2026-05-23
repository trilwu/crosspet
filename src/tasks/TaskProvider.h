#pragma once

#include <cstdint>
#include <vector>

#include "Task.h"
#include "TasksFilter.h"

namespace tasks {

enum class FetchResult : uint8_t {
  Ok = 0,
  NoAuth,        // empty / invalid token (provider-decided)
  NetworkError,  // WiFi / DNS / TCP / TLS
  AuthError,     // 401 / 403
  RateLimited,   // 429
  ParseError,    // malformed response body
  Empty,         // call succeeded but no usable data
};

// Token-based task provider interface. Implementations are global static
// singletons (zero heap for the provider itself). Caller owns `out` and
// MUST call out.reserve(N) before invoking fetch — the ESP32-C3 RAM
// rules in .skills/SKILL.md make this mandatory.
//
// Honest limitation: this interface assumes synchronous token-based auth.
// OAuth providers (Google Tasks, Microsoft To Do) will need a
// TokenProvider subinterface or interface evolution when they land. The
// shape was chosen against Todoist as the v1 concrete; this is documented
// so the next person isn't surprised by the assumption.
class TaskProvider {
 public:
  virtual ~TaskProvider() = default;

  // Human-readable provider name for UI (e.g. "Todoist"). Not i18n'd —
  // backend names are de-facto proper nouns.
  virtual const char* displayName() const = 0;

  // Fetch tasks matching `filter`. WiFi must be up and the system clock
  // must be set (NTP) for calendar-bounded date filters. Synchronous.
  virtual FetchResult fetch(const TasksFilter& filter,
                            std::vector<Task>& out) = 0;
};

}  // namespace tasks
