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

// ---- Fallback renderer --------------------------------------------------

void PetSpriteRenderer::drawFallback(GfxRenderer& renderer, int x, int y, int w, int h,
                                     PetStage stage) {
  // Outline rect
  renderer.drawRect(x, y, w, h);
  // Stage initial letter centered inside rect
  char label[2] = {stageInitial(stage), '\0'};
  const int cx = x + w / 2 - renderer.getTextWidth(UI_10_FONT_ID, label) / 2;
  const int cy = y + (h - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
  renderer.drawText(UI_10_FONT_ID, cx, cy, label);
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
