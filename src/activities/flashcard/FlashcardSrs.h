#pragma once
#include <cstdint>
#include <ctime>

// SRS state stored per card. Compact representation using days-since-2000 epoch.
struct SrsState {
    uint16_t interval = 0;  // days until next review (0 = new card)
    uint16_t ease = 250;    // ease factor * 100 (250 = 2.5, range [130, 400])
    uint32_t dueDate = 0;   // days since 2000-01-01 (0 = new/unscheduled)
};

enum class SrsRating : uint8_t { Again = 0, Hard = 1, Good = 2, Easy = 3 };

namespace FlashcardSrs {

// Apply SM-2 algorithm to compute next SrsState after a review.
SrsState review(const SrsState& current, SrsRating rating, uint32_t today);

// Convert struct tm to compact days-since-2000 representation.
uint32_t tmToDays(const struct tm& t);

// Convert compact days-since-2000 to struct tm.
struct tm daysToTm(uint32_t days);

// Parse "DD/MM/YYYY" string to days-since-2000. Returns 0 if empty or invalid.
uint32_t parseDateStr(const char* str);

// Format days-since-2000 to "DD/MM/YYYY". Writes to caller-provided buffer.
void formatDateStr(uint32_t days, char* buf, size_t bufSize);

// Get today as days-since-2000 using ESP-IDF getLocalTime().
uint32_t getToday();

// Preview the resulting interval string for a given rating without modifying state.
// Returns a static buffer — e.g. "<10m", "1d", "3d", "1m".
const char* previewInterval(const SrsState& current, SrsRating rating);

}  // namespace FlashcardSrs
