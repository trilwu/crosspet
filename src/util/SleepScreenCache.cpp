#include "util/SleepScreenCache.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>

#include "CrossPointSettings.h"

uint32_t SleepScreenCache::hashKey(const std::string& sourcePath) {
  // FNV-1a 32-bit hash of: sourcePath + coverFilter + coverMode
  uint32_t hash = 2166136261u;
  for (char c : sourcePath) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 16777619u;
  }
  // Mix in settings that affect the rendered output
  hash ^= static_cast<uint8_t>(SETTINGS.sleepScreenCoverFilter);
  hash *= 16777619u;
  hash ^= static_cast<uint8_t>(SETTINGS.sleepScreenCoverMode);
  hash *= 16777619u;
  return hash;
}

std::string SleepScreenCache::getCachePath(const std::string& sourcePath) {
  char filename[48];
  snprintf(filename, sizeof(filename), "%s/%08x.raw", CACHE_DIR, hashKey(sourcePath));
  return std::string(filename);
}

bool SleepScreenCache::load(GfxRenderer& renderer, const std::string& sourcePath) {
  const auto path = getCachePath(sourcePath);
  FsFile file;
  if (!Storage.openFileForRead("SLC", path, file)) {
    return false;
  }

  // Validate file size matches framebuffer
  if (file.fileSize() != HalDisplay::BUFFER_SIZE) {
    LOG_ERR("SLC", "Cache file size mismatch: %u vs %u", (unsigned)file.fileSize(),
            (unsigned)HalDisplay::BUFFER_SIZE);
    file.close();
    Storage.remove(path.c_str());
    return false;
  }

  uint8_t* fb = renderer.getFrameBuffer();
  const int bytesRead = file.read(fb, HalDisplay::BUFFER_SIZE);
  file.close();

  if (bytesRead != static_cast<int>(HalDisplay::BUFFER_SIZE)) {
    LOG_ERR("SLC", "Cache read incomplete: %d / %u", bytesRead, (unsigned)HalDisplay::BUFFER_SIZE);
    return false;
  }

  LOG_DBG("SLC", "Cache hit: %s", path.c_str());
  return true;
}

void SleepScreenCache::save(const GfxRenderer& renderer, const std::string& sourcePath) {
  Storage.mkdir(CACHE_DIR);

  const auto path = getCachePath(sourcePath);
  FsFile file;
  if (!Storage.openFileForWrite("SLC", path, file)) {
    LOG_ERR("SLC", "Failed to open cache for write: %s", path.c_str());
    return;
  }

  const uint8_t* fb = renderer.getFrameBuffer();
  const size_t written = file.write(fb, HalDisplay::BUFFER_SIZE);
  file.close();

  if (written != HalDisplay::BUFFER_SIZE) {
    LOG_ERR("SLC", "Cache write incomplete: %u / %u", (unsigned)written, (unsigned)HalDisplay::BUFFER_SIZE);
    Storage.remove(path.c_str());
    return;
  }

  LOG_DBG("SLC", "Cache saved: %s", path.c_str());
}

int SleepScreenCache::invalidateAll() {
  auto dir = Storage.open(CACHE_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return 0;
  }

  int count = 0;
  char name[64];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    file.getName(name, sizeof(name));
    file.close();
    auto fullPath = std::string(CACHE_DIR) + "/" + name;
    if (Storage.remove(fullPath.c_str())) {
      count++;
    }
  }
  dir.close();

  LOG_DBG("SLC", "Invalidated %d cache files", count);
  return count;
}
