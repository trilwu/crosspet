#pragma once
#include <cstdint>
#include <string>

class GfxRenderer;

/// Cache rendered BW framebuffer to SD card as `.raw` files.
/// On cache hit, load directly into framebuffer (~50ms vs ~1-3s for BMP render).
/// Only caches BW (1-bit) renders; greyscale is skipped.
class SleepScreenCache {
 public:
  /// Try to load a cached raw framebuffer for the given source BMP.
  /// Returns true if cache hit and framebuffer is populated.
  static bool load(GfxRenderer& renderer, const std::string& sourcePath);

  /// Save the current framebuffer to cache for the given source BMP.
  static void save(const GfxRenderer& renderer, const std::string& sourcePath);

  /// Delete all cached .raw files.
  /// Returns the number of files removed.
  static int invalidateAll();

 private:
  static constexpr const char* CACHE_DIR = "/.crosspoint/sleep_cache";

  /// FNV-1a hash of the cache key (source path + cover filter + cover mode).
  static uint32_t hashKey(const std::string& sourcePath);

  /// Build the full cache file path for a given source BMP.
  static std::string getCachePath(const std::string& sourcePath);
};
