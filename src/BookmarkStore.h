#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Per-book bookmark stored in .crosspoint/epub_<hash>/bookmarks.bin
struct Bookmark {
  uint16_t spineIndex;
  uint16_t pageIndex;
  uint32_t timestamp;  // seconds since epoch (approximate)
  char snippet[64];    // first ~60 chars of bookmarked page text

  // Comparison for sorted order (spine, then page)
  bool operator<(const Bookmark& other) const {
    if (spineIndex != other.spineIndex) return spineIndex < other.spineIndex;
    return pageIndex < other.pageIndex;
  }
  bool matches(uint16_t spine, uint16_t page) const { return spineIndex == spine && pageIndex == page; }
};

// Manages bookmarks for a single book (not a singleton — one per open book)
class BookmarkStore {
  std::vector<Bookmark> bookmarks;
  std::string filePath;

  static constexpr uint8_t FILE_VERSION = 1;
  static constexpr size_t MAX_BOOKMARKS = 50;

 public:
  explicit BookmarkStore(const std::string& cachePath);

  bool load();
  bool save() const;
  bool addBookmark(uint16_t spine, uint16_t page, const char* snippet);
  bool removeBookmark(uint16_t spine, uint16_t page);
  bool isBookmarked(uint16_t spine, uint16_t page) const;
  const std::vector<Bookmark>& getBookmarks() const { return bookmarks; }
  size_t count() const { return bookmarks.size(); }
};
