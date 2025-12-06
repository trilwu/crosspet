#pragma once
#include <EpdFontFamily.h>
#include <HardwareSerial.h>
#include <Utf8.h>

inline int min(const int a, const int b) { return a < b ? a : b; }
inline int max(const int a, const int b) { return a > b ? a : b; }

template <typename Renderable>
class EpdFontRenderer {
  Renderable& renderer;
  void renderChar(uint32_t cp, int* x, const int* y, uint16_t color, EpdFontStyle style = REGULAR);

 public:
  const EpdFontFamily* fontFamily;
  explicit EpdFontRenderer(const EpdFontFamily* fontFamily, Renderable& renderer)
      : fontFamily(fontFamily), renderer(renderer) {}
  ~EpdFontRenderer() = default;
  void renderString(const char* string, int* x, int* y, uint16_t color, EpdFontStyle style = REGULAR);
};

template <typename Renderable>
void EpdFontRenderer<Renderable>::renderString(const char* string, int* x, int* y, const uint16_t color,
                                               const EpdFontStyle style) {
  // cannot draw a NULL / empty string
  if (string == nullptr || *string == '\0') {
    return;
  }

  // no printable characters
  if (!fontFamily->hasPrintableChars(string, style)) {
    return;
  }

  uint32_t cp;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&string)))) {
    renderChar(cp, x, y, color, style);
  }

  *y += fontFamily->getData(style)->advanceY;
}

template <typename Renderable>
void EpdFontRenderer<Renderable>::renderChar(const uint32_t cp, int* x, const int* y, uint16_t color,
                                             const EpdFontStyle style) {
  const EpdGlyph* glyph = fontFamily->getGlyph(cp, style);
  if (!glyph) {
    // TODO: Replace with fallback glyph property?
    glyph = fontFamily->getGlyph('?', style);
  }

  // no glyph?
  if (!glyph) {
    Serial.printf("No glyph for codepoint %d\n", cp);
    return;
  }

  const uint32_t offset = glyph->dataOffset;
  const uint8_t width = glyph->width;
  const uint8_t height = glyph->height;
  const int left = glyph->left;

  const uint8_t* bitmap = nullptr;
  bitmap = &fontFamily->getData(style)->bitmap[offset];

  if (bitmap != nullptr) {
    for (int glyphY = 0; glyphY < height; glyphY++) {
      int screenY = *y - glyph->top + glyphY;
      for (int glyphX = 0; glyphX < width; glyphX++) {
        const int pixelPosition = glyphY * width + glyphX;
        int screenX = *x + left + glyphX;

        const uint8_t byte = bitmap[pixelPosition / 8];
        const uint8_t bit_index = 7 - (pixelPosition % 8);

        if ((byte >> bit_index) & 1) {
          renderer.drawPixel(screenX, screenY, color);
        }
      }
    }
  }

  *x += glyph->advanceX;
}
