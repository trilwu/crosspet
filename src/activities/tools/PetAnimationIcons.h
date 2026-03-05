#pragma once

#include <GfxRenderer.h>
#include <cstdint>

// Action feedback icon type shown briefly after pet actions
enum class PetAnimIcon : uint8_t {
  NONE = 0,
  HEART,     // petting
  FOOD,      // feeding meal
  SNACK,     // feeding snack
  MEDICINE,  // medicine cross
  EXERCISE,  // lightning bolt
  CLEAN,     // water drops
  SCOLD,     // exclamation
  SLEEP,     // Zzz
};

// Draws small 16x16 pixel-art action feedback icons
void drawPetActionIcon(GfxRenderer& renderer, PetAnimIcon icon, int x, int y);
