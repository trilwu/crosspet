#include "TodoistFilter.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace tasks::todoist {

namespace {

// Format `today + daysAhead` as YYYY-MM-DD in the device's local timezone.
// Caller must have already verified the clock is set (via NTP).
std::string formatLocalDate(int daysAhead) {
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  tm.tm_mday += daysAhead;
  mktime(&tm);  // normalises across month/year boundaries
  char buf[11];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
  return std::string(buf);
}

// Days from today to the upcoming Monday (1..7). ISO week ends on Sunday,
// so the strict `due before:` cutoff for "this week" is next Monday. If
// today is Monday we want a full week ahead, not zero — 0 maps to 7.
int daysUntilNextMonday() {
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  int days = (1 - tm.tm_wday + 7) % 7;
  return days == 0 ? 7 : days;
}

// First day of next calendar month, YYYY-MM-DD.
std::string firstOfNextMonth() {
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  tm.tm_mon += 1;
  tm.tm_mday = 1;
  mktime(&tm);  // normalises December → January roll-over
  char buf[11];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
  return std::string(buf);
}

}  // namespace

std::string buildQuery(DateFilter dateF, OverdueFilter overdueF) {
  std::string date;
  switch (dateF) {
    case DateFilter::None:  break;
    case DateFilter::Today: date = "today"; break;
    case DateFilter::ThisWeek:
      date = "due after: yesterday & due before: " +
             formatLocalDate(daysUntilNextMonday());
      break;
    case DateFilter::ThisMonth:
      date = "due after: yesterday & due before: " + firstOfNextMonth();
      break;
  }

  std::string overdue;
  switch (overdueF) {
    case OverdueFilter::None:                                                break;
    case OverdueFilter::Last7Days: overdue = "overdue & due after: -7 days"; break;
    case OverdueFilter::All:       overdue = "overdue";                      break;
  }

  if (date.empty() && overdue.empty()) return "today";  // degenerate fallback
  if (date.empty())    return overdue;
  if (overdue.empty()) return date;
  return "(" + date + ") | (" + overdue + ")";
}

std::string urlEncode(const std::string& s) {
  std::string out;
  out.reserve(s.size() * 3);
  static const char hex[] = "0123456789ABCDEF";
  for (char c : s) {
    unsigned char uc = static_cast<unsigned char>(c);
    if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') ||
        (uc >= '0' && uc <= '9') ||
        uc == '-' || uc == '_' || uc == '.' || uc == '~') {
      out.push_back(c);
    } else {
      out.push_back('%');
      out.push_back(hex[uc >> 4]);
      out.push_back(hex[uc & 0x0F]);
    }
  }
  return out;
}

}  // namespace tasks::todoist
