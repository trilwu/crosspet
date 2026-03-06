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

  // Draw sprite at (x,y). scale multiplies each logical pixel (default 1 = 48x48, 2 = 96x96).
  // variant selects evolution branch (0=default, 1=chubby, 2=misbehaved).
  // petType selects built-in pixel-art design (0=Chicken, 1=Cat, 2=Dog, 3=Dragon, 4=Bunny, 5=Robot, 6=Bear, 7=Slime).
  // Tries {stage}_v{variant}_{mood}.bin first, falls back to {stage}_{mood}.bin, then built-in art.
  static void drawPet(GfxRenderer& renderer, int x, int y, PetStage stage, PetMood mood,
                      int scale = 1, uint8_t variant = 0, uint8_t petType = 0,
                      uint8_t animFrame = 0);
  // Built-in grid is 24x24 logical pixels; each pixel renders as (2*scale) physical pixels.
  // At scale=3: 24*2*3 = 144px (same as old 12*4*3).
  static constexpr int BUILTIN_GRID = 24;
  static constexpr int displaySize(int scale = 1) { return BUILTIN_GRID * 2 * scale; }

  // Draw 24x24 mini sprite at (x,y). Falls back to pixel-art if file missing.
  static void drawMini(GfxRenderer& renderer, int x, int y, PetStage stage, PetMood mood,
                       uint8_t variant = 0, uint8_t petType = 0);

 private:
  // Shared 288-byte buffer — large enough for a full sprite frame.
  static uint8_t spriteBuffer[SPRITE_BYTES];

  static const char* stageName(PetStage stage);
  static const char* moodName(PetMood mood);
  static char stageInitial(PetStage stage);

  // Attempt to load sprite into spriteBuffer. Returns bytes read (0 on fail).
  static size_t loadSprite(const char* path, size_t expectedBytes);

  static void drawFallback(GfxRenderer& renderer, int x, int y, int scale, PetStage stage,
                           uint8_t variant = 0, uint8_t petType = 0, uint8_t animFrame = 0);
};
