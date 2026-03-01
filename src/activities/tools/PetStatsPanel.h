#pragma once

#include <GfxRenderer.h>

#include "pet/PetState.h"

// Renders pet stat bars (hunger, happiness, health, weight, discipline)
// and a status icon row (sleeping, sick, dirty, attention indicators).
// All methods are stateless; pass GfxRenderer and PetState by ref.
class PetStatsPanel {
 public:
  // Draw row of status icons below the pet sprite (x/y = top-left, w = available width)
  void renderStatusIcons(GfxRenderer& renderer, const PetState& state, int x, int y, int w) const;

  // Draw 5 stat bars + care-mistakes counter
  void renderStats(GfxRenderer& renderer, const PetState& state, int x, int y, int w) const;

 private:
  void drawStatBar(GfxRenderer& renderer, int x, int y, int barW,
                   const char* label, uint8_t value) const;
};
