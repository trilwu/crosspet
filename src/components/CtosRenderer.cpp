#include "CtosRenderer.h"

#include <cstdio>
#include <cstring>

#include "fontIds.h"

void CtosRenderer::drawCornerBrackets(int x, int y, int w, int h, int size) const {
  // Top-left
  gfx.drawLine(x, y, x + size, y);
  gfx.drawLine(x, y, x, y + size);
  // Top-right
  gfx.drawLine(x + w - size, y, x + w, y);
  gfx.drawLine(x + w, y, x + w, y + size);
  // Bottom-left
  gfx.drawLine(x, y + h - size, x, y + h);
  gfx.drawLine(x, y + h, x + size, y + h);
  // Bottom-right
  gfx.drawLine(x + w, y + h - size, x + w, y + h);
  gfx.drawLine(x + w - size, y + h, x + w, y + h);
}

void CtosRenderer::drawHeader(int y, const char* title, const char* rightText) const {
  int sw = gfx.getScreenWidth();
  // Draw header text with chevron prefix
  char buf[64];
  snprintf(buf, sizeof(buf), "[CTOS] >> %s", title);
  gfx.drawText(FONT, MARGIN, y, buf);

  if (rightText) {
    int tw = gfx.getTextWidth(FONT, rightText);
    gfx.drawText(FONT, sw - MARGIN - tw, y, rightText);
  }

  // Separator line below header
  int lineY = y + gfx.getLineHeight(FONT) + 2;
  gfx.drawLine(MARGIN, lineY, sw - MARGIN, lineY);
}

void CtosRenderer::drawStatusBar(int y, const char* text) const {
  int sw = gfx.getScreenWidth();
  gfx.drawLine(MARGIN, y, sw - MARGIN, y);
  char buf[128];
  snprintf(buf, sizeof(buf), ">> %s", text);
  gfx.drawText(FONT, MARGIN, y + 4, buf);
}

void CtosRenderer::drawSeparator(int y, const char* label) const {
  int sw = gfx.getScreenWidth();
  if (label) {
    int tw = gfx.getTextWidth(FONT, label);
    int mid = sw / 2;
    int half = tw / 2 + 8;
    gfx.drawLine(MARGIN, y, mid - half, y);
    gfx.drawText(FONT, mid - tw / 2, y - gfx.getLineHeight(FONT) / 2, label);
    gfx.drawLine(mid + half, y, sw - MARGIN, y);
  } else {
    gfx.drawLine(MARGIN, y, sw - MARGIN, y);
  }
}

void CtosRenderer::drawMenuItem(int x, int y, int w, int h, const char* label, bool selected) const {
  if (selected) {
    gfx.fillRect(x, y, w, h, true);  // black fill
  } else {
    drawCornerBrackets(x, y, w, h, 6);
  }

  // Draw label with \n support — center vertically in cell
  int lineH = gfx.getLineHeight(FONT);
  // Count lines
  int lineCount = 1;
  for (const char* p = label; *p; p++) if (*p == '\n') lineCount++;
  int totalTextH = lineCount * lineH;
  int ty = y + (h - totalTextH) / 2;

  const char* start = label;
  for (int line = 0; line < lineCount; line++) {
    const char* end = start;
    while (*end && *end != '\n') end++;
    char lineBuf[32];
    int len = static_cast<int>(end - start);
    if (len >= static_cast<int>(sizeof(lineBuf))) len = sizeof(lineBuf) - 1;
    memcpy(lineBuf, start, len);
    lineBuf[len] = '\0';
    // Center horizontally with chevron prefix
    char prefixed[40];
    snprintf(prefixed, sizeof(prefixed), ">> %s", lineBuf);
    int tw = gfx.getTextWidth(FONT, prefixed);
    int tx = x + (w - tw) / 2;
    gfx.drawText(FONT, tx, ty, prefixed, !selected);
    ty += lineH;
    start = (*end == '\n') ? end + 1 : end;
  }
}

void CtosRenderer::drawSignalBar(int x, int y, int rssi) const {
  // Map RSSI to 0-4 bars: >-50=4, >-60=3, >-70=2, >-80=1, else 0
  int bars = 0;
  if (rssi > -50) bars = 4;
  else if (rssi > -60) bars = 3;
  else if (rssi > -70) bars = 2;
  else if (rssi > -80) bars = 1;

  int barW = 4, barGap = 2, barMaxH = 12;
  for (int i = 0; i < 4; i++) {
    int bh = (i + 1) * 3;  // 3, 6, 9, 12
    int bx = x + i * (barW + barGap);
    int by = y + barMaxH - bh;
    if (i < bars) {
      gfx.fillRect(bx, by, barW, bh);
    } else {
      gfx.drawRect(bx, by, barW, bh);
    }
  }
}

void CtosRenderer::drawKeyValue(int x, int y, const char* key, const char* value) const {
  char buf[96];
  snprintf(buf, sizeof(buf), "%s: %s", key, value);
  gfx.drawText(FONT, x, y, buf);
}

void CtosRenderer::drawMonoText(int x, int y, const char* text, bool inverted) const {
  if (inverted) {
    int tw = gfx.getTextWidth(FONT, text);
    int th = gfx.getLineHeight(FONT);
    gfx.fillRect(x - 2, y - 1, tw + 4, th + 2);
    gfx.drawText(FONT, x, y, text, false);
  } else {
    gfx.drawText(FONT, x, y, text);
  }
}

void CtosRenderer::drawChevronLabel(int x, int y, const char* label) const {
  char buf[64];
  snprintf(buf, sizeof(buf), ">> %s", label);
  gfx.drawText(FONT, x, y, buf);
}

void CtosRenderer::drawCtosFrame(const char* title, const char* rightText) const {
  int sw = gfx.getScreenWidth();
  int sh = gfx.getScreenHeight();
  gfx.clearScreen();
  drawCornerBrackets(4, 4, sw - 8, sh - 8, 12);
  drawHeader(MARGIN + 4, title, rightText);
}

void CtosRenderer::drawToolGrid(const char* const* labels, int count, int selectedIdx) const {
  int sw = gfx.getScreenWidth();
  int sh = gfx.getScreenHeight();
  int headerH = gfx.getLineHeight(FONT) + MARGIN + 8;  // header + separator
  int statusH = 40;  // status bar at bottom
  int availH = sh - headerH - statusH;

  int cols = 2;
  int rows = (count + cols - 1) / cols;
  int gap = MARGIN;
  int cellW = (sw - gap * (cols + 1)) / cols;
  int cellH = (availH - gap * (rows + 1)) / rows;

  int gridH = rows * cellH + (rows - 1) * gap;
  int startY = headerH + (availH - gridH) / 2;

  for (int i = 0; i < count; i++) {
    int col = i % cols;
    int row = i / cols;
    int x = gap + col * (cellW + gap);
    int y = startY + row * (cellH + gap);
    drawMenuItem(x, y, cellW, cellH, labels[i], i == selectedIdx);
  }
}
