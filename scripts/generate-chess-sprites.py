#!/usr/bin/env python3
"""Generate monochrome 48x48 chess piece sprites as C byte arrays.

Draws pieces using Pillow primitives. Output: ChessPieceSprites.h
E-ink format: 1=white(off), 0=black(on). MSB first, 6 bytes per row.
drawImageTransparent: only draws black(0) pixels, white(1) = transparent.

White pieces: outline (border black, fill white)
Black pieces: filled solid (entire shape black)
"""

from PIL import Image, ImageDraw
import os

SIZE = 48
# Draw slightly smaller for padding
PAD = 4

def draw_king(draw, filled):
    """King: cross on top, body trapezoid, wide base."""
    cx = SIZE // 2
    # Cross on top
    cross_y = PAD + 2
    cross_h = 8
    cross_w = 4
    arm_w = 12
    arm_h = 3
    # Vertical bar
    draw.rectangle([cx - cross_w//2, cross_y, cx + cross_w//2, cross_y + cross_h], fill=0, outline=0)
    # Horizontal bar
    draw.rectangle([cx - arm_w//2, cross_y + 2, cx + arm_w//2, cross_y + 2 + arm_h], fill=0, outline=0)

    # Head circle
    head_y = cross_y + cross_h + 1
    head_r = 6
    draw.ellipse([cx - head_r, head_y, cx + head_r, head_y + head_r*2],
                 fill=0 if filled else 1, outline=0, width=2)

    # Body trapezoid
    body_top = head_y + head_r*2
    body_bot = SIZE - PAD - 4
    top_w = 10
    bot_w = 16
    body = [(cx - top_w, body_top), (cx + top_w, body_top),
            (cx + bot_w, body_bot), (cx - bot_w, body_bot)]
    draw.polygon(body, fill=0 if filled else 1, outline=0, width=2)

    # Base
    base_y = body_bot
    base_h = 4
    draw.rectangle([cx - bot_w - 2, base_y, cx + bot_w + 2, base_y + base_h], fill=0, outline=0)


def draw_queen(draw, filled):
    """Queen: crown with 3 points, body, wide base."""
    cx = SIZE // 2
    # Crown points
    crown_top = PAD + 1
    crown_bot = PAD + 14
    # 5 points crown
    pts = []
    for i in range(5):
        px = cx - 14 + i * 7
        if i % 2 == 0:
            pts.append((px, crown_top))
        else:
            pts.append((px, crown_top + 6))
    # Add bottom corners to close
    pts.append((cx + 14, crown_bot))
    pts.append((cx - 14, crown_bot))
    draw.polygon(pts, fill=0 if filled else 1, outline=0, width=2)

    # Small circles on crown tips
    for i in range(0, 5, 2):
        px = cx - 14 + i * 7
        draw.ellipse([px - 2, crown_top - 2, px + 2, crown_top + 2], fill=0, outline=0)

    # Body
    body_top = crown_bot
    body_bot = SIZE - PAD - 4
    top_w = 12
    bot_w = 16
    body = [(cx - top_w, body_top), (cx + top_w, body_top),
            (cx + bot_w, body_bot), (cx - bot_w, body_bot)]
    draw.polygon(body, fill=0 if filled else 1, outline=0, width=2)

    # Base
    draw.rectangle([cx - bot_w - 2, body_bot, cx + bot_w + 2, body_bot + 4], fill=0, outline=0)


def draw_rook(draw, filled):
    """Rook: castle turret top, rectangular body, base."""
    cx = SIZE // 2
    # Turret - 3 merlons
    turret_top = PAD + 2
    turret_bot = PAD + 10
    merlon_w = 5
    gap = 3
    total_w = 3 * merlon_w + 2 * gap
    start_x = cx - total_w // 2

    for i in range(3):
        mx = start_x + i * (merlon_w + gap)
        draw.rectangle([mx, turret_top, mx + merlon_w, turret_bot], fill=0 if filled else 1, outline=0, width=2)

    # Connecting wall below merlons
    draw.rectangle([start_x, turret_bot, start_x + total_w, turret_bot + 3], fill=0, outline=0)

    # Body
    body_top = turret_bot + 3
    body_bot = SIZE - PAD - 4
    bw = 10
    body = [(cx - bw, body_top), (cx + bw, body_top),
            (cx + bw + 3, body_bot), (cx - bw - 3, body_bot)]
    draw.polygon(body, fill=0 if filled else 1, outline=0, width=2)

    # Base
    draw.rectangle([cx - bw - 5, body_bot, cx + bw + 5, body_bot + 4], fill=0, outline=0)


def draw_bishop(draw, filled):
    """Bishop: pointed hat with slit, body, base."""
    cx = SIZE // 2
    # Pointed hat (teardrop shape)
    hat_top = PAD + 1
    hat_bot = PAD + 20
    hat_w = 10

    # Small ball on top
    draw.ellipse([cx - 3, hat_top, cx + 3, hat_top + 5], fill=0, outline=0)

    # Hat body (diamond/pointed oval)
    hat_pts = [(cx, hat_top + 3), (cx + hat_w, hat_bot - 4),
               (cx, hat_bot), (cx - hat_w, hat_bot - 4)]
    draw.polygon(hat_pts, fill=0 if filled else 1, outline=0, width=2)

    # Diagonal slit on hat
    if not filled:
        draw.line([cx - 4, hat_bot - 8, cx + 4, hat_bot - 14], fill=0, width=2)

    # Collar
    collar_y = hat_bot
    draw.ellipse([cx - 12, collar_y - 2, cx + 12, collar_y + 4], fill=0 if filled else 1, outline=0, width=2)

    # Body
    body_top = collar_y + 3
    body_bot = SIZE - PAD - 4
    body = [(cx - 8, body_top), (cx + 8, body_top),
            (cx + 14, body_bot), (cx - 14, body_bot)]
    draw.polygon(body, fill=0 if filled else 1, outline=0, width=2)

    # Base
    draw.rectangle([cx - 16, body_bot, cx + 16, body_bot + 4], fill=0, outline=0)


def draw_knight(draw, filled):
    """Knight: horse head profile facing left."""
    cx = SIZE // 2
    # Horse head profile - simplified polygon
    head = [
        (cx - 8, PAD + 2),      # top of head
        (cx - 2, PAD + 1),      # forehead
        (cx + 8, PAD + 4),      # ear tip
        (cx + 6, PAD + 10),     # behind ear
        (cx + 10, PAD + 14),    # back of head
        (cx + 10, PAD + 24),    # back of neck
        (cx + 6, PAD + 30),     # lower neck
        (cx + 14, SIZE - PAD - 4),  # body right
        (cx - 14, SIZE - PAD - 4),  # body left
        (cx - 14, PAD + 22),    # chest
        (cx - 16, PAD + 16),    # chin
        (cx - 14, PAD + 10),    # jaw
        (cx - 12, PAD + 6),     # nose
    ]
    draw.polygon(head, fill=0 if filled else 1, outline=0, width=2)

    # Eye
    eye_x = cx - 2
    eye_y = PAD + 10
    if filled:
        draw.ellipse([eye_x - 2, eye_y - 2, eye_x + 2, eye_y + 2], fill=1, outline=1)
    else:
        draw.ellipse([eye_x - 2, eye_y - 2, eye_x + 2, eye_y + 2], fill=0, outline=0)

    # Nostril
    draw.ellipse([cx - 13, PAD + 10, cx - 10, PAD + 13], fill=0 if not filled else 1)

    # Base
    draw.rectangle([cx - 16, SIZE - PAD - 4, cx + 16, SIZE - PAD], fill=0, outline=0)


def draw_pawn(draw, filled):
    """Pawn: small ball, body cone, base."""
    cx = SIZE // 2
    # Head circle
    head_r = 7
    head_y = PAD + 4
    draw.ellipse([cx - head_r, head_y, cx + head_r, head_y + head_r*2],
                 fill=0 if filled else 1, outline=0, width=2)

    # Neck
    neck_y = head_y + head_r * 2
    draw.rectangle([cx - 4, neck_y, cx + 4, neck_y + 3], fill=0, outline=0)

    # Body cone
    body_top = neck_y + 3
    body_bot = SIZE - PAD - 4
    body = [(cx - 6, body_top), (cx + 6, body_top),
            (cx + 14, body_bot), (cx - 14, body_bot)]
    draw.polygon(body, fill=0 if filled else 1, outline=0, width=2)

    # Base
    draw.rectangle([cx - 16, body_bot, cx + 16, body_bot + 4], fill=0, outline=0)


PIECE_DRAWERS = {
    'K': draw_king,
    'Q': draw_queen,
    'R': draw_rook,
    'B': draw_bishop,
    'N': draw_knight,
    'P': draw_pawn,
}

PIECE_NAMES = {
    'K': 'KING',
    'Q': 'QUEEN',
    'R': 'ROOK',
    'B': 'BISHOP',
    'N': 'KNIGHT',
    'P': 'PAWN',
}


def image_to_bytes(img):
    """Convert 1-bit image to byte array. MSB first, 1=white, 0=black."""
    pixels = img.load()
    data = []
    for y in range(SIZE):
        for x_byte in range(SIZE // 8):
            byte = 0
            for bit in range(8):
                x = x_byte * 8 + bit
                # Pillow "1" mode: 0=black, 255=white
                if pixels[x, y] != 0:
                    byte |= (1 << (7 - bit))  # MSB first, 1=white
            data.append(byte)
    return data


def generate_header(output_path):
    """Generate ChessPieceSprites.h with all 12 piece sprites."""
    arrays = {}

    # Generate preview directory
    preview_dir = os.path.join(os.path.dirname(output_path), '..', 'scripts', 'chess_preview')
    os.makedirs(preview_dir, exist_ok=True)

    for piece_char, drawer in PIECE_DRAWERS.items():
        for color in ['W', 'B']:
            filled = (color == 'B')
            # Create white background image
            img = Image.new('1', (SIZE, SIZE), 1)  # 1 = white
            draw = ImageDraw.Draw(img)
            drawer(draw, filled)

            name = f"{color}_{PIECE_NAMES[piece_char]}"
            arrays[name] = image_to_bytes(img)

            # Save preview PNG
            preview = img.resize((SIZE * 4, SIZE * 4), Image.NEAREST)
            preview.save(os.path.join(preview_dir, f"{name}.png"))

    # Write header file
    with open(output_path, 'w') as f:
        f.write('#pragma once\n\n')
        f.write('#include <cstdint>\n\n')
        f.write('// Auto-generated chess piece sprites (48x48 monochrome)\n')
        f.write('// Format: 1=white(transparent), 0=black(drawn). MSB first, 6 bytes/row.\n')
        f.write(f'// Total: {len(arrays)} sprites x {SIZE*SIZE//8} bytes = {len(arrays) * SIZE*SIZE//8} bytes\n\n')

        for name, data in arrays.items():
            f.write(f'static const uint8_t CHESS_{name}[] = {{\n')
            for row in range(SIZE):
                offset = row * (SIZE // 8)
                row_bytes = data[offset:offset + SIZE // 8]
                hex_str = ', '.join(f'0x{b:02X}' for b in row_bytes)
                f.write(f'  {hex_str},\n')
            f.write('};\n\n')

        # Lookup function using ChessActivity piece constants
        # Piece values: PAWN=1, KNIGHT=2, BISHOP=3, ROOK=4, QUEEN=5, KING=6
        # Positive=white, Negative=black
        f.write('// Lookup sprite by piece value (positive=white, negative=black)\n')
        f.write('// Piece types: 1=Pawn, 2=Knight, 3=Bishop, 4=Rook, 5=Queen, 6=King\n')
        f.write('inline const uint8_t* getChessPieceSprite(int8_t piece) {\n')
        f.write('  if (piece == 0) return nullptr;\n')
        f.write('  static const uint8_t* const WHITE_PIECES[] = {\n')
        f.write('    nullptr,         // 0: empty\n')
        f.write('    CHESS_W_PAWN,    // 1: pawn\n')
        f.write('    CHESS_W_KNIGHT,  // 2: knight\n')
        f.write('    CHESS_W_BISHOP,  // 3: bishop\n')
        f.write('    CHESS_W_ROOK,    // 4: rook\n')
        f.write('    CHESS_W_QUEEN,   // 5: queen\n')
        f.write('    CHESS_W_KING,    // 6: king\n')
        f.write('  };\n')
        f.write('  static const uint8_t* const BLACK_PIECES[] = {\n')
        f.write('    nullptr,         // 0: empty\n')
        f.write('    CHESS_B_PAWN,    // 1: pawn\n')
        f.write('    CHESS_B_KNIGHT,  // 2: knight\n')
        f.write('    CHESS_B_BISHOP,  // 3: bishop\n')
        f.write('    CHESS_B_ROOK,    // 4: rook\n')
        f.write('    CHESS_B_QUEEN,   // 5: queen\n')
        f.write('    CHESS_B_KING,    // 6: king\n')
        f.write('  };\n')
        f.write('  int8_t type = piece > 0 ? piece : -piece;\n')
        f.write('  if (type < 1 || type > 6) return nullptr;\n')
        f.write('  return piece > 0 ? WHITE_PIECES[type] : BLACK_PIECES[type];\n')
        f.write('}\n')

    print(f"Generated {output_path}")
    print(f"  {len(arrays)} sprites, {len(arrays) * SIZE*SIZE//8} bytes total")
    print(f"  Previews saved to {preview_dir}/")


if __name__ == '__main__':
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    output = os.path.join(project_dir, 'src', 'activities', 'tools', 'ChessPieceSprites.h')
    generate_header(output)
