#!/usr/bin/env python3
"""
Convert a TTF/OTF font to CrossPoint .bin external font format.

The .bin format uses direct Unicode codepoint indexing:
  offset = codepoint * bytes_per_char
  Each glyph = bytes_per_row * char_height bytes, 1-bit bitmap MSB-first

Output filename: FontName_size_WxH.bin
  size = point size, W = pixel width, H = pixel height

Usage:
    python3 generate_external_font.py MyFont.ttf --size 24
    python3 generate_external_font.py MyFont.ttf --size 24 --max-codepoint 0x9FFF
    python3 generate_external_font.py MyFont.ttf --size 24 --latin-only

The generated .bin file goes in /fonts/ on the SD card.
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Error: Pillow not installed. Run: pip3 install Pillow")
    sys.exit(1)


def generate_bin_font(font_path, pt_size, max_codepoint, output_dir):
    """Generate a .bin font file from a TTF/OTF font."""

    try:
        font = ImageFont.truetype(font_path, pt_size)
    except Exception as e:
        print(f"Error loading font: {e}")
        return None

    ascent, descent = font.getmetrics()
    font_height = ascent + descent

    # Determine cell size: width = max advance of sample chars, height = ascent + descent
    # For CJK fonts, width ~= height (square cells). For Latin, width < height.
    sample_cjk = "国字體"
    sample_latin = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    max_width = 0

    for ch in sample_cjk + sample_latin:
        try:
            bbox = font.getbbox(ch)
            if bbox:
                w = bbox[2] - bbox[0]
                if w > max_width:
                    max_width = w
        except:
            pass

    # Cell dimensions — add 1px padding
    cell_w = max_width + 2
    cell_h = font_height + 2
    baseline = ascent + 1  # 1px top padding

    bytes_per_row = (cell_w + 7) // 8
    bytes_per_char = bytes_per_row * cell_h

    # Check glyph size limit (ExternalFont::MAX_GLYPH_BYTES = 260)
    if bytes_per_char > 260:
        print(f"Warning: glyph size {bytes_per_char} bytes exceeds 260-byte limit.")
        print(f"  Reduce --size or the font will be marked [!] in the picker.")

    # Extract font name from filename
    font_name = Path(font_path).stem.replace(" ", "").replace("-", "")
    # Truncate to fit FontInfo::name[32]
    if len(font_name) > 28:
        font_name = font_name[:28]

    output_filename = f"{font_name}_{pt_size}_{cell_w}x{cell_h}.bin"
    output_path = Path(output_dir) / output_filename

    print(f"Font: {font_name}")
    print(f"Size: {pt_size}pt, Cell: {cell_w}x{cell_h}, {bytes_per_char} bytes/glyph")
    print(f"Max codepoint: U+{max_codepoint:04X}")
    print(f"Output: {output_path}")
    print(f"File size: {(max_codepoint + 1) * bytes_per_char / 1024 / 1024:.1f} MB")
    print()

    # Generate the .bin file
    # Direct codepoint indexing: write bytes_per_char for every codepoint 0..max_codepoint
    empty_glyph = bytes(bytes_per_char)

    written = 0
    rendered = 0

    with open(output_path, "wb") as f:
        for cp in range(max_codepoint + 1):
            if cp % 10000 == 0 and cp > 0:
                print(f"  U+{cp:04X} ({rendered} glyphs rendered)...", flush=True)

            ch = chr(cp)

            # Check if font has this glyph
            try:
                bbox = font.getbbox(ch)
                has_glyph = bbox is not None and (bbox[2] - bbox[0]) > 0
            except:
                has_glyph = False

            if not has_glyph or cp < 0x20:
                f.write(empty_glyph)
                written += 1
                continue

            # Render glyph
            img = Image.new("1", (cell_w, cell_h), 0)
            draw = ImageDraw.Draw(img)

            try:
                draw.text((1, baseline), ch, font=font, fill=1, anchor="ls")
            except TypeError:
                draw.text((1, 1), ch, font=font, fill=1)

            # Convert to bytes (MSB-first, 1-bit)
            glyph_bytes = bytearray(bytes_per_char)
            for row in range(cell_h):
                for byte_idx in range(bytes_per_row):
                    byte_val = 0
                    for bit in range(8):
                        px = byte_idx * 8 + bit
                        if px < cell_w and img.getpixel((px, row)):
                            byte_val |= 1 << (7 - bit)
                    glyph_bytes[row * bytes_per_row + byte_idx] = byte_val

            f.write(glyph_bytes)
            written += 1
            rendered += 1

    print(f"\nDone! {rendered} glyphs rendered, {written} total slots.")
    print(f"Copy {output_path} to /fonts/ on the SD card.")
    return output_path


def main():
    parser = argparse.ArgumentParser(
        description="Convert TTF/OTF font to CrossPoint .bin external font format"
    )
    parser.add_argument("font", help="Path to TTF/OTF font file")
    parser.add_argument(
        "--size", type=int, default=24, help="Point size (default: 24)"
    )
    parser.add_argument(
        "--max-codepoint",
        type=lambda x: int(x, 0),
        default=None,
        help="Max Unicode codepoint (default: 0x9FFF for CJK, 0x024F for latin-only)",
    )
    parser.add_argument(
        "--latin-only",
        action="store_true",
        help="Latin + Vietnamese + common symbols only (small file, ~150KB)",
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default=".",
        help="Output directory (default: current dir)",
    )

    args = parser.parse_args()

    if not Path(args.font).exists():
        print(f"Error: Font file not found: {args.font}")
        sys.exit(1)

    if args.latin_only:
        max_cp = 0x024F  # Latin Extended-B — covers Vietnamese
    elif args.max_codepoint:
        max_cp = args.max_codepoint
    else:
        max_cp = 0x9FFF  # CJK Unified Ideographs end

    generate_bin_font(args.font, args.size, max_cp, args.output_dir)


if __name__ == "__main__":
    main()
