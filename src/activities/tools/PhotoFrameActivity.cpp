#include "PhotoFrameActivity.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include "components/UITheme.h"
#include "fontIds.h"

void PhotoFrameActivity::scanPhotos() {
  photos.clear();
  // Share the same /sleep folder used by the sleep screen custom images
  auto dir = Storage.open("/sleep");
  if (!dir || !dir.isDirectory()) return;

  char name[256];
  for (auto f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) { f.close(); continue; }
    f.getName(name, sizeof(name));
    std::string fname(name);
    if (fname.empty() || fname[0] == '.') { f.close(); continue; }  // skip hidden/macOS files
    if (fname.size() >= 4) {
      std::string ext = fname.substr(fname.size() - 4);
      // Case-insensitive .bmp check (FAT32 may return uppercase)
      for (auto& c : ext) c = (char)tolower((unsigned char)c);
      if (ext == ".bmp") photos.emplace_back("/sleep/" + fname);
    }
    f.close();
  }
  dir.close();
  LOG_DBG("PF", "Found %d photos in /sleep", (int)photos.size());
}

void PhotoFrameActivity::onEnter() {
  Activity::onEnter();
  scanPhotos();
  currentIndex = 0;
  lastAdvanceMs = millis();
  requestUpdate();
}

void PhotoFrameActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  bool changed = false;

  if (mappedInput.wasReleased(MappedInputManager::Button::Left) && !photos.empty()) {
    currentIndex = (currentIndex - 1 + (int)photos.size()) % (int)photos.size();
    lastAdvanceMs = millis();
    showOverlay = true;
    overlayEndMs = millis() + 2000;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) && !photos.empty()) {
    currentIndex = (currentIndex + 1) % (int)photos.size();
    lastAdvanceMs = millis();
    showOverlay = true;
    overlayEndMs = millis() + 2000;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    intervalIdx = (intervalIdx + 1) % INTERVAL_COUNT;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    intervalIdx = (intervalIdx - 1 + INTERVAL_COUNT) % INTERVAL_COUNT;
    changed = true;
  }

  // Auto-advance timer
  if (!photos.empty() && (millis() - lastAdvanceMs) >= INTERVALS_MS[intervalIdx]) {
    currentIndex = (currentIndex + 1) % (int)photos.size();
    lastAdvanceMs = millis();
    showOverlay = false;
    changed = true;
  }

  // Overlay expiry
  if (showOverlay && millis() >= overlayEndMs) {
    showOverlay = false;
    changed = true;
  }

  if (changed) requestUpdate();
}

void PhotoFrameActivity::render(RenderLock&&) {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  if (photos.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "No photos found");
    renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 32, "Place .bmp files in /sleep/");
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  FsFile file;
  if (Storage.openFileForRead("PF", photos[currentIndex], file)) {
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      int x, y;
      float ratio = (float)bitmap.getWidth() / (float)bitmap.getHeight();
      float screenRatio = (float)pageWidth / (float)pageHeight;
      if (ratio > screenRatio) {
        x = 0;
        y = (pageHeight - (int)(pageWidth / ratio)) / 2;
      } else {
        x = (pageWidth - (int)(pageHeight * ratio)) / 2;
        y = 0;
      }
      renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0);
    }
    file.close();
  }

  // Filename + index overlay (briefly after navigation)
  if (showOverlay) {
    char info[64];
    const char* path = photos[currentIndex].c_str();
    const char* slash = strrchr(path, '/');
    snprintf(info, sizeof(info), "%s  [%d/%d]",
             slash ? slash + 1 : path, currentIndex + 1, (int)photos.size());
    const int textY = pageHeight - 48;
    renderer.fillRect(0, textY - 6, pageWidth, 44);
    renderer.drawCenteredText(SMALL_FONT_ID, textY, info, false);
  }

  const char* ivlStr = intervalIdx == 0 ? "30s" : intervalIdx == 1 ? "1m" :
                       intervalIdx == 2 ? "2m" : "5m";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", ivlStr, ivlStr);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}
