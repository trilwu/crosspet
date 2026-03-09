#pragma once

#include <string>

namespace StringUtils {

/**
 * Format a reading duration in seconds into a human-readable string.
 * Results: "< 1 min", "X min", "X hr Y min", "X days Y hr"
 */
void formatReadingDuration(char* buf, size_t size, uint32_t secs);

/**
 * Sanitize a string for use as a filename.
 * Replaces invalid characters with underscores, trims spaces/dots,
 * and limits length to maxBytes bytes.
 */
std::string sanitizeFilename(const std::string& name, size_t maxBytes = 100);

}  // namespace StringUtils
