#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Stores favorite file paths as hashes for quick lookup.
// Binary file: [uint16_t count][uint32_t hash1][hash2]...
class FavoritesStore {
  static FavoritesStore instance;
  std::vector<uint32_t> hashes;  // sorted for binary_search

  static constexpr size_t MAX_FAVORITES = 200;
  static constexpr char FILE_PATH[] = "/.crosspoint/favorites.bin";

 public:
  ~FavoritesStore() = default;

  static FavoritesStore& getInstance() { return instance; }

  bool load();
  bool save() const;
  bool isFavorite(const std::string& path) const;
  // Returns true if added, false if removed
  bool toggleFavorite(const std::string& path);
};

#define FAVORITES FavoritesStore::getInstance()
