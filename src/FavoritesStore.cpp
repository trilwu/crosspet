#include "FavoritesStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <functional>

FavoritesStore FavoritesStore::instance;

bool FavoritesStore::load() {
  hashes.clear();

  FsFile file;
  if (!Storage.openFileForRead("FAV", FILE_PATH, file)) {
    return false;
  }

  uint16_t count = 0;
  if (file.read(&count, sizeof(count)) != sizeof(count)) {
    file.close();
    return false;
  }

  if (count > MAX_FAVORITES) count = MAX_FAVORITES;
  hashes.reserve(count);

  for (uint16_t i = 0; i < count; i++) {
    uint32_t hash;
    if (file.read(&hash, sizeof(hash)) != sizeof(hash)) break;
    hashes.push_back(hash);
  }

  file.close();
  std::sort(hashes.begin(), hashes.end());
  LOG_DBG("FAV", "Loaded %zu favorites", hashes.size());
  return true;
}

bool FavoritesStore::save() const {
  Storage.mkdir("/.crosspoint");

  FsFile file;
  if (!Storage.openFileForWrite("FAV", FILE_PATH, file)) {
    LOG_ERR("FAV", "Failed to open favorites file for writing");
    return false;
  }

  uint16_t count = static_cast<uint16_t>(hashes.size());
  file.write(reinterpret_cast<const uint8_t*>(&count), sizeof(count));
  for (const auto& h : hashes) {
    file.write(reinterpret_cast<const uint8_t*>(&h), sizeof(h));
  }

  file.close();
  return true;
}

bool FavoritesStore::isFavorite(const std::string& path) const {
  uint32_t hash = static_cast<uint32_t>(std::hash<std::string>{}(path));
  return std::binary_search(hashes.begin(), hashes.end(), hash);
}

bool FavoritesStore::toggleFavorite(const std::string& path) {
  uint32_t hash = static_cast<uint32_t>(std::hash<std::string>{}(path));
  auto it = std::lower_bound(hashes.begin(), hashes.end(), hash);

  if (it != hashes.end() && *it == hash) {
    // Remove
    hashes.erase(it);
    save();
    LOG_DBG("FAV", "Removed favorite: %s", path.c_str());
    return false;
  }

  // Add
  if (hashes.size() >= MAX_FAVORITES) {
    LOG_ERR("FAV", "Max favorites reached (%zu)", MAX_FAVORITES);
    return false;
  }
  hashes.insert(it, hash);
  save();
  LOG_DBG("FAV", "Added favorite: %s", path.c_str());
  return true;
}
