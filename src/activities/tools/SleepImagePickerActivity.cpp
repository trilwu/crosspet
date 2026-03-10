#include "SleepImagePickerActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include "CrossPointSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

// Maximum number of .bmp files to list (RAM constraint).
static constexpr int MAX_FILES = 30;

void SleepImagePickerActivity::loadFiles() {
  fileList.clear();
  sleepDir.clear();

  // Prefer /.sleep, fall back to /sleep
  auto dir = Storage.open("/.sleep");
  if (dir && dir.isDirectory()) {
    sleepDir = "/.sleep";
  } else {
    if (dir) dir.close();
    dir = Storage.open("/sleep");
    if (dir && dir.isDirectory()) {
      sleepDir = "/sleep";
    }
  }

  if (sleepDir.empty()) {
    if (dir) dir.close();
    return;
  }

  char name[256];
  while (static_cast<int>(fileList.size()) < MAX_FILES) {
    auto file = dir.openNextFile();
    if (!file) break;
    file.getName(name, sizeof(name));
    if (name[0] != '.' && !file.isDirectory()) {
      std::string_view sv{name};
      if (FsHelpers::hasBmpExtension(sv)) {
        fileList.emplace_back(name);
      }
    }
    file.close();
  }

  dir.close();
  LOG_DBG("SIP", "Found %d bmp files in %s", (int)fileList.size(), sleepDir.c_str());
}

void SleepImagePickerActivity::onEnter() {
  Activity::onEnter();
  loadFiles();
  selectorIndex = 0;

  // Pre-select the item that matches the current setting
  if (SETTINGS.sleepImagePath[0] != '\0') {
    // Extract just the filename from the stored path
    const char* slash = strrchr(SETTINGS.sleepImagePath, '/');
    const char* storedFile = (slash != nullptr) ? (slash + 1) : SETTINGS.sleepImagePath;
    for (int i = 0; i < static_cast<int>(fileList.size()); ++i) {
      if (fileList[i] == storedFile) {
        selectorIndex = i + 1;  // +1 because index 0 = "Any (random)"
        break;
      }
    }
  }

  requestUpdate();
}

void SleepImagePickerActivity::onExit() {
  fileList.clear();
  fileList.shrink_to_fit();
  Activity::onExit();
}

void SleepImagePickerActivity::loop() {
  buttonNavigator.onNext([this] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, totalItems());
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, totalItems());
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex == 0) {
      // "Any (random)" — clear the stored path
      SETTINGS.sleepImagePath[0] = '\0';
    } else {
      const std::string& filename = fileList[selectorIndex - 1];
      const std::string fullPath = sleepDir + "/" + filename;
      strncpy(SETTINGS.sleepImagePath, fullPath.c_str(), sizeof(SETTINGS.sleepImagePath) - 1);
      SETTINGS.sleepImagePath[sizeof(SETTINGS.sleepImagePath) - 1] = '\0';
      LOG_DBG("SIP", "Selected sleep image: %s", SETTINGS.sleepImagePath);
    }
    SETTINGS.saveToFile();
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void SleepImagePickerActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_SLEEP_IMAGE_PICKER));

  const int menuTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int menuHeight = pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (fileList.empty()) {
    // No files: show hint
    const int midY = menuTop + menuHeight / 2 - renderer.getLineHeight(UI_10_FONT_ID);
    renderer.drawCenteredText(UI_10_FONT_ID, midY, tr(STR_NO_BMP_FILES));
    renderer.drawCenteredText(SMALL_FONT_ID, midY + renderer.getLineHeight(UI_10_FONT_ID) + 6,
                              tr(STR_PLACE_BMP_IN_SLEEP));
  } else {
    // Build item labels: index 0 = "Any (random)", then filenames
    const int count = totalItems();
    GUI.drawList(renderer, Rect{0, menuTop, pageWidth, menuHeight}, count, selectorIndex,
                 [this](int idx) -> std::string {
                   if (idx == 0) return tr(STR_CUSTOM);  // "Custom" = random from folder
                   return fileList[idx - 1];
                 });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), fileList.empty() ? "" : tr(STR_SELECT),
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
