#include "BookmarkStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

BookmarkStore::BookmarkStore(const std::string& cachePath) : filePath(cachePath + "/bookmarks.bin") {}

bool BookmarkStore::load() {
  bookmarks.clear();

  FsFile file;
  if (!Storage.openFileForRead("BKM", filePath.c_str(), file)) {
    return false;  // No bookmarks file yet — not an error
  }

  uint8_t version = 0;
  if (file.read(&version, sizeof(version)) != sizeof(version) || version != FILE_VERSION) {
    file.close();
    return false;
  }

  uint16_t count = 0;
  if (file.read(&count, sizeof(count)) != sizeof(count)) {
    file.close();
    return false;
  }

  if (count > MAX_BOOKMARKS) count = MAX_BOOKMARKS;
  bookmarks.reserve(count);

  for (uint16_t i = 0; i < count; i++) {
    Bookmark bm;
    // Read fields individually to avoid alignment issues on RISC-V
    if (file.read(&bm.spineIndex, sizeof(bm.spineIndex)) != sizeof(bm.spineIndex)) break;
    if (file.read(&bm.pageIndex, sizeof(bm.pageIndex)) != sizeof(bm.pageIndex)) break;
    if (file.read(&bm.timestamp, sizeof(bm.timestamp)) != sizeof(bm.timestamp)) break;
    if (file.read(bm.snippet, sizeof(bm.snippet)) != sizeof(bm.snippet)) break;
    bookmarks.push_back(bm);
  }

  file.close();
  std::sort(bookmarks.begin(), bookmarks.end());
  LOG_DBG("BKM", "Loaded %zu bookmarks from %s", bookmarks.size(), filePath.c_str());
  return true;
}

bool BookmarkStore::save() const {
  FsFile file;
  if (!Storage.openFileForWrite("BKM", filePath.c_str(), file)) {
    LOG_ERR("BKM", "Failed to open bookmarks file for writing");
    return false;
  }

  uint8_t version = FILE_VERSION;
  file.write(&version, sizeof(version));

  uint16_t count = static_cast<uint16_t>(bookmarks.size());
  file.write(reinterpret_cast<const uint8_t*>(&count), sizeof(count));

  for (const auto& bm : bookmarks) {
    file.write(reinterpret_cast<const uint8_t*>(&bm.spineIndex), sizeof(bm.spineIndex));
    file.write(reinterpret_cast<const uint8_t*>(&bm.pageIndex), sizeof(bm.pageIndex));
    file.write(reinterpret_cast<const uint8_t*>(&bm.timestamp), sizeof(bm.timestamp));
    file.write(reinterpret_cast<const uint8_t*>(bm.snippet), sizeof(bm.snippet));
  }

  file.close();
  return true;
}

bool BookmarkStore::addBookmark(uint16_t spine, uint16_t page, const char* snippet) {
  if (bookmarks.size() >= MAX_BOOKMARKS) {
    LOG_ERR("BKM", "Max bookmarks reached (%zu)", MAX_BOOKMARKS);
    return false;
  }

  // Check if already bookmarked
  if (isBookmarked(spine, page)) return false;

  Bookmark bm;
  bm.spineIndex = spine;
  bm.pageIndex = page;
  bm.timestamp = 0;  // Could use millis()/1000 if time is available
  memset(bm.snippet, 0, sizeof(bm.snippet));
  if (snippet) {
    strncpy(bm.snippet, snippet, sizeof(bm.snippet) - 1);
  }

  // Insert sorted
  auto it = std::lower_bound(bookmarks.begin(), bookmarks.end(), bm);
  bookmarks.insert(it, bm);
  save();

  LOG_DBG("BKM", "Added bookmark: spine=%u page=%u", spine, page);
  return true;
}

bool BookmarkStore::removeBookmark(uint16_t spine, uint16_t page) {
  auto it = std::find_if(bookmarks.begin(), bookmarks.end(),
                         [spine, page](const Bookmark& bm) { return bm.matches(spine, page); });
  if (it == bookmarks.end()) return false;

  bookmarks.erase(it);
  save();
  LOG_DBG("BKM", "Removed bookmark: spine=%u page=%u", spine, page);
  return true;
}

bool BookmarkStore::isBookmarked(uint16_t spine, uint16_t page) const {
  Bookmark key;
  key.spineIndex = spine;
  key.pageIndex = page;
  auto it = std::lower_bound(bookmarks.begin(), bookmarks.end(), key);
  return it != bookmarks.end() && it->matches(spine, page);
}
