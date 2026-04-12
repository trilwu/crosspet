#!/bin/bash

set -e

cd "$(dirname "$0")"

READER_FONT_STYLES=("Regular" "Italic" "Bold" "BoldItalic")
BOOKERLY_FONT_SIZES=(12 14 16 18)
LEXEND_FONT_SIZES=(12 14 16 18)
LEXEND_FONT_STYLES=("Regular" "Bold")
# Lexend uses variable font: Regular=350 (balanced between Light/Regular), Bold=700
LEXEND_STYLE_WEIGHTS=(350 700)
for size in ${BOOKERLY_FONT_SIZES[@]}; do
  for style in ${READER_FONT_STYLES[@]}; do
    font_name="bookerly_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/Bookerly/Bookerly-${style}.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path --2bit --compress > $output_path
    echo "Generated $output_path"
  done
done

for size in ${LEXEND_FONT_SIZES[@]}; do
  for i in "${!LEXEND_FONT_STYLES[@]}"; do
    style="${LEXEND_FONT_STYLES[$i]}"
    weight="${LEXEND_STYLE_WEIGHTS[$i]}"
    font_name="lexend_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/Lexend/Lexend-Variable.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path --2bit --compress --weight $weight > $output_path
    echo "Generated $output_path (weight ${weight})"
  done
done

# Bokerlam UI fonts (Vietnamese-optimized)
UI_FONT_STYLES=("Regular" "Bold")
for style in ${UI_FONT_STYLES[@]}; do
  font_name="bokerlam_12_$(echo $style | tr '[:upper:]' '[:lower:]')"
  font_path="../builtinFonts/source/Bokerlam/Bokerlam-${style}.ttf"
  output_path="../builtinFonts/${font_name}.h"
  python fontconvert.py $font_name 12 $font_path --2bit --compress > $output_path
  echo "Generated $output_path"
done

python fontconvert.py notosans_8_regular 8 ../builtinFonts/source/NotoSans/NotoSans-Regular.ttf > ../builtinFonts/notosans_8_regular.h

echo ""
echo "Running compression verification..."
python verify_compression.py ../builtinFonts/
