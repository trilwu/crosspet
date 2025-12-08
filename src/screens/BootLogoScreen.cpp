#include "BootLogoScreen.h"

#include <GfxRenderer.h>

#include "config.h"
#include "images/CrossLarge.h"

void BootLogoScreen::onEnter() {
  const auto pageWidth = GfxRenderer::getScreenWidth();
  const auto pageHeight = GfxRenderer::getScreenHeight();

  renderer.clearScreen();
  // Location for images is from top right in landscape orientation
  renderer.drawImage(CrossLarge, (pageHeight - 128) / 2, (pageWidth - 128) / 2, 128, 128);
  const int width = renderer.getTextWidth(UI_FONT_ID, "CrossPoint", BOLD);
  renderer.drawText(UI_FONT_ID, (pageWidth - width) / 2, pageHeight / 2 + 70, "CrossPoint", true, BOLD);
  const int bootingWidth = renderer.getTextWidth(SMALL_FONT_ID, "BOOTING");
  renderer.drawText(SMALL_FONT_ID, (pageWidth - bootingWidth) / 2, pageHeight / 2 + 95, "BOOTING");
  renderer.displayBuffer();
}
