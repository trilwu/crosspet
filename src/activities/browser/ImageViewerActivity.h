#pragma once

#include <string>

#include "../Activity.h"

/**
 * ImageViewerActivity
 *
 * Displays a single image file (JPEG, PNG, or BMP) centered on screen.
 * Left button cycles dither mode. Back button exits.
 *
 * For JPEG/PNG: converts to a temp BMP on SD card, then reads via Bitmap class.
 * For BMP: reads directly.
 */
class ImageViewerActivity final : public Activity {
 public:
  ImageViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string imagePath);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string imagePath;

  // Dither mode: 0=None, 1=Bayer (matches CrossPointSettings::DITHER_MODE)
  uint8_t ditherMode = 0;

  // Whether the image was successfully loaded and rendered
  bool loadSuccess = false;

  // Show dither mode label briefly after toggle
  bool showDitherLabel = false;
  unsigned long ditherLabelMs = 0;

  static constexpr unsigned long DITHER_LABEL_DURATION_MS = 1500;
  static constexpr const char* TEMP_BMP_PATH = "/.imgview_tmp.bmp";

  // Convert JPEG/PNG to temp BMP, then render via drawBitmapFromFile
  bool convertAndDisplay();
  // Display a BMP file (path must already be a .bmp)
  bool displayBmpFile(const std::string& bmpPath);
  // Center-fit coordinates for an image of given size on screen
  void calcCenterFit(int imgW, int imgH, int screenW, int screenH, int& outX, int& outY) const;
  // Show error message and return false
  bool showError(const char* msg);
};
