#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// File entry with metadata for sorting by date/size/favorites
struct FileEntry {
  std::string name;
  uint32_t modTime = 0;
  uint32_t fileSize = 0;
  bool favorite = false;  // pre-computed during loadFiles()
};

class FileBrowserActivity final : public Activity {
 private:
  // Deletion
  void clearFileMetadata(const std::string& fullPath);

  ButtonNavigator buttonNavigator;

  size_t selectorIndex = 0;

  // Files state
  std::string basepath = "/";
  std::vector<FileEntry> files;

  // Data loading
  void loadFiles();
  size_t findEntry(const std::string& name) const;

  // Build full path for a file entry
  std::string buildFullPath(const std::string& filename) const;

 public:
  explicit FileBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string initialPath = "/")
      : Activity("FileBrowser", renderer, mappedInput), basepath(initialPath.empty() ? "/" : std::move(initialPath)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
