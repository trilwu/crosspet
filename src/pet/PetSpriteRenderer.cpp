#include "PetSpriteRenderer.h"

#include <HalStorage.h>

#include "PetSpriteData.h"
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
    // Attention moods map to neutral sprite; indicator drawn by caller
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
    case PetStage::ELDER:     return 'A';
    case PetStage::DEAD:      return 'X';
    default:                  return '?';
  }
}

// ---- File loading -------------------------------------------------------

size_t PetSpriteRenderer::loadSprite(const char* path, size_t expectedBytes) {
  size_t read = Storage.readFileToBuffer(path, reinterpret_cast<char*>(spriteBuffer),
                                         SPRITE_BYTES + 1, expectedBytes);
  return read;
}

// ---- Fallback renderer (pixel-art from PetSpriteData.h) -----------------

void PetSpriteRenderer::drawFallback(GfxRenderer& renderer, int x, int y, int scale,
                                     PetStage stage, uint8_t variant, uint8_t petType,
                                     uint8_t animFrame) {
  // 24x24 grid; each logical pixel = (2*scale) physical pixels
  const int cell = 2 * scale;
  const uint32_t* rows = getSpriteRows(stage, variant, petType, animFrame);
  for (int row = 0; row < 24; row++) {
    uint32_t mask = rows[row];
    for (int col = 0; col < 24; col++) {
      if (mask & (1u << (23 - col))) {
        renderer.fillRect(x + col * cell, y + row * cell, cell, cell);
      }
    }
  }
}

// ---- Public API ---------------------------------------------------------

void PetSpriteRenderer::drawPet(GfxRenderer& renderer, int x, int y, PetStage stage,
                                 PetMood mood, int scale, uint8_t variant, uint8_t petType,
                                 uint8_t animFrame) {
  char path[80];
  // SD card sprites are 48x48 binary — only used at scale==1, no animFrame
  if (variant > 0) {
    snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_v%d_%s.bin",
             stageName(stage), (int)variant, moodName(mood));
    if (loadSprite(path, SPRITE_BYTES) == SPRITE_BYTES && scale == 1) {
      renderer.drawImage(spriteBuffer, x, y, SPRITE_W, SPRITE_H);
      return;
    }
  }
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_%s.bin", stageName(stage),
           moodName(mood));
  if (loadSprite(path, SPRITE_BYTES) == SPRITE_BYTES && scale == 1) {
    renderer.drawImage(spriteBuffer, x, y, SPRITE_W, SPRITE_H);
  } else {
    drawFallback(renderer, x, y, scale, stage, variant, petType, animFrame);
  }
}

void PetSpriteRenderer::drawMini(GfxRenderer& renderer, int x, int y, PetStage stage,
                                  PetMood mood, uint8_t variant, uint8_t petType) {
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
  // Default mini SD card path
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_%s.bin", stageName(stage),
           moodName(mood));
  if (loadSprite(path, MINI_BYTES) == MINI_BYTES) {
    renderer.drawImage(spriteBuffer, x, y, MINI_W, MINI_H);
  } else {
    drawFallback(renderer, x, y, /*scale=*/1, stage, variant, petType, /*animFrame=*/0);
  }
}
