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

// ---- Pixel-art fallback sprites (12x12 logical grid → 4px per cell = 48x48) ----
// Encoding: bit 11 = col 0 (left), bit 0 = col 11 (right)
namespace {
// Cute egg with smiley face
static const uint16_t kSprite_Egg[12] = {
  0x1F8, 0x204, 0x402, 0x801, 0xA09, 0x801, 0x8F1, 0x801, 0x402, 0x204, 0x1F8, 0x000
};
// Fluffy hatchling chick with tiny beak
static const uint16_t kSprite_Hatchling[12] = {
  0x0F0, 0x3FC, 0x50A, 0x4F2, 0x3FC, 0x7FE, 0x7FE, 0x3FC, 0x204, 0x70E, 0x30C, 0x000
};
// Rounder youngster with bigger eyes
static const uint16_t kSprite_Youngster[12] = {
  0x3FC, 0x7FE, 0xA05, 0x9F9, 0x7FE, 0xFFF, 0xFFF, 0x7FE, 0x8F1, 0x3FC, 0x30C, 0x30C
};
// Companion — round body, full smile
static const uint16_t kSprite_Companion[12] = {
  0x3FC, 0x7FE, 0xA05, 0x801, 0x9F9, 0x7FE, 0xFFF, 0xFFF, 0x3FC, 0x3FC, 0x30C, 0x1F0
};
// Elder — crowned head, regal expression
static const uint16_t kSprite_Elder[12] = {
  0x0C0, 0x1E0, 0x3FC, 0x7FE, 0xA05, 0x801, 0xBFD, 0x7FE, 0xFFF, 0xBFD, 0x30C, 0x79E
};
// Dead — diagonal X pattern
static const uint16_t kSprite_Dead[12] = {
  0x801, 0x402, 0x204, 0x108, 0x090, 0x060, 0x060, 0x090, 0x108, 0x204, 0x402, 0x801
};

const uint16_t* getSpriteRows(PetStage stage) {
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

void PetSpriteRenderer::drawFallback(GfxRenderer& renderer, int x, int y, int w, int h,
                                     PetStage stage) {
  // Scale: each logical pixel = cell size (supports both 48x48 and 24x24)
  const int cell = w / 12;  // 4 for full, 2 for mini
  const uint16_t* rows = getSpriteRows(stage);
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
                                 PetMood mood) {
  char path[64];
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/%s_%s.bin", stageName(stage),
           moodName(mood));

  const size_t read = loadSprite(path, SPRITE_BYTES);
  if (read == SPRITE_BYTES) {
    renderer.drawImage(spriteBuffer, x, y, SPRITE_W, SPRITE_H);
  } else {
    drawFallback(renderer, x, y, SPRITE_W, SPRITE_H, stage);
  }
}

void PetSpriteRenderer::drawMini(GfxRenderer& renderer, int x, int y, PetStage stage,
                                  PetMood mood) {
  char path[72];
  snprintf(path, sizeof(path), "/.crosspoint/pet/sprites/mini/%s_%s.bin", stageName(stage),
           moodName(mood));

  const size_t read = loadSprite(path, MINI_BYTES);
  if (read == MINI_BYTES) {
    renderer.drawImage(spriteBuffer, x, y, MINI_W, MINI_H);
  } else {
    drawFallback(renderer, x, y, MINI_W, MINI_H, stage);
  }
}
