#pragma once

#include <GfxRenderer.h>

#include "fontIds.h"

// Watch Dogs ctOS-style drawing primitives for e-ink.
// Wraps GfxRenderer — no extra framebuffer, pure BW.
class CtosRenderer {
 public:
  explicit CtosRenderer(GfxRenderer& renderer) : gfx(renderer) {}

  // Frame elements
  void drawCornerBrackets(int x, int y, int w, int h, int size = 8) const;
  void drawHeader(int y, const char* title, const char* rightText = nullptr) const;
  void drawStatusBar(int y, const char* text) const;
  void drawSeparator(int y, const char* label = nullptr) const;

  // Content elements
  void drawMenuItem(int x, int y, int w, int h, const char* label, bool selected = false) const;
  void drawSignalBar(int x, int y, int rssi) const;  // -30 to -100 dBm → 4 bars
  void drawKeyValue(int x, int y, const char* key, const char* value) const;

  // Text helpers
  void drawMonoText(int x, int y, const char* text, bool inverted = false) const;
  void drawChevronLabel(int x, int y, const char* label) const;  // >> LABEL

  // Screen templates
  void drawCtosFrame(const char* title, const char* rightText = nullptr) const;
  void drawToolGrid(const char** labels, int count, int selectedIdx) const;

  GfxRenderer& getGfx() { return gfx; }

 private:
  GfxRenderer& gfx;
  static constexpr int FONT = UI_12_FONT_ID;
  static constexpr int MARGIN = 10;
};
