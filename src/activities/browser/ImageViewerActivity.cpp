#include "ImageViewerActivity.h"

#include <Bitmap.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <JpegToBmpConverter.h>
#include <Logging.h>
#include <PngToBmpConverter.h>

#include <cmath>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

ImageViewerActivity::ImageViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         std::string imagePath)
    : Activity("ImageViewer", renderer, mappedInput), imagePath(std::move(imagePath)) {}

void ImageViewerActivity::onEnter() {
  Activity::onEnter();
  ditherMode = SETTINGS.ditherMode;
  loadSuccess = convertAndDisplay();
}

void ImageViewerActivity::onExit() {
  Activity::onExit();
  // Clean up temp BMP if it exists
  Storage.remove(TEMP_BMP_PATH);
}

void ImageViewerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // Left button: cycle dither mode
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    ditherMode = (ditherMode == CrossPointSettings::DITHER_NONE) ? CrossPointSettings::DITHER_BAYER
                                                                 : CrossPointSettings::DITHER_NONE;
    SETTINGS.ditherMode = ditherMode;
    SETTINGS.saveToFile();

    showDitherLabel = true;
    ditherLabelMs = millis();

    // Re-convert and re-render with new dither mode
    Storage.remove(TEMP_BMP_PATH);
    loadSuccess = convertAndDisplay();
    return;
  }

  // Hide dither label after timeout
  if (showDitherLabel && (millis() - ditherLabelMs >= DITHER_LABEL_DURATION_MS)) {
    showDitherLabel = false;
    requestUpdate();
  }
}

void ImageViewerActivity::render(RenderLock&&) {
  // Rendering is done in onEnter/loop via direct display calls (image viewer pattern)
  // Only re-draw label overlay if needed
  if (showDitherLabel) {
    const char* label = (ditherMode == CrossPointSettings::DITHER_BAYER) ? tr(STR_DITHER_BAYER) : tr(STR_DITHER_NONE);
    const int screenW = renderer.getScreenWidth();
    const int screenH = renderer.getScreenHeight();
    // Draw label at bottom center
    const int labelY = screenH - 30;
    const int labelW = renderer.getTextWidth(UI_10_FONT_ID, label) + 20;
    const int labelX = (screenW - labelW) / 2;
    renderer.fillRect(labelX, labelY - 4, labelW, 24, false);
    renderer.drawCenteredText(UI_10_FONT_ID, labelY, label);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}

bool ImageViewerActivity::convertAndDisplay() {
  if (FsHelpers::hasJpgExtension(imagePath)) {
    FsFile srcFile;
    if (!Storage.openFileForRead("IVA", imagePath, srcFile)) {
      LOG_ERR("IVA", "Failed to open JPEG: %s", imagePath.c_str());
      return showError("Cannot open image");
    }

    FsFile bmpFile;
    if (!Storage.openFileForWrite("IVA", std::string(TEMP_BMP_PATH), bmpFile)) {
      srcFile.close();
      LOG_ERR("IVA", "Failed to create temp BMP");
      return showError("SD write error");
    }

    const int screenW = renderer.getScreenWidth();
    const int screenH = renderer.getScreenHeight();
    const bool ok = JpegToBmpConverter::jpegFileToBmpStreamWithSize(srcFile, bmpFile, screenW, screenH);
    srcFile.close();
    bmpFile.close();

    if (!ok) {
      Storage.remove(TEMP_BMP_PATH);
      LOG_ERR("IVA", "JPEG conversion failed: %s", imagePath.c_str());
      return showError("JPEG decode failed");
    }

    return displayBmpFile(std::string(TEMP_BMP_PATH));

  } else if (FsHelpers::hasPngExtension(imagePath)) {
    FsFile srcFile;
    if (!Storage.openFileForRead("IVA", imagePath, srcFile)) {
      LOG_ERR("IVA", "Failed to open PNG: %s", imagePath.c_str());
      return showError("Cannot open image");
    }

    FsFile bmpFile;
    if (!Storage.openFileForWrite("IVA", std::string(TEMP_BMP_PATH), bmpFile)) {
      srcFile.close();
      LOG_ERR("IVA", "Failed to create temp BMP");
      return showError("SD write error");
    }

    const int screenW = renderer.getScreenWidth();
    const int screenH = renderer.getScreenHeight();
    const bool ok = PngToBmpConverter::pngFileToBmpStreamWithSize(srcFile, bmpFile, screenW, screenH);
    srcFile.close();
    bmpFile.close();

    if (!ok) {
      Storage.remove(TEMP_BMP_PATH);
      LOG_ERR("IVA", "PNG conversion failed: %s", imagePath.c_str());
      return showError("PNG decode failed");
    }

    return displayBmpFile(std::string(TEMP_BMP_PATH));

  } else if (FsHelpers::hasBmpExtension(imagePath)) {
    return displayBmpFile(imagePath);

  } else {
    return showError("Unsupported format");
  }
}

bool ImageViewerActivity::displayBmpFile(const std::string& bmpPath) {
  FsFile file;
  if (!Storage.openFileForRead("IVA", bmpPath, file)) {
    LOG_ERR("IVA", "Failed to open BMP: %s", bmpPath.c_str());
    return showError("Cannot open BMP");
  }

  const bool isDither = (ditherMode == CrossPointSettings::DITHER_BAYER);
  Bitmap bitmap(file, isDither);

  const auto parseResult = bitmap.parseHeaders();
  if (parseResult != BmpReaderError::Ok) {
    file.close();
    LOG_ERR("IVA", "BMP parse error: %s", Bitmap::errorToString(parseResult));
    return showError("Invalid image");
  }

  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();

  int x, y;
  calcCenterFit(bitmap.getWidth(), bitmap.getHeight(), screenW, screenH, x, y);

  renderer.clearScreen();
  renderer.drawBitmap(bitmap, x, y, screenW, screenH, 0, 0);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DITHER_CYCLE), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  file.close();
  return true;
}

void ImageViewerActivity::calcCenterFit(int imgW, int imgH, int screenW, int screenH, int& outX, int& outY) const {
  if (imgW <= 0 || imgH <= 0) {
    outX = 0;
    outY = 0;
    return;
  }

  if (imgW > screenW || imgH > screenH) {
    const float ratio = static_cast<float>(imgW) / static_cast<float>(imgH);
    const float screenRatio = static_cast<float>(screenW) / static_cast<float>(screenH);
    if (ratio > screenRatio) {
      outX = 0;
      outY = static_cast<int>(std::round((static_cast<float>(screenH) - static_cast<float>(screenW) / ratio) / 2.0f));
    } else {
      outX = static_cast<int>(std::round((static_cast<float>(screenW) - static_cast<float>(screenH) * ratio) / 2.0f));
      outY = 0;
    }
  } else {
    outX = (screenW - imgW) / 2;
    outY = (screenH - imgH) / 2;
  }
}

bool ImageViewerActivity::showError(const char* msg) {
  renderer.clearScreen();
  const int screenH = renderer.getScreenHeight();
  renderer.drawCenteredText(UI_10_FONT_ID, screenH / 2, msg);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  return false;
}
