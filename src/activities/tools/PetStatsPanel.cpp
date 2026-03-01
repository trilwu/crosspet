#include "PetStatsPanel.h"

#include "fontIds.h"

// ---- Status icons -------------------------------------------------------

void PetStatsPanel::renderStatusIcons(GfxRenderer& renderer, const PetState& state,
                                       int x, int y, int w) const {
  // Draw small text indicators for active conditions, spaced horizontally.
  // E-ink limitation: no real icons, use text symbols.
  const int lh = renderer.getLineHeight(SMALL_FONT_ID);
  int cx = x;
  const int spacing = 6;

  auto drawIcon = [&](const char* symbol) {
    renderer.drawText(SMALL_FONT_ID, cx, y, symbol);
    cx += renderer.getTextWidth(SMALL_FONT_ID, symbol) + spacing;
  };

  if (state.isSleeping)    drawIcon("ZZ");
  if (state.isSick)        drawIcon("+");
  if (state.wasteCount > 0) drawIcon("**");
  if (state.attentionCall) drawIcon("!");
  if (state.hunger > 70 && state.happiness > 70) drawIcon("<3");

  (void)w;
  (void)lh;
}

// ---- Stat bars ----------------------------------------------------------

void PetStatsPanel::drawStatBar(GfxRenderer& renderer, int x, int y, int barW,
                                 const char* label, uint8_t value) const {
  constexpr int BAR_H = 10;
  const int lh = renderer.getLineHeight(SMALL_FONT_ID);
  const int lblW = renderer.getTextWidth(SMALL_FONT_ID, label);

  renderer.drawText(SMALL_FONT_ID, x, y, label);

  const int bx = x + lblW + 6;
  const int actualW = barW - lblW - 6;
  const int barY = y + (lh - BAR_H) / 2;
  renderer.drawRect(bx, barY, actualW, BAR_H);

  if (value > 0) {
    const int fillW = (actualW - 2) * value / 100;
    if (fillW > 0) renderer.fillRect(bx + 1, barY + 1, fillW, BAR_H - 2);
  }
}

void PetStatsPanel::renderStats(GfxRenderer& renderer, const PetState& state,
                                 int x, int y, int w) const {
  const int barSpacing = renderer.getLineHeight(SMALL_FONT_ID) + 10;

  drawStatBar(renderer, x, y,                     w, "Hunger",     state.hunger);
  drawStatBar(renderer, x, y + barSpacing,         w, "Happy",      state.happiness);
  drawStatBar(renderer, x, y + barSpacing * 2,     w, "Health",     state.health);
  drawStatBar(renderer, x, y + barSpacing * 3,     w, "Weight",     state.weight);
  drawStatBar(renderer, x, y + barSpacing * 4,     w, "Discipline", state.discipline);

  // Care mistakes counter below bars
  char mistakesBuf[24];
  snprintf(mistakesBuf, sizeof(mistakesBuf), "Mistakes: %d", state.careMistakes);
  renderer.drawText(SMALL_FONT_ID, x, y + barSpacing * 5, mistakesBuf);
}
