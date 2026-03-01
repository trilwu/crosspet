#!/usr/bin/env python3
"""
CrossPet Sprite Generator — creates placeholder 1-bit raw binary sprites.

Output layout (copy contents of out/ to .crosspoint/pet/sprites/ on the SD card):
  out/{stage}_{mood}.bin      — 48x48 pixels, 288 bytes
  out/mini/{stage}_{mood}.bin — 24x24 pixels, 72 bytes

Usage:
  pip install Pillow
  python3 generate_pet_sprites.py

For real art: draw 48x48 1-bit PNGs in Aseprite/Piskel, then run:
  python3 generate_pet_sprites.py --from-png path/to/sprite.png out/stage_mood.bin
"""

import os
import sys
import struct
import math

# ─── Constants ────────────────────────────────────────────────────────────────
W, H = 48, 48
MW, MH = 24, 24

STAGES = ["egg", "hatchling", "youngster", "companion", "elder", "dead"]
MOODS  = ["happy", "neutral", "sad", "sick", "sleeping", "dead"]

# ─── Bitmap helpers ───────────────────────────────────────────────────────────

def empty_grid(w, h):
    return [[0] * w for _ in range(h)]

def set_pixel(grid, x, y, v=1):
    if 0 <= x < len(grid[0]) and 0 <= y < len(grid):
        grid[y][x] = v

def draw_circle(grid, cx, cy, r, fill=True, v=1):
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            dist = math.sqrt((x - cx)**2 + (y - cy)**2)
            if fill:
                if dist <= r:
                    set_pixel(grid, x, y, v)
            else:
                if r - 1 <= dist <= r:
                    set_pixel(grid, x, y, v)

def draw_rect(grid, x, y, w, h, fill=True, v=1):
    for ry in range(y, y + h):
        for rx in range(x, x + w):
            if fill:
                set_pixel(grid, rx, ry, v)
            elif rx == x or rx == x+w-1 or ry == y or ry == y+h-1:
                set_pixel(grid, rx, ry, v)

def draw_eyes(grid, cx, ey, spacing=6, radius=2):
    """Draw two filled eye circles."""
    draw_circle(grid, cx - spacing, ey, radius)
    draw_circle(grid, cx + spacing, ey, radius)

def draw_smile(grid, cx, sy, width=8):
    for dx in range(-width//2, width//2 + 1):
        y_off = int(3 * (dx / width)**2)
        set_pixel(grid, cx + dx, sy + y_off)

def draw_frown(grid, cx, sy, width=8):
    for dx in range(-width//2, width//2 + 1):
        y_off = -int(3 * (dx / width)**2)
        set_pixel(grid, cx + dx, sy + y_off)

def to_binary(grid, w, h):
    """Pack pixel grid into MSB-first 1-bit binary (same as drawImage expects)."""
    data = bytearray()
    bytes_per_row = (w + 7) // 8
    for row in grid:
        for byte_idx in range(bytes_per_row):
            byte = 0
            for bit in range(8):
                px = byte_idx * 8 + bit
                if px < w and row[px]:
                    byte |= (0x80 >> bit)
            data.append(byte)
    return bytes(data)

# ─── Shape builders ───────────────────────────────────────────────────────────

def make_egg(mood, w=W, h=H):
    g = empty_grid(w, h)
    cx, cy = w//2, h//2
    # Egg shape: ellipse (taller than wide)
    rx, ry = int(w * 0.38), int(h * 0.45)
    for y in range(h):
        for x in range(w):
            dx, dy = (x - cx) / rx, (y - cy - 2) / ry
            if dx*dx + dy*dy <= 1.0:
                g[y][x] = 1
    if mood == "hatching":  # crack lines
        for i in range(3):
            set_pixel(g, cx + i, cy - 4, 0)
        for i in range(2):
            set_pixel(g, cx - i, cy - 2, 0)
    return g

def make_hatchling(mood, w=W, h=H):
    g = empty_grid(w, h)
    cx, cy = w//2, int(h*0.55)
    r = int(w * 0.28)
    draw_circle(g, cx, cy, r)
    draw_eyes(g, cx, cy - r//2, spacing=4, radius=2)
    if mood == "happy":
        draw_smile(g, cx, cy + 3, width=6)
    elif mood in ("sad", "sick"):
        draw_frown(g, cx, cy + 5, width=6)
    # Tiny feet
    set_pixel(g, cx - 4, cy + r)
    set_pixel(g, cx - 4, cy + r + 1)
    set_pixel(g, cx + 4, cy + r)
    set_pixel(g, cx + 4, cy + r + 1)
    return g

def make_youngster(mood, w=W, h=H):
    g = empty_grid(w, h)
    cx, cy = w//2, int(h*0.48)
    r = int(w * 0.32)
    draw_circle(g, cx, cy, r)
    draw_eyes(g, cx, cy - r//3, spacing=5, radius=2)
    if mood == "happy":
        draw_smile(g, cx, cy + r//2, width=8)
    elif mood in ("sad", "sick"):
        draw_frown(g, cx, cy + r//2 + 2, width=8)
    # Arms
    draw_rect(g, cx - r - 3, cy - 2, 4, 4, fill=True)
    draw_rect(g, cx + r, cy - 2, 4, 4, fill=True)
    return g

def make_companion(mood, w=W, h=H):
    g = empty_grid(w, h)
    cx, cy = w//2, int(h*0.45)
    r = int(w * 0.36)
    draw_circle(g, cx, cy, r)
    draw_eyes(g, cx, cy - r//3, spacing=7, radius=3)
    if mood == "happy":
        draw_smile(g, cx, cy + r//2, width=12)
    elif mood in ("sad", "sick"):
        draw_frown(g, cx, cy + r//2 + 2, width=12)
    # Ears / horns
    draw_circle(g, cx - r + 2, cy - r + 2, 3)
    draw_circle(g, cx + r - 2, cy - r + 2, 3)
    return g

def make_elder(mood, w=W, h=H):
    g = empty_grid(w, h)
    cx, cy = w//2, int(h*0.44)
    r = int(w * 0.38)
    draw_circle(g, cx, cy, r)
    draw_eyes(g, cx, cy - r//4, spacing=8, radius=3)
    if mood == "happy":
        draw_smile(g, cx, cy + r//2, width=14)
    # Hat (distinguished elder)
    draw_rect(g, cx - r//2, cy - r - 7, r, 7)
    draw_rect(g, cx - r//2 - 3, cy - r - 1, r + 6, 3, fill=True)
    return g

def make_dead(w=W, h=H):
    """Tombstone shape."""
    g = empty_grid(w, h)
    # Tombstone body
    bx, by, bw, bh = w//2 - 10, 12, 20, 28
    draw_rect(g, bx, by, bw, bh, fill=False)
    # Rounded top (semicircle)
    draw_circle(g, w//2, by, 10, fill=False)
    # R.I.P. cross
    draw_rect(g, w//2 - 4, by + 8, 8, 2, fill=True)
    draw_rect(g, w//2 - 1, by + 5, 2, 8, fill=True)
    # Ground mound
    draw_rect(g, w//2 - 14, by + bh, 28, 3, fill=True)
    return g

# ─── Dispatch ─────────────────────────────────────────────────────────────────

BUILDERS = {
    "egg":       lambda mood: make_egg(mood),
    "hatchling": lambda mood: make_hatchling(mood),
    "youngster": lambda mood: make_youngster(mood),
    "companion": lambda mood: make_companion(mood),
    "elder":     lambda mood: make_elder(mood),
    "dead":      lambda mood: make_dead(),
}

def make_mini(stage, mood):
    """Generate a 24x24 sprite by scaling down the 48x48 version."""
    g48 = BUILDERS[stage](mood)
    g24 = empty_grid(MW, MH)
    for y in range(MH):
        for x in range(MW):
            # 2×2 block average
            block = [g48[y*2+dy][x*2+dx] for dy in range(2) for dx in range(2)]
            g24[y][x] = 1 if sum(block) >= 2 else 0
    return g24

def write_sprite(path, grid, w, h):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(to_binary(grid, w, h))
    print(f"  wrote {path} ({w}x{h})")

# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    out_dir = os.path.join(os.path.dirname(__file__), "out", "pet_sprites")
    print(f"Generating placeholder sprites → {out_dir}")

    for stage in STAGES:
        builder = BUILDERS[stage]
        for mood in MOODS:
            # Full 48x48 sprite
            grid = builder(mood)
            path = os.path.join(out_dir, f"{stage}_{mood}.bin")
            write_sprite(path, grid, W, H)
            # Mini 24x24 sprite (sleeping and sad only, plus dead)
            if mood in ("sleeping", "sad", "dead") or stage in ("egg", "dead"):
                mini = make_mini(stage, mood)
                mini_path = os.path.join(out_dir, "mini", f"{stage}_{mood}.bin")
                write_sprite(mini_path, mini, MW, MH)

    print(f"\nDone! Copy contents of {out_dir}/ to .crosspoint/pet/sprites/ on your SD card.")
    print("Replace .bin files with real pixel art using Aseprite → export as 1-bit PNG,")
    print("then re-run with: python3 generate_pet_sprites.py --from-png sprite.png out.bin")

if __name__ == "__main__":
    # Simple --from-png mode
    if len(sys.argv) == 4 and sys.argv[1] == "--from-png":
        try:
            from PIL import Image
            img = Image.open(sys.argv[2]).convert("1")
            w, h = img.size
            grid = [[1 if img.getpixel((x, y)) == 0 else 0 for x in range(w)] for y in range(h)]
            out = sys.argv[3]
            os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
            with open(out, "wb") as f:
                f.write(to_binary(grid, w, h))
            print(f"Converted {sys.argv[2]} → {out} ({w}x{h}, {w*h//8} bytes)")
        except ImportError:
            print("ERROR: pip install Pillow")
            sys.exit(1)
    else:
        main()
