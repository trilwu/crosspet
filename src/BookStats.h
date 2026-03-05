#pragma once
#include <cstdint>
#include <map>
#include <string>

// Per-book reading statistics stored in JSON on SD card.
// Tracks cumulative reading time, session count, and progress for each book.
class BookStats {
  static BookStats instance;

 public:
  struct BookEntry {
    char title[64];
    uint32_t totalSeconds;       // cumulative reading time for this book
    uint32_t lastReadTimestamp;  // epoch seconds of last session
    uint8_t progress;            // 0-100%
    uint16_t sessions;           // number of reading sessions
  };

  static BookStats& getInstance() { return instance; }

  // Update stats for a specific book after a reading session
  void updateBook(const char* path, const char* title, uint32_t sessionSeconds, uint8_t progress);

  // Get stats for a specific book (nullptr if not found)
  const BookEntry* getBook(const char* path) const;

  // Get all entries (for display)
  const std::map<std::string, BookEntry>& getBooks() const { return books; }

  bool saveToFile();
  bool loadFromFile();

 private:
  std::map<std::string, BookEntry> books;
  static constexpr int MAX_BOOKS = 50;  // LRU cap

  void evictOldest();  // Remove least recently read when over MAX_BOOKS
};

#define BOOK_STATS BookStats::getInstance()
