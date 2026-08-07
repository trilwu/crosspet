#pragma once

#include <cctype>
#include <cstdint>
#include <string>

namespace StringUtils {

/**
 * Format a reading duration in seconds into a human-readable string.
 * Results: "< 1 min", "X min", "X hr Y min", "X days Y hr"
 */
void formatReadingDuration(char* buf, size_t size, uint32_t secs);

/**
 * Case-insensitive ASCII strcmp. Returns <0, 0, or >0 like strcmp, comparing
 * each byte by its lowercased value.
 *
 * Used wherever data is sorted case-insensitively: StarDict indexes (including
 * wiktionary-derived dictionaries) and the on-disk dictionary folder list.
 * Plain strcmp would land a binary search on the wrong page for any word whose
 * alphabetic neighbourhood contains mixed-case boundaries.
 *
 * Inline (header) because Dictionary's binary search calls it per comparison
 * step; a cross-TU call here would defeat inlining on a hot path.
 */
inline int asciiCaseCmp(const char* a, const char* b) {
  while (*a && *b) {
    int diff = std::tolower(static_cast<unsigned char>(*a)) - std::tolower(static_cast<unsigned char>(*b));
    if (diff != 0) return diff;
    ++a;
    ++b;
  }
  return std::tolower(static_cast<unsigned char>(*a)) - std::tolower(static_cast<unsigned char>(*b));
}

/**
 * Sanitize a string for use as a filename.
 * Replaces invalid characters with underscores, trims spaces/dots,
 * and limits length to maxBytes bytes.
 */
std::string sanitizeFilename(const std::string& name, size_t maxBytes = 100);

}  // namespace StringUtils
