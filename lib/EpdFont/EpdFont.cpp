#include "EpdFont.h"

#include <Utf8.h>

inline int min(const int a, const int b) { return a < b ? a : b; }
inline int max(const int a, const int b) { return a < b ? b : a; }

void EpdFont::getTextBounds(const char* string, const int startX, const int startY, int* minX, int* minY, int* maxX,
                            int* maxY) const {
  *minX = startX;
  *minY = startY;
  *maxX = startX;
  *maxY = startY;

  if (*string == '\0') {
    return;
  }

  int cursorX = startX;
  const int cursorY = startY;
  uint32_t cp;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&string)))) {
    const EpdGlyph* glyph = getGlyph(cp);

    if (!glyph) {
      // TODO: Replace with fallback glyph property?
      glyph = getGlyph('?');
    }

    if (!glyph) {
      // TODO: Better handle this?
      continue;
    }

    *minX = min(*minX, cursorX + glyph->left);
    *maxX = max(*maxX, cursorX + glyph->left + glyph->width);
    *minY = min(*minY, cursorY + glyph->top - glyph->height);
    *maxY = max(*maxY, cursorY + glyph->top);
    cursorX += glyph->advanceX;
  }
}

void EpdFont::getTextDimensions(const char* string, int* w, int* h) const {
  int minX = 0, minY = 0, maxX = 0, maxY = 0;

  getTextBounds(string, 0, 0, &minX, &minY, &maxX, &maxY);

  *w = maxX - minX;
  *h = maxY - minY;
}

bool EpdFont::hasPrintableChars(const char* string) const {
  int w = 0, h = 0;

  getTextDimensions(string, &w, &h);

  return w > 0 || h > 0;
}

const EpdGlyph* EpdFont::getGlyph(const uint32_t cp) const {
  const EpdUnicodeInterval* intervals = data->intervals;
  for (int i = 0; i < data->intervalCount; i++) {
    const EpdUnicodeInterval* interval = &intervals[i];
    if (cp >= interval->first && cp <= interval->last) {
      return &data->glyph[interval->offset + (cp - interval->first)];
    }
    if (cp < interval->first) {
      return nullptr;
    }
  }
  return nullptr;
}
