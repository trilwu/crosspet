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
  previewMode = false;

  // Pre-select the item that matches the current setting
  if (SETTINGS.sleepImagePath[0] != '\0') {
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

void SleepImagePickerActivity::saveSelection() {
  if (selectorIndex == 0) {
    SETTINGS.sleepImagePath[0] = '\0';
  } else {
    const std::string& filename = fileList[selectorIndex - 1];
    const std::string fullPath = sleepDir + "/" + filename;
    strncpy(SETTINGS.sleepImagePath, fullPath.c_str(), sizeof(SETTINGS.sleepImagePath) - 1);
    SETTINGS.sleepImagePath[sizeof(SETTINGS.sleepImagePath) - 1] = '\0';
    LOG_DBG("SIP", "Selected sleep image: %s", SETTINGS.sleepImagePath);
  }
  SETTINGS.saveToFile();

  // Show guidance if sleep mode is not already set to show custom images
  const auto mode = SETTINGS.sleepScreen;
  const bool customModeActive =
      (mode == CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM ||
       mode == CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM);

  if (!customModeActive && selectorIndex != 0) {
    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - renderer.getLineHeight(UI_10_FONT_ID),
                              tr(STR_SLEEP_IMAGE_SELECTED));
    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 8, tr(STR_SLEEP_MODE_HINT));
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    delay(2500);
  }
}

void SleepImagePickerActivity::loop() {
  if (previewMode) {
    // In preview: Confirm saves, Back cancels back to list
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      saveSelection();
      previewMode = false;
      finish();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      previewMode = false;
      requestUpdate();
    }
    return;
  }

  // List view navigation
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
      // "Any (random)" — no preview needed, save immediately
      saveSelection();
      finish();
      return;
    }
    // Enter preview mode for a specific file
    previewMode = true;
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void SleepImagePickerActivity::renderList(int pageWidth, int pageHeight) {
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_SLEEP_IMAGE_PICKER));

  const int menuTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int menuHeight = pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (fileList.empty()) {
    const int midY = menuTop + menuHeight / 2 - renderer.getLineHeight(UI_10_FONT_ID);
    renderer.drawCenteredText(UI_10_FONT_ID, midY, tr(STR_NO_BMP_FILES));
    renderer.drawCenteredText(SMALL_FONT_ID, midY + renderer.getLineHeight(UI_10_FONT_ID) + 6,
                              tr(STR_PLACE_BMP_IN_SLEEP));
  } else {
    const int count = totalItems();
    GUI.drawList(renderer, Rect{0, menuTop, pageWidth, menuHeight}, count, selectorIndex,
                 [this](int idx) -> std::string {
                   if (idx == 0) return tr(STR_ANY_RANDOM);
                   return fileList[idx - 1];
                 });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), fileList.empty() ? "" : tr(STR_SELECT),
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void SleepImagePickerActivity::renderPreview(int pageWidth, int pageHeight) {
  // Build full path for the selected file
  const std::string fullPath = sleepDir + "/" + fileList[selectorIndex - 1];

  FsFile bmpFile;
  if (Storage.openFileForRead("SIP", fullPath.c_str(), bmpFile)) {
    Bitmap bitmap(bmpFile, false);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      // Center smaller images; larger images fill from origin and are scaled by drawBitmap
      const int x = (bitmap.getWidth() < pageWidth) ? (pageWidth - bitmap.getWidth()) / 2 : 0;
      const int y = (bitmap.getHeight() < pageHeight) ? (pageHeight - bitmap.getHeight()) / 2 : 0;
      renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0);
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Error loading preview");
    }
    bmpFile.close();
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "File not found");
  }

  // Overlay: filename at top, button hints at bottom
  const std::string& filename = fileList[selectorIndex - 1];
  renderer.drawCenteredText(SMALL_FONT_ID, 6, filename.c_str());

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void SleepImagePickerActivity::render(RenderLock&&) {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  if (previewMode && selectorIndex > 0) {
    renderPreview(pageWidth, pageHeight);
  } else {
    renderList(pageWidth, pageHeight);
  }

  renderer.displayBuffer();
}
