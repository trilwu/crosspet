#pragma once
#include <cstdint>

// Tracks reading session time and book progress for the Reading Stats sleep screen.
// Stats are accumulated each time the reader activity exits (including on sleep entry).
class ReadingStats {
  static ReadingStats instance;
  unsigned long sessionStartMs = 0;
  bool sessionActive = false;

 public:
  uint32_t totalReadSeconds = 0;    // All-time cumulative reading time
  uint32_t todayReadSeconds = 0;    // Current-day reading time (resets at midnight)
  int16_t  lastReadYear = 0;        // tm_year when last session was recorded
  int16_t  lastReadDayOfYear = -1;  // tm_yday when last session was recorded
  char     lastBookTitle[64] = {};  // Title of the last book read
  uint8_t  lastBookProgress = 0;    // 0-100% progress in last book

  static ReadingStats& getInstance() { return instance; }

  // Called when entering the reader activity; marks session start time.
  void startSession();

  // Called when exiting the reader activity; accumulates elapsed time and saves.
  void endSession(const char* title, uint8_t progress);

  bool saveToFile() const;
  bool loadFromFile();
};

#define READ_STATS ReadingStats::getInstance()
