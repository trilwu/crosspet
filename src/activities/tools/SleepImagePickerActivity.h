#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Activity that lists .bmp files in /sleep/ (or /.sleep/) and lets the user
// select one as the fixed custom sleep-screen image.
// Selecting an image saves the full path to SETTINGS.sleepImagePath.
// An "Any (random)" option at the top restores default random behaviour.
class SleepImagePickerActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  std::vector<std::string> fileList;  // filenames only (no directory prefix)
  std::string sleepDir;               // resolved dir: "/.sleep" or "/sleep"
  int selectorIndex = 0;

  // Load .bmp filenames from the sleep directory into fileList.
  void loadFiles();

  // Full items count including the "Any (random)" header item.
  int totalItems() const { return static_cast<int>(fileList.size()) + 1; }

 public:
  explicit SleepImagePickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SleepImagePicker", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
