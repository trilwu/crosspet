#pragma once
#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// Simple file picker that shows folders + .bin firmware files on the SD card.
// Returns FilePathResult with the selected firmware path, or isCancelled if the
// user backs out of the SD-card root. Modeled on FlashcardFilePickerActivity but
// restricted to firmware images so the SD-card firmware update flow can reuse the
// standard list UI without pulling in the book-oriented FileBrowserActivity.
class FirmwarePickerActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  std::string basepath;
  std::vector<std::string> files;
  int selectorIndex = 0;

  void loadFiles();

  static bool hasBinExtension(const std::string& name) {
    if (name.size() < 5) return false;
    auto ext = name.substr(name.size() - 4);
    return ext == ".bin" || ext == ".BIN";
  }

 public:
  explicit FirmwarePickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FirmwarePicker", renderer, mappedInput), basepath("/") {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
