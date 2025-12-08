#include "FullScreenMessageScreen.h"

#include <GfxRenderer.h>

#include "config.h"

void FullScreenMessageScreen::onEnter() {
  const auto width = renderer.getTextWidth(UI_FONT_ID, text.c_str(), style);
  const auto height = renderer.getLineHeight(UI_FONT_ID);
  const auto left = (GfxRenderer::getScreenWidth() - width) / 2;
  const auto top = (GfxRenderer::getScreenHeight() - height) / 2;

  renderer.clearScreen();
  renderer.drawText(UI_FONT_ID, left, top, text.c_str(), true, style);
  renderer.displayBuffer(refreshMode);
}
