#include "FlashcardSrs.h"

#include <Logging.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>

// ESP-IDF provides getLocalTime()
#include <esp_sntp.h>
extern "C" bool getLocalTime(struct tm* info, uint32_t ms = 5000);

namespace {

// 2000-01-01 as Unix timestamp (UTC, no leap second)
static constexpr time_t EPOCH_2000 = 946684800L;
static constexpr uint16_t EASE_MIN = 130;
static constexpr uint16_t EASE_MAX = 400;

static uint16_t clampEase(int ease) {
    if (ease < EASE_MIN) return EASE_MIN;
    if (ease > EASE_MAX) return EASE_MAX;
    return static_cast<uint16_t>(ease);
}

}  // namespace

namespace FlashcardSrs {

uint32_t tmToDays(const struct tm& t) {
    // Make a mutable copy so mktime can normalise fields
    struct tm tmp = t;
    tmp.tm_hour = 12;
    tmp.tm_min = 0;
    tmp.tm_sec = 0;
    time_t unix = mktime(&tmp);
    if (unix < EPOCH_2000) return 0;
    return static_cast<uint32_t>((unix - EPOCH_2000) / 86400UL);
}

struct tm daysToTm(uint32_t days) {
    time_t unix = EPOCH_2000 + (time_t)days * 86400L;
    struct tm result = {};
    gmtime_r(&unix, &result);
    return result;
}

uint32_t parseDateStr(const char* str) {
    if (!str || str[0] == '\0') return 0;
    // Expected format: "DD/MM/YYYY"
    int d = 0, m = 0, y = 0;
    if (sscanf(str, "%d/%d/%d", &d, &m, &y) != 3) return 0;
    if (d < 1 || d > 31 || m < 1 || m > 12 || y < 2000 || y > 2100) return 0;
    struct tm t = {};
    t.tm_mday = d;
    t.tm_mon = m - 1;
    t.tm_year = y - 1900;
    return tmToDays(t);
}

void formatDateStr(uint32_t days, char* buf, size_t bufSize) {
    if (!buf || bufSize < 11) return;
    if (days == 0) {
        buf[0] = '\0';
        return;
    }
    struct tm t = daysToTm(days);
    snprintf(buf, bufSize, "%02d/%02d/%04d",
             t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
}

uint32_t getToday() {
    struct tm timeinfo = {};
    if (!getLocalTime(&timeinfo, 0)) {
        // Fallback to time() if NTP not synced
        time_t now = time(nullptr);
        if (now < EPOCH_2000) return 0;
        gmtime_r(&now, &timeinfo);
    }
    return tmToDays(timeinfo);
}

SrsState review(const SrsState& current, SrsRating rating, uint32_t today) {
    SrsState next = current;
    const bool isNew = (current.interval == 0);

    switch (rating) {
        case SrsRating::Again:
            next.interval = 0;
            next.ease = clampEase((int)current.ease - 20);
            // Due today — will be re-queued within the session
            next.dueDate = today;
            break;

        case SrsRating::Hard:
            if (isNew) {
                next.interval = 1;
            } else {
                uint32_t newInterval = (uint32_t)((float)current.interval * 1.2f);
                next.interval = (uint16_t)std::max((uint32_t)1, newInterval);
            }
            next.ease = clampEase((int)current.ease - 15);
            next.dueDate = today + next.interval;
            break;

        case SrsRating::Good:
            if (isNew) {
                next.interval = 1;
            } else {
                uint32_t newInterval = (uint32_t)((float)current.interval * current.ease / 100.0f);
                next.interval = (uint16_t)std::max((uint32_t)1, newInterval);
            }
            // ease unchanged for Good
            next.dueDate = today + next.interval;
            break;

        case SrsRating::Easy:
            if (isNew) {
                next.interval = 4;
            } else {
                uint32_t newInterval = (uint32_t)((float)current.interval * current.ease / 100.0f * 1.3f);
                next.interval = (uint16_t)std::max((uint32_t)1, newInterval);
            }
            next.ease = clampEase((int)current.ease + 15);
            next.dueDate = today + next.interval;
            break;
    }

    return next;
}

const char* previewInterval(const SrsState& current, SrsRating rating) {
    // Static buffer — not thread-safe but consistent with ESP-IDF single-core usage patterns
    static char buf[16];
    if (rating == SrsRating::Again) {
        snprintf(buf, sizeof(buf), "<10m");
        return buf;
    }
    // Use today=0 as relative offset; we only care about resulting interval
    uint32_t fakeToday = 0;
    SrsState next = review(current, rating, fakeToday);
    uint32_t days = next.interval;
    if (days == 0) {
        snprintf(buf, sizeof(buf), "<10m");
    } else if (days < 30) {
        snprintf(buf, sizeof(buf), "%ud", (unsigned)days);
    } else {
        uint32_t months = days / 30;
        snprintf(buf, sizeof(buf), "%um", (unsigned)months);
    }
    return buf;
}

}  // namespace FlashcardSrs
