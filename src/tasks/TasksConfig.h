#pragma once

#include <GfxRenderer.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "TasksFilter.h"

namespace tasks {

class TaskProvider;  // forward decl

enum class Provider : uint8_t {
  Todoist = 0,
};

// Overall layout. Minimal = compact list. Daily = date + day-of-week + tasks.
enum class DesignMode : uint8_t {
  Minimal = 0,  // ← default
  Daily = 1,
};

// Display format for any date rendered by the activity. Year is omitted
// (task date ranges never span multiple years in normal use).
enum class DateFormat : uint8_t {
  DayMonthSlash = 0,  // 31/12  ← default
  MonthDaySlash = 1,  // 12/31
  DayMonthDash = 2,   // 31-12
  MonthDayDash = 3,   // 12-31
  DayMonthDot = 4,    // 31.12
  MonthDayDot = 5,    // 12.31
};

// Render a date into `out` per `fmt`. Always writes a NUL terminator.
// outSize must be >= 6. Returns chars written (excluding NUL), 0 on overflow.
size_t formatDate(int day, int month, DateFormat fmt, char* out, size_t outSize);

// Canonical wire string for `f`, e.g. "dd/mm". Used as JSON value and as
// settings-row label (universal English shorthand serves both).
const char* dateFormatToString(DateFormat f);

// Canonical wire string for the design mode ("minimal" / "daily").
const char* designModeToString(DesignMode d);

class TasksConfig {
 public:
  static TasksConfig& getInstance();

  // Loads /.crosspoint/tasks.json. If the file doesn't exist, writes a
  // stub with default values + empty todoistApiToken (user is expected to
  // edit the file via SD card to populate the token). Returns true on
  // success in either case; false only on disk error.
  bool load();

  // Token getter (no setter — token is only loaded from disk).
  const std::string& getTodoistApiToken() const { return todoistApiToken; }
  bool hasToken() const { return !todoistApiToken.empty(); }

  Provider getProvider() const { return provider; }
  DesignMode getDesignMode() const { return designMode; }
  DateFilter getDateFilter() const { return dateFilter; }
  OverdueFilter getOverdueFilter() const { return overdueFilter; }
  DateFormat getDateFormat() const { return dateFormat; }
  GfxRenderer::Orientation getActivityOrientation() const { return activityOrientation; }
  GfxRenderer::Orientation getSnapshotOrientation() const { return snapshotOrientation; }

  // Convenience: build a TasksFilter from current date+overdue filters.
  TasksFilter getFilter() const { return TasksFilter{dateFilter, overdueFilter}; }

  // Active provider singleton. Switches on `provider`.
  TaskProvider& getActiveProvider();

  // Setters — value-change-guarded, persist on change. Return false on
  // disk error; return true if nothing changed (no-op).
  bool setProvider(Provider p);
  bool setDesignMode(DesignMode d);
  bool setDateFilter(DateFilter f);
  bool setOverdueFilter(OverdueFilter f);
  bool setDateFormat(DateFormat f);
  bool setActivityOrientation(GfxRenderer::Orientation o);
  bool setSnapshotOrientation(GfxRenderer::Orientation o);

  // Blank the token on disk. Used by the "Forget" settings row. Activity
  // reverts to setup state on next launch.
  bool forget();

 private:
  TasksConfig() = default;
  TasksConfig(const TasksConfig&) = delete;
  TasksConfig& operator=(const TasksConfig&) = delete;

  bool persist();
  bool writeStubIfMissing();

  Provider provider = Provider::Todoist;
  std::string todoistApiToken;
  DesignMode designMode = DesignMode::Minimal;
  DateFilter dateFilter = DateFilter::Today;
  OverdueFilter overdueFilter = OverdueFilter::Last7Days;
  DateFormat dateFormat = DateFormat::DayMonthSlash;
  GfxRenderer::Orientation activityOrientation = GfxRenderer::Orientation::Portrait;
  GfxRenderer::Orientation snapshotOrientation = GfxRenderer::Orientation::Portrait;
  bool loaded = false;
};

}  // namespace tasks

#define TASKS_CONFIG tasks::TasksConfig::getInstance()
