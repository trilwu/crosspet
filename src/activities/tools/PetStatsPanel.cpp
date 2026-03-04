#include "PetStatsPanel.h"

#include <I18n.h>

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

  drawStatBar(renderer, x, y,                     w, tr(STR_PET_HUNGER),     state.hunger);
  drawStatBar(renderer, x, y + barSpacing,         w, tr(STR_PET_STAT_HAPPY), state.happiness);
  drawStatBar(renderer, x, y + barSpacing * 2,     w, tr(STR_PET_HEALTH),     state.health);
  drawStatBar(renderer, x, y + barSpacing * 3,     w, tr(STR_PET_WEIGHT),     state.weight);
  drawStatBar(renderer, x, y + barSpacing * 4,     w, tr(STR_PET_DISCIPLINE), state.discipline);

  // Reading progress bar: pages accumulated toward next meal (0-PAGES_PER_MEAL)
  const uint8_t readPct = static_cast<uint8_t>(
      state.pageAccumulator * 100u / PetConfig::PAGES_PER_MEAL);
  drawStatBar(renderer, x, y + barSpacing * 5,     w, tr(STR_PET_STAT_READING), readPct);

  // Care mistakes counter below bars
  char mistakesBuf[24];
  snprintf(mistakesBuf, sizeof(mistakesBuf), tr(STR_PET_MISTAKES_FORMAT), state.careMistakes);
  renderer.drawText(SMALL_FONT_ID, x, y + barSpacing * 6, mistakesBuf);
}
