# Phase 6: Sprite Assets & Polish

## Overview
- **Priority:** Medium (can use placeholders until ready)
- **Status:** Pending
- **Effort:** 2h
- **Depends on:** Phase 2 (sprite system)

## Description
Create actual 48×48 and 24×24 pixel art sprites for all pet stages and moods. Convert to raw 1-bit binary format. Package for SD card deployment.

## Sprite List (23 full + 11 mini)

### Full Sprites (48×48)
| Stage | Sprites |
|-------|---------|
| Egg | idle (egg shape), hatching (cracking egg) |
| Hatchling | idle, happy (bouncing), sad (droopy), eating, sleeping |
| Youngster | idle, happy, sad, eating, sleeping |
| Companion | idle, happy, sad, eating, sleeping |
| Elder | idle, happy, sad, eating, sleeping |
| Dead | grave (tombstone + flower) |

### Mini Sprites (24×24)
| Stage | Sprites |
|-------|---------|
| Egg | idle |
| Hatchling | sleeping, sad |
| Youngster | sleeping, sad |
| Companion | sleeping, sad |
| Elder | sleeping, sad |
| Dead | grave |

## Art Style Guidelines
- 1-bit only: pure black and white pixels, no gray
- Cute, round shapes — big eyes, small body
- Each evolution stage visibly larger/more detailed
- Egg: simple oval with crack patterns
- Hatchling: tiny blob with big eyes
- Youngster: small creature with limbs
- Companion: medium creature, expressive face
- Elder: distinguished creature, maybe with glasses/hat
- Grave: simple tombstone with R.I.P.

## Conversion Pipeline
1. Draw sprites in any pixel art editor (Aseprite, Piskel, or even MS Paint)
2. Export as 1-bit PNG (black & white)
3. Convert PNG → raw binary using Python script:
   ```python
   # convert_sprite.py: PNG → raw 1-bit binary
   from PIL import Image
   img = Image.open("sprite.png").convert("1")
   raw = bytes(img.tobytes())  # 1-bit packed MSB
   open("sprite.bin", "wb").write(raw)
   ```
4. Place `.bin` files in `.crosspoint/pet/sprites/` on SD card

## Alternative: PROGMEM Sprites
For frequently used sprites (egg, grave), could embed in firmware as PROGMEM arrays. Saves SD read latency.
- Only worthwhile for <5 sprites
- Decision: start with all on SD, optimize later if needed

## Todo
- [ ] Create pixel art for all 23 full sprites
- [ ] Create pixel art for all 11 mini sprites
- [ ] Write PNG → binary conversion script
- [ ] Test all sprites render correctly on device
- [ ] Iterate on art based on e-ink appearance

## Success Criteria
- All 34 sprites created and converted
- Sprites look charming on e-ink display
- Each evolution stage is visually distinct
- Mood states are clearly distinguishable
