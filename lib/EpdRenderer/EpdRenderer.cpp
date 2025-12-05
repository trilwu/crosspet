#include "EpdRenderer.h"

#include "builtinFonts/babyblue.h"
#include "builtinFonts/bookerly.h"
#include "builtinFonts/bookerly_bold.h"
#include "builtinFonts/bookerly_bold_italic.h"
#include "builtinFonts/bookerly_italic.h"

EpdRenderer::EpdRenderer(XteinkDisplay* display) {
  this->display = display;
  this->regularFont = new EpdFontRenderer<XteinkDisplay>(new EpdFont(&bookerly), display);
  this->boldFont = new EpdFontRenderer<XteinkDisplay>(new EpdFont(&bookerly_bold), display);
  this->italicFont = new EpdFontRenderer<XteinkDisplay>(new EpdFont(&bookerly_italic), display);
  this->bold_italicFont = new EpdFontRenderer<XteinkDisplay>(new EpdFont(&bookerly_bold_italic), display);
  this->smallFont = new EpdFontRenderer<XteinkDisplay>(new EpdFont(&babyblue), display);

  this->marginTop = 11;
  this->marginBottom = 30;
  this->marginLeft = 10;
  this->marginRight = 10;
  this->lineCompression = 0.95f;
}

EpdFontRenderer<XteinkDisplay>* EpdRenderer::getFontRenderer(const bool bold, const bool italic) const {
  if (bold && italic) {
    return bold_italicFont;
  }
  if (bold) {
    return boldFont;
  }
  if (italic) {
    return italicFont;
  }
  return regularFont;
}

int EpdRenderer::getTextWidth(const char* text, const bool bold, const bool italic) const {
  int w = 0, h = 0;

  getFontRenderer(bold, italic)->font->getTextDimensions(text, &w, &h);

  return w;
}

int EpdRenderer::getSmallTextWidth(const char* text) const {
  int w = 0, h = 0;

  smallFont->font->getTextDimensions(text, &w, &h);

  return w;
}

void EpdRenderer::drawText(const int x, const int y, const char* text, const bool bold, const bool italic,
                           const uint16_t color) const {
  int ypos = y + getLineHeight() + marginTop;
  int xpos = x + marginLeft;
  getFontRenderer(bold, italic)->renderString(text, &xpos, &ypos, color > 0 ? GxEPD_BLACK : GxEPD_WHITE);
}

void EpdRenderer::drawSmallText(const int x, const int y, const char* text, const uint16_t color) const {
  int ypos = y + smallFont->font->data->advanceY + marginTop;
  int xpos = x + marginLeft;
  smallFont->renderString(text, &xpos, &ypos, color > 0 ? GxEPD_BLACK : GxEPD_WHITE);
}

void EpdRenderer::drawTextBox(const int x, const int y, const std::string& text, const int width, const int height,
                              const bool bold, const bool italic) const {
  const size_t length = text.length();
  // fit the text into the box
  int start = 0;
  int end = 1;
  int ypos = 0;
  while (true) {
    if (end >= length) {
      drawText(x, y + ypos, text.substr(start, length - start).c_str(), bold, italic);
      break;
    }

    if (ypos + getLineHeight() >= height) {
      break;
    }

    if (text[end - 1] == '\n') {
      drawText(x, y + ypos, text.substr(start, end - start).c_str(), bold, italic);
      ypos += getLineHeight();
      start = end;
      end = start + 1;
      continue;
    }

    if (getTextWidth(text.substr(start, end - start).c_str(), bold, italic) > width) {
      drawText(x, y + ypos, text.substr(start, end - start - 1).c_str(), bold, italic);
      ypos += getLineHeight();
      start = end - 1;
      continue;
    }

    end++;
  }
}

void EpdRenderer::drawLine(int x1, int y1, int x2, int y2, uint16_t color) const {
  display->drawLine(x1 + marginLeft, y1 + marginTop, x2 + marginLeft, y2 + marginTop,
                    color > 0 ? GxEPD_BLACK : GxEPD_WHITE);
}

void EpdRenderer::drawRect(const int x, const int y, const int width, const int height, const uint16_t color) const {
  display->drawRect(x + marginLeft, y + marginTop, width, height, color > 0 ? GxEPD_BLACK : GxEPD_WHITE);
}

void EpdRenderer::fillRect(const int x, const int y, const int width, const int height,
                           const uint16_t color = 0) const {
  display->fillRect(x + marginLeft, y + marginTop, width, height, color > 0 ? GxEPD_BLACK : GxEPD_WHITE);
}

void EpdRenderer::clearScreen(const bool black) const {
  Serial.println("Clearing screen");
  display->fillScreen(black ? GxEPD_BLACK : GxEPD_WHITE);
}

void EpdRenderer::flushDisplay(const bool partialUpdate) const { display->display(partialUpdate); }

void EpdRenderer::flushArea(const int x, const int y, const int width, const int height) const {
  display->displayWindow(x, y, width, height);
}

int EpdRenderer::getPageWidth() const { return display->width() - marginLeft - marginRight; }

int EpdRenderer::getPageHeight() const { return display->height() - marginTop - marginBottom; }

int EpdRenderer::getSpaceWidth() const { return regularFont->font->getGlyph(' ')->advanceX; }

int EpdRenderer::getLineHeight() const { return regularFont->font->data->advanceY * lineCompression; }

// deep sleep helper - persist any state to disk that may be needed on wake
bool EpdRenderer::dehydrate() {
  // TODO: Implement
  return false;
};

// deep sleep helper - retrieve any state from disk after wake
bool EpdRenderer::hydrate() {
  // TODO: Implement
  return false;
};

// really really clear the screen
void EpdRenderer::reset() {
  // TODO: Implement
};
