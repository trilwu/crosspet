#include "PetSpriteRenderer.h"

#include <HalStorage.h>

#include "fontIds.h"

// Static buffer shared across all sprite loads (saves heap, one-at-a-time use)
uint8_t PetSpriteRenderer::spriteBuffer[PetSpriteRenderer::SPRITE_BYTES];

// ---- Name helpers -------------------------------------------------------

const char* PetSpriteRenderer::stageName(PetStage stage) {
  switch (stage) {
    case PetStage::EGG:       return "egg";
    case PetStage::HATCHLING: return "hatchling";
    case PetStage::YOUNGSTER: return "youngster";
    case PetStage::COMPANION: return "companion";
    case PetStage::ELDER:     return "elder";
    case PetStage::DEAD:      return "dead";
    default:                  return "egg";
  }
}

const char* PetSpriteRenderer::moodName(PetMood mood) {
  switch (mood) {
    case PetMood::HAPPY:    return "happy";
    case PetMood::NEUTRAL:  return "neutral";
    case PetMood::SAD:      return "sad";
    case PetMood::SICK:     return "sick";
    case PetMood::SLEEPING: return "sleeping";
    case PetMood::DEAD:     return "dead";
    // New moods map to neutral sprite; attention indicator drawn separately by caller
    case PetMood::NEEDY:    return "neutral";
    case PetMood::REFUSING: return "neutral";
    default:                return "neutral";
  }
}

char PetSpriteRenderer::stageInitial(PetStage stage) {
  switch (stage) {
    case PetStage::EGG:       return 'E';
    case PetStage::HATCHLING: return 'H';
    case PetStage::YOUNGSTER: return 'Y';
    case PetStage::COMPANION: return 'C';
    case PetStage::ELDER:     return 'A';  // "Adult/Ancient"
    case PetStage::DEAD:      return 'X';
    default:                  return '?';
  }
}

// ---- File loading -------------------------------------------------------

size_t PetSpriteRenderer::loadSprite(const char* path, size_t expectedBytes) {
  // readFileToBuffer reads up to bufferSize-1 bytes and null-terminates;
  // we pass SPRITE_BYTES+1 so the binary payload is not truncated.
  size_t read = Storage.readFileToBuffer(path, reinterpret_cast<char*>(spriteBuffer),
                                         SPRITE_BYTES + 1, expectedBytes);
  return read;
}

// ---- Kitty pixel-art sprites (12x12 grid, bit11=col0 left, bit0=col11 right) ----
// Solid-filled body with hollow eye holes and feature gaps for a cute cat look.
// Each logical pixel renders as (4 * scale) physical pixels → 48x48 at scale=1, 96x96 at scale=2.
namespace {
// Kitten peeking from egg — cat ears above oval shell, two eye dots
static const uint16_t kSprite_Egg[12] = {
  0x204, 0x70E, 0x3FC, 0x7FE, 0xEF7, 0xFFF,
  0xF9F, 0xFFF, 0xFFF, 0x7FE, 0x3FC, 0x1F8
};
// Tiny kitten — compact 8-wide head, small ear tips, dot eyes, stub paws
static const uint16_t kSprite_Hatchling[12] = {
  0x108, 0x39C, 0x3FC, 0x3FC, 0x2F4, 0x3FC,
  0x39C, 0x3FC, 0x3FC, 0x3FC, 0x198, 0x198
};
// Growing kitten — medium head, clear eyes, mouth gap, four paws
static const uint16_t kSprite_Youngster[12] = {
  0x204, 0x70E, 0x7FE, 0x7FE, 0x6F6, 0x7FE,
  0x79E, 0x7FE, 0x7FE, 0x3FC, 0x30C, 0x30C
};
// Adult cat — full head, dot eyes, whisker gap, sturdy body and paws
static const uint16_t kSprite_Companion[12] = {
  0x204, 0x70E, 0x7FE, 0xFFF, 0xEF7, 0xFFF,
  0xBFD, 0xFFF, 0x7FE, 0x7FE, 0x606, 0x606
};
// Elder cat — broad ears, large head, full body, dignified whiskers
static const uint16_t kSprite_Elder[12] = {
  0x606, 0xF0F, 0xFFF, 0xFFF, 0xEF7, 0xF9F,
  0xBFD, 0xFFF, 0xFFF, 0x7FE, 0x606, 0x606
};
// Dead cat — X eyes (crossed hole pattern), flat laid-out body
static const uint16_t kSprite_Dead[12] = {
  0x204, 0x70E, 0x7FE, 0xFFF, 0xEF7, 0xD6B,
  0xF9F, 0xFFF, 0xFFF, 0xFFF, 0x8F1, 0xFFF
};

// Chubby youngster — wider lower body (variant 1)
static const uint16_t kSprite_Youngster_v1[12] = {
  0x204, 0x70E, 0x7FE, 0xFFE, 0x6F6, 0xFFE,
  0x79E, 0xFFE, 0xFFE, 0x7FC, 0x70E, 0x70E
};
// Rowdy youngster — furrowed brows, aggressive eye gap (variant 2)
static const uint16_t kSprite_Youngster_v2[12] = {
  0x204, 0xF0F, 0xFFE, 0xFFE, 0x5B6, 0x7FE,
  0x79E, 0x7FE, 0x7FE, 0x3FC, 0x30C, 0x30C
};
// Chonky companion — full/wide body (variant 1)
static const uint16_t kSprite_Companion_v1[12] = {
  0x204, 0x70E, 0xFFE, 0xFFF, 0xEF7, 0xFFF,
  0xBFD, 0xFFF, 0xFFE, 0xFFE, 0x70E, 0x70E
};
// Wild companion — intense eye pattern (variant 2)
static const uint16_t kSprite_Companion_v2[12] = {
  0x204, 0xF0F, 0xFFE, 0xFFF, 0xCF3, 0xFFF,
  0xBFD, 0xFFF, 0x7FE, 0x7FE, 0x606, 0x606
};

const uint16_t* getSpriteRows(PetStage stage, uint8_t variant = 0) {
  if (variant == 1) {
    if (stage == PetStage::YOUNGSTER) return kSprite_Youngster_v1;
    if (stage == PetStage::COMPANION) return kSprite_Companion_v1;
  } else if (variant == 2) {
    if (stage == PetStage::YOUNGSTER) return kSprite_Youngster_v2;
    if (stage == PetStage::COMPANION) return kSprite_Companion_v2;
  }
  switch (stage) {
    case PetStage::EGG:       return kSprite_Egg;
    case PetStage::HATCHLING: return kSprite_Hatchling;
    case PetStage::YOUNGSTER: return kSprite_Youngster;
    case PetStage::COMPANION: return kSprite_Companion;
    case PetStage::ELDER:     return kSprite_Elder;
    default:                  return kSprite_Dead;
  }
}
}  // namespace

// ---- Fallback renderer --------------------------------------------------

void PetSpriteRenderer::drawFallback(GfxRenderer& renderer, int x, int y, int scale,
                                     PetStage stage, uint8_t variant) {
  const int cell = 4 * scale;
  const uint16_t* rows = getSpriteRows(stage, variant);
  for (int row = 0; row < 12; row++) {
    uint16_t mask = rows[row];
    for (int col = 0; col < 12; col++) {
      if (mask & (1u << (11 - col))) {
        renderer.fillRect(x + col * cell, y + row * cell, cell, cell);
      }
    }
  }
}

// ---- Public API ---------------------------------------------------------

void PetSpriteRenderer::drawPet(GfxRenderer& renderer, int x, int y, PetStage stage,
                                 PetMood mood, int scale, uint8_t variant) {
  char path[80];
  // Try variant-specific file first
  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_v%d_%s.bin",
             stageName(stage), (int)variant, moodName(mood));
    if (loadSprite(path, SPRITE_BYTES) == SPRITE_BYTES && scale == 1) {
      renderer.drawImage(spriteBuffer, x, y, SPRITE_W, SPRITE_H);
      return;
    }
  }
  // Default path
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_%s.bin", stageName(stage),
           moodName(mood));
  if (loadSprite(path, SPRITE_BYTES) == SPRITE_BYTES && scale == 1) {
    renderer.drawImage(spriteBuffer, x, y, SPRITE_W, SPRITE_H);
  } else {
    drawFallback(renderer, x, y, scale, stage, variant);
  }
}

void PetSpriteRenderer::drawMini(GfxRenderer& renderer, int x, int y, PetStage stage,
                                  PetMood mood, uint8_t variant) {
  char path[88];
  // Try variant-specific mini file first
  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_v%d_%s.bin",
             stageName(stage), (int)variant, moodName(mood));
    if (loadSprite(path, MINI_BYTES) == MINI_BYTES) {
      renderer.drawImage(spriteBuffer, x, y, MINI_W, MINI_H);
      return;
    }
  }
  // Default mini path
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_%s.bin", stageName(stage),
           moodName(mood));
  if (loadSprite(path, MINI_BYTES) == MINI_BYTES) {
    renderer.drawImage(spriteBuffer, x, y, MINI_W, MINI_H);
  } else {
    drawFallback(renderer, x, y, /*scale=*/1, stage, variant);
  }
}
