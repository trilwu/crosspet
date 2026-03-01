#pragma once

#include <GfxRenderer.h>

#include "PetState.h"

// Renders pet sprites loaded from SD card at /.crosspoint/pet/sprites/
// Sprite format: raw 1-bit packed bitmap, MSB first per row, no header.
// Full sprites: 48x48 px = 288 bytes/frame
// Mini sprites: 24x24 px =  72 bytes/frame  (subfolder: mini/)
// Filename pattern: {stage}_{mood}.bin  e.g. hatchling_happy.bin
// Fallback: filled rect with stage initial letter when file is missing.
class PetSpriteRenderer {
 public:
  static constexpr int SPRITE_W    = 48;
  static constexpr int SPRITE_H    = 48;
  static constexpr int MINI_W      = 24;
  static constexpr int MINI_H      = 24;
  static constexpr int SPRITE_BYTES = SPRITE_W * SPRITE_H / 8;  // 288
  static constexpr int MINI_BYTES   = MINI_W * MINI_H / 8;       // 72

  // Draw 48x48 sprite at (x,y). Falls back to labelled rect if file missing.
  static void drawPet(GfxRenderer& renderer, int x, int y, PetStage stage, PetMood mood);

  // Draw 24x24 mini sprite at (x,y). Falls back to labelled rect if file missing.
  static void drawMini(GfxRenderer& renderer, int x, int y, PetStage stage, PetMood mood);

 private:
  // Shared 288-byte buffer — large enough for a full sprite frame.
  static uint8_t spriteBuffer[SPRITE_BYTES];

  static const char* stageName(PetStage stage);
  static const char* moodName(PetMood mood);
  static char stageInitial(PetStage stage);

  // Attempt to load sprite into spriteBuffer. Returns bytes read (0 on fail).
  static size_t loadSprite(const char* path, size_t expectedBytes);

  // Draw fallback rect + initial letter
  static void drawFallback(GfxRenderer& renderer, int x, int y, int w, int h, PetStage stage);
};
