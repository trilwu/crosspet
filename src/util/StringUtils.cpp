#include "StringUtils.h"

#include <Utf8.h>

#include <cstdio>
#include <cstdint>
#include <cstring>


namespace StringUtils {

void formatReadingDuration(char* buf, size_t size, uint32_t secs) {
  const uint32_t mins  = secs / 60;
  const uint32_t hours = mins / 60;
  const uint32_t days  = hours / 24;
  if (secs < 60) {
    snprintf(buf, size, "< 1 min");
  } else if (hours == 0) {
    snprintf(buf, size, "%lu min", (unsigned long)mins);
  } else if (days == 0) {
    snprintf(buf, size, "%lu hr %lu min", (unsigned long)hours, (unsigned long)(mins % 60));
  } else {
    snprintf(buf, size, "%lu days %lu hr", (unsigned long)days, (unsigned long)(hours % 24));
  }
}

std::string sanitizeFilename(const std::string& name, size_t maxBytes) {
  std::string result;
  result.reserve(std::min(name.size(), maxBytes));

  const auto* text = reinterpret_cast<const unsigned char*>(name.c_str());

  // Skip leading spaces and dots so they don't consume the byte budget
  while (*text == ' ' || *text == '.') {
    text++;
  }

  // Process full UTF-8 codepoints to avoid trimming in the middle of a multibyte sequence
  while (*text != 0) {
    const auto* cpStart = text;
    uint32_t cp = utf8NextCodepoint(&text);

    if (cp == '/' || cp == '\\' || cp == ':' || cp == '*' || cp == '?' || cp == '"' || cp == '<' || cp == '>' ||
        cp == '|') {
      // Replace illegal and control characters with '_'
      if (result.length() + 1 > maxBytes) break;
      result += '_';
    } else if (cp >= 128 || (cp >= 32 && cp < 127)) {
      const size_t cpBytes = text - cpStart;
      if (result.length() + cpBytes > maxBytes) break;
      result.append(reinterpret_cast<const char*>(cpStart), cpBytes);
    }
  }

  // Trim trailing spaces and dots
  size_t end = result.find_last_not_of(" .");
  if (end != std::string::npos) {
    result.resize(end + 1);
  } else {
    result.clear();
  }

  return result.empty() ? "book" : result;
}

}  // namespace StringUtils
