#include "TasksConfig.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

// Note: TaskProvider include and getActiveProvider() definition deferred
// to Phase 2 (when TodoistProvider lands). TasksConfig.h declares the
// method; the definition lives alongside TodoistProvider so this file
// doesn't depend on Phase 2 code.

namespace tasks {

namespace {

constexpr const char* kConfigPath = "/.crosspoint/tasks.json";

const char* providerToString(Provider p) {
  switch (p) {
    case Provider::Todoist: return "todoist";
  }
  return "todoist";
}

Provider providerFromString(const char* s, Provider fallback) {
  if (!s) return fallback;
  if (strcmp(s, "todoist") == 0) return Provider::Todoist;
  return fallback;
}

const char* orientationToString(GfxRenderer::Orientation o) {
  switch (o) {
    case GfxRenderer::Orientation::Portrait: return "portrait";
    case GfxRenderer::Orientation::PortraitInverted: return "portrait_inverted";
    case GfxRenderer::Orientation::LandscapeClockwise: return "landscape_cw";
    case GfxRenderer::Orientation::LandscapeCounterClockwise: return "landscape_ccw";
  }
  return "portrait";
}

GfxRenderer::Orientation orientationFromString(const char* s, GfxRenderer::Orientation fallback) {
  if (!s) return fallback;
  if (strcmp(s, "portrait") == 0) return GfxRenderer::Orientation::Portrait;
  if (strcmp(s, "portrait_inverted") == 0) return GfxRenderer::Orientation::PortraitInverted;
  if (strcmp(s, "landscape_cw") == 0) return GfxRenderer::Orientation::LandscapeClockwise;
  if (strcmp(s, "landscape_ccw") == 0) return GfxRenderer::Orientation::LandscapeCounterClockwise;
  return fallback;
}

const char* dateFilterToString(DateFilter f) {
  switch (f) {
    case DateFilter::None:      return "none";
    case DateFilter::Today:     return "today";
    case DateFilter::ThisWeek:  return "this_week";
    case DateFilter::ThisMonth: return "this_month";
  }
  return "today";
}

DateFilter dateFilterFromString(const char* s, DateFilter fallback) {
  if (!s) return fallback;
  if (strcmp(s, "none") == 0)       return DateFilter::None;
  if (strcmp(s, "today") == 0)      return DateFilter::Today;
  if (strcmp(s, "this_week") == 0)  return DateFilter::ThisWeek;
  if (strcmp(s, "this_month") == 0) return DateFilter::ThisMonth;
  return fallback;
}

const char* overdueFilterToString(OverdueFilter f) {
  switch (f) {
    case OverdueFilter::None:      return "none";
    case OverdueFilter::Last7Days: return "last_7_days";
    case OverdueFilter::All:       return "all";
  }
  return "last_7_days";
}

OverdueFilter overdueFilterFromString(const char* s, OverdueFilter fallback) {
  if (!s) return fallback;
  if (strcmp(s, "none") == 0)        return OverdueFilter::None;
  if (strcmp(s, "last_7_days") == 0) return OverdueFilter::Last7Days;
  if (strcmp(s, "all") == 0)         return OverdueFilter::All;
  return fallback;
}

DateFormat dateFormatFromString(const char* s, DateFormat fallback) {
  if (!s) return fallback;
  if (strcmp(s, "dd/mm") == 0) return DateFormat::DayMonthSlash;
  if (strcmp(s, "mm/dd") == 0) return DateFormat::MonthDaySlash;
  if (strcmp(s, "dd-mm") == 0) return DateFormat::DayMonthDash;
  if (strcmp(s, "mm-dd") == 0) return DateFormat::MonthDayDash;
  if (strcmp(s, "dd.mm") == 0) return DateFormat::DayMonthDot;
  if (strcmp(s, "mm.dd") == 0) return DateFormat::MonthDayDot;
  return fallback;
}

DesignMode designModeFromString(const char* s, DesignMode fallback) {
  if (!s) return fallback;
  if (strcmp(s, "minimal") == 0) return DesignMode::Minimal;
  if (strcmp(s, "daily") == 0)   return DesignMode::Daily;
  return fallback;
}

}  // namespace

const char* dateFormatToString(DateFormat f) {
  switch (f) {
    case DateFormat::DayMonthSlash: return "dd/mm";
    case DateFormat::MonthDaySlash: return "mm/dd";
    case DateFormat::DayMonthDash:  return "dd-mm";
    case DateFormat::MonthDayDash:  return "mm-dd";
    case DateFormat::DayMonthDot:   return "dd.mm";
    case DateFormat::MonthDayDot:   return "mm.dd";
  }
  return "dd/mm";
}

const char* designModeToString(DesignMode d) {
  switch (d) {
    case DesignMode::Minimal: return "minimal";
    case DesignMode::Daily:   return "daily";
  }
  return "minimal";
}

size_t formatDate(int day, int month, DateFormat fmt, char* out, size_t outSize) {
  if (!out || outSize < 6) return 0;
  const char sep = (fmt == DateFormat::DayMonthSlash || fmt == DateFormat::MonthDaySlash) ? '/'
                 : (fmt == DateFormat::DayMonthDash  || fmt == DateFormat::MonthDayDash)  ? '-'
                                                                                          : '.';
  const bool dayFirst = (fmt == DateFormat::DayMonthSlash ||
                         fmt == DateFormat::DayMonthDash ||
                         fmt == DateFormat::DayMonthDot);
  const int a = dayFirst ? day : month;
  const int b = dayFirst ? month : day;
  int n = snprintf(out, outSize, "%02d%c%02d", a, sep, b);
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

TasksConfig& TasksConfig::getInstance() {
  static TasksConfig instance;
  return instance;
}

bool TasksConfig::load() {
  // Reset to defaults before reading.
  provider = Provider::Todoist;
  todoistApiToken.clear();
  designMode = DesignMode::Minimal;
  dateFilter = DateFilter::Today;
  overdueFilter = OverdueFilter::Last7Days;
  dateFormat = DateFormat::DayMonthSlash;
  activityOrientation = GfxRenderer::Orientation::Portrait;
  snapshotOrientation = GfxRenderer::Orientation::Portrait;
  loaded = false;

  // First-launch: file doesn't exist. Write a stub so the user has
  // something to edit, then load defaults.
  if (!Storage.exists(kConfigPath)) {
    LOG_DBG("TASKS", "No config at %s — writing stub", kConfigPath);
    writeStubIfMissing();
    loaded = true;
    return true;
  }

  String json = Storage.readFile(kConfigPath);
  if (json.isEmpty()) {
    LOG_ERR("TASKS", "Config file empty/unreadable at %s", kConfigPath);
    return false;
  }

  JsonDocument doc;
  auto err = deserializeJson(doc, json);
  if (err) {
    LOG_ERR("TASKS", "JSON parse error: %s", err.c_str());
    return false;
  }

  provider = providerFromString(
      doc["provider"] | static_cast<const char*>(nullptr), Provider::Todoist);
  todoistApiToken = doc["todoistApiToken"] | std::string("");

  designMode = designModeFromString(
      doc["design_mode"] | static_cast<const char*>(nullptr), DesignMode::Minimal);
  dateFilter = dateFilterFromString(
      doc["date_filter"] | static_cast<const char*>(nullptr), DateFilter::Today);
  overdueFilter = overdueFilterFromString(
      doc["overdue_filter"] | static_cast<const char*>(nullptr), OverdueFilter::Last7Days);
  dateFormat = dateFormatFromString(
      doc["date_format"] | static_cast<const char*>(nullptr), DateFormat::DayMonthSlash);
  activityOrientation = orientationFromString(
      doc["activity_orientation"] | static_cast<const char*>(nullptr),
      GfxRenderer::Orientation::Portrait);
  snapshotOrientation = orientationFromString(
      doc["snapshot_orientation"] | static_cast<const char*>(nullptr),
      GfxRenderer::Orientation::Portrait);

  loaded = true;
  LOG_DBG("TASKS", "Config loaded (token=%s)", todoistApiToken.empty() ? "no" : "yes");
  return true;
}

bool TasksConfig::writeStubIfMissing() {
  if (Storage.exists(kConfigPath)) return true;
  Storage.mkdir("/.crosspoint");

  JsonDocument doc;
  doc["provider"] = providerToString(provider);
  doc["todoistApiToken"] = "";
  doc["design_mode"] = designModeToString(designMode);
  doc["date_filter"] = dateFilterToString(dateFilter);
  doc["overdue_filter"] = overdueFilterToString(overdueFilter);
  doc["date_format"] = dateFormatToString(dateFormat);
  doc["activity_orientation"] = orientationToString(activityOrientation);
  doc["snapshot_orientation"] = orientationToString(snapshotOrientation);

  String out;
  serializeJsonPretty(doc, out);
  bool ok = Storage.writeFile(kConfigPath, out);
  if (!ok) LOG_ERR("TASKS", "Failed to write stub config");
  return ok;
}

bool TasksConfig::persist() {
  Storage.mkdir("/.crosspoint");

  JsonDocument doc;
  doc["provider"] = providerToString(provider);
  doc["todoistApiToken"] = todoistApiToken;
  doc["design_mode"] = designModeToString(designMode);
  doc["date_filter"] = dateFilterToString(dateFilter);
  doc["overdue_filter"] = overdueFilterToString(overdueFilter);
  doc["date_format"] = dateFormatToString(dateFormat);
  doc["activity_orientation"] = orientationToString(activityOrientation);
  doc["snapshot_orientation"] = orientationToString(snapshotOrientation);

  String out;
  serializeJsonPretty(doc, out);
  bool ok = Storage.writeFile(kConfigPath, out);
  if (ok) {
    LOG_DBG("TASKS", "Config persisted");
  } else {
    LOG_ERR("TASKS", "Failed to persist config");
  }
  return ok;
}

// getActiveProvider() is defined in Phase 2 (providers/todoist/TodoistProvider.cpp).

bool TasksConfig::setProvider(Provider p) {
  if (p == provider) return true;
  provider = p;
  return persist();
}

bool TasksConfig::setDesignMode(DesignMode d) {
  if (d == designMode) return true;
  designMode = d;
  return persist();
}

bool TasksConfig::setDateFilter(DateFilter f) {
  if (f == dateFilter) return true;
  dateFilter = f;
  return persist();
}

bool TasksConfig::setOverdueFilter(OverdueFilter f) {
  if (f == overdueFilter) return true;
  overdueFilter = f;
  return persist();
}

bool TasksConfig::setDateFormat(DateFormat f) {
  if (f == dateFormat) return true;
  dateFormat = f;
  return persist();
}

bool TasksConfig::setActivityOrientation(GfxRenderer::Orientation o) {
  if (o == activityOrientation) return true;
  activityOrientation = o;
  return persist();
}

bool TasksConfig::setSnapshotOrientation(GfxRenderer::Orientation o) {
  if (o == snapshotOrientation) return true;
  snapshotOrientation = o;
  return persist();
}

bool TasksConfig::forget() {
  if (todoistApiToken.empty()) return true;
  todoistApiToken.clear();
  return persist();
}

}  // namespace tasks
