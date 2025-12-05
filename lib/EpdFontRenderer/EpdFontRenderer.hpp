#pragma once
#include <EpdFont.h>
#include <HardwareSerial.h>
#include <Utf8.h>
#include <miniz.h>

inline int min(const int a, const int b) { return a < b ? a : b; }
inline int max(const int a, const int b) { return a > b ? a : b; }

static tinfl_decompressor decomp;

template <typename Renderable>
class EpdFontRenderer {
  Renderable* renderer;
  void renderChar(uint32_t cp, int* x, const int* y, uint16_t color);

 public:
  const EpdFont* font;
  explicit EpdFontRenderer(const EpdFont* font, Renderable* renderer);
  ~EpdFontRenderer() = default;
  void renderString(const char* string, int* x, int* y, uint16_t color);
};

inline int uncompress(uint8_t* dest, size_t uncompressedSize, const uint8_t* source, size_t sourceSize) {
  if (uncompressedSize == 0 || dest == nullptr || sourceSize == 0 || source == nullptr) {
    return -1;
  }
  tinfl_init(&decomp);

  // we know everything will fit into the buffer.
  const tinfl_status decomp_status =
      tinfl_decompress(&decomp, source, &sourceSize, dest, dest, &uncompressedSize,
                       TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (decomp_status != TINFL_STATUS_DONE) {
    return decomp_status;
  }
  return 0;
}

template <typename Renderable>
EpdFontRenderer<Renderable>::EpdFontRenderer(const EpdFont* font, Renderable* renderer) {
  this->font = font;
  this->renderer = renderer;
}

template <typename Renderable>
void EpdFontRenderer<Renderable>::renderString(const char* string, int* x, int* y, const uint16_t color) {
  // cannot draw a NULL / empty string
  if (string == nullptr || *string == '\0') {
    return;
  }

  // no printable characters
  if (!font->hasPrintableChars(string)) {
    return;
  }

  uint32_t cp;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&string)))) {
    renderChar(cp, x, y, color);
  }

  *y += font->data->advanceY;
}

template <typename Renderable>
void EpdFontRenderer<Renderable>::renderChar(const uint32_t cp, int* x, const int* y, uint16_t color) {
  const EpdGlyph* glyph = font->getGlyph(cp);
  if (!glyph) {
    // TODO: Replace with fallback glyph property?
    glyph = font->getGlyph('?');
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

  const int byteWidth = width / 2 + width % 2;
  const unsigned long bitmapSize = byteWidth * height;
  const uint8_t* bitmap = nullptr;

  if (font->data->compressed) {
    auto* tmpBitmap = static_cast<uint8_t*>(malloc(bitmapSize));
    if (tmpBitmap == nullptr && bitmapSize) {
      Serial.println("Failed to allocate memory for decompression buffer");
      return;
    }

    uncompress(tmpBitmap, bitmapSize, &font->data->bitmap[offset], glyph->compressedSize);
    bitmap = tmpBitmap;
  } else {
    bitmap = &font->data->bitmap[offset];
  }

  if (bitmap != nullptr) {
    for (int localY = 0; localY < height; localY++) {
      int yy = *y - glyph->top + localY;
      const int startPos = *x + left;
      bool byteComplete = startPos % 2;
      int localX = max(0, -startPos);
      const int maxX = startPos + width;

      for (int xx = startPos; xx < maxX; xx++) {
        uint8_t bm = bitmap[localY * byteWidth + localX / 2];
        if ((localX & 1) == 0) {
          bm = bm & 0xF;
        } else {
          bm = bm >> 4;
        }

        if (bm) {
          renderer->drawPixel(xx, yy, color);
        }
        byteComplete = !byteComplete;
        localX++;
      }
    }

    if (font->data->compressed) {
      free(const_cast<uint8_t*>(bitmap));
    }
  }

  *x += glyph->advanceX;
}
