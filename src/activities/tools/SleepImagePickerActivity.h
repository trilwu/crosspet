#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// All-in-one sleep screen configuration: mode selection + image picker + overlay slideshow.
// Replaces the old 2-step flow (Settings→mode, then Apps→image).
class SleepImagePickerActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  std::vector<std::string> fileList;  // image filenames (no directory prefix)
  std::string sleepDir;               // resolved: "/.sleep" or "/sleep"
  int selectorIndex = 0;
  bool slideshowActive = false;
  int slideshowIndex = 0;

  // Menu structure:
  // [0] Sleep mode selector (cycles on confirm)
  // [1] Cover mode: Fit/Crop (only when mode uses covers)
  // [2] Cover filter: None/Contrast/Inverted (only when mode uses covers)
  // [3..N] Image files (only when mode uses images)
  // [N+1] Clear cache (always last)
  static constexpr int MODE_ITEM = 0;

  void loadFiles();
  bool modeUsesImages() const;
  bool modeUsesCovers() const;
  int coverOptionCount() const;
  int totalItems() const;
  int firstFileIndex() const { return 1 + coverOptionCount(); }
  int cacheItemIndex() const { return totalItems() - 1; }
  bool isFileItem(int idx) const { return modeUsesImages() && idx >= firstFileIndex() && idx < cacheItemIndex(); }
  bool isCoverModeItem(int idx) const { return modeUsesCovers() && idx == 1; }
  bool isCoverFilterItem(int idx) const { return modeUsesCovers() && idx == 2; }

  bool isCurrentImageSelection(int idx) const;
  void cycleSleepMode();
  void cycleCoverMode();
  void cycleCoverFilter();
  void saveImageSelection();
  void clearCache();

  // Rendering helpers
  void renderList(int pageWidth, int pageHeight);
  const char* currentModeName() const;
  const char* modeDescription() const;

  // Overlay slideshow
  void renderSlideshow();
  bool renderBookPage();
  bool drawOverlayImage(const std::string& path);

 public:
  explicit SleepImagePickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SleepImagePicker", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
