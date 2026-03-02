#include "ReadingStats.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstring>
#include <ctime>

namespace {
constexpr uint8_t STATS_FILE_VERSION = 1;
constexpr char STATS_FILE[] = "/.crosspoint/reading_stats.bin";
}  // namespace

ReadingStats ReadingStats::instance;

void ReadingStats::startSession() {
  sessionStartMs = millis();
  sessionActive = true;
}

void ReadingStats::endSession(const char* title, uint8_t progress) {
  if (!sessionActive) return;

  // Check for day rollover; reset today's counter if the date changed
  time_t now = time(nullptr);
  struct tm timeinfo = {};
  localtime_r(&now, &timeinfo);
  const int16_t curYear = static_cast<int16_t>(timeinfo.tm_year);
  const int16_t curDay  = static_cast<int16_t>(timeinfo.tm_yday);
  if (curDay != lastReadDayOfYear || curYear != lastReadYear) {
    todayReadSeconds = 0;
    lastReadYear      = curYear;
    lastReadDayOfYear = curDay;
  }

  // Accumulate session duration; cap at 24 h to guard against millis() wrap
  const unsigned long elapsed = millis() - sessionStartMs;
  uint32_t elapsedSecs = static_cast<uint32_t>(elapsed / 1000UL);
  constexpr uint32_t MAX_SESSION_SECS = 86400;
  if (elapsedSecs > MAX_SESSION_SECS) elapsedSecs = MAX_SESSION_SECS;

  todayReadSeconds += elapsedSecs;
  totalReadSeconds += elapsedSecs;

  if (title && title[0] != '\0') {
    strncpy(lastBookTitle, title, sizeof(lastBookTitle) - 1);
    lastBookTitle[sizeof(lastBookTitle) - 1] = '\0';
  }
  lastBookProgress = progress;
  sessionActive = false;
  saveToFile();
}

bool ReadingStats::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  FsFile file;
  if (!Storage.openFileForWrite("RST", STATS_FILE, file)) {
    LOG_ERR("RST", "Failed to open reading_stats.bin for write");
    return false;
  }
  const uint8_t version = STATS_FILE_VERSION;
  serialization::writePod(file, version);
  serialization::writePod(file, totalReadSeconds);
  serialization::writePod(file, todayReadSeconds);
  serialization::writePod(file, lastReadYear);
  serialization::writePod(file, lastReadDayOfYear);
  serialization::writePod(file, lastBookProgress);
  file.write(reinterpret_cast<const uint8_t*>(lastBookTitle), sizeof(lastBookTitle));
  file.close();
  return true;
}

bool ReadingStats::loadFromFile() {
  FsFile file;
  if (!Storage.openFileForRead("RST", STATS_FILE, file)) {
    return false;
  }
  uint8_t version;
  serialization::readPod(file, version);
  if (version != STATS_FILE_VERSION) {
    LOG_ERR("RST", "Unknown reading_stats.bin version %u", version);
    file.close();
    return false;
  }
  serialization::readPod(file, totalReadSeconds);
  serialization::readPod(file, todayReadSeconds);
  serialization::readPod(file, lastReadYear);
  serialization::readPod(file, lastReadDayOfYear);
  serialization::readPod(file, lastBookProgress);
  file.read(reinterpret_cast<uint8_t*>(lastBookTitle), sizeof(lastBookTitle));
  lastBookTitle[sizeof(lastBookTitle) - 1] = '\0';
  file.close();
  return true;
}
