#pragma once
#include <EpdFontFamily.h>
#include <HalStorage.h>

#include <memory>
#include <string>
#include <vector>

#include "Block.h"
#include "BlockStyle.h"

// Represents a line of text on a page
class TextBlock final : public Block {
 private:
  std::vector<std::string> words;
  std::vector<int16_t> wordXpos;
  std::vector<EpdFontFamily::Style> wordStyles;
  BlockStyle blockStyle;

 public:
  explicit TextBlock(std::vector<std::string> words, std::vector<int16_t> word_xpos,
                     std::vector<EpdFontFamily::Style> word_styles, const BlockStyle& blockStyle = BlockStyle())
      : words(std::move(words)),
        wordXpos(std::move(word_xpos)),
        wordStyles(std::move(word_styles)),
        blockStyle(blockStyle) {}
  ~TextBlock() override = default;
  void setBlockStyle(const BlockStyle& blockStyle) { this->blockStyle = blockStyle; }
  const BlockStyle& getBlockStyle() const { return blockStyle; }
  const std::vector<std::string>& getWords() const { return words; }
  bool isEmpty() override { return words.empty(); }
  size_t wordCount() const { return words.size(); }

  // --- Compatibility shim for the CrossPoint 1.5.0 dictionary word selector ---
  // CrossPoint 1.5.0 rewrote TextBlock onto a flat arena and exposes per-word
  // accessors instead of raw vectors. CrossPet still uses the vector layout, so
  // these forward to it. Semantics match upstream:
  //   - wordText(i) returns a NUL-terminated pointer valid as long as this block
  //     is alive (guaranteed here by std::string's own storage), which is what
  //     DictionaryWordSelectActivity::WordBox::text relies on.
  //   - valid() is always true: the vector layout has no arena-OOM failure mode.
  //   - getRubyShift() returns 0: CrossPet has no ruby (CJK furigana) support.
  bool valid() const { return true; }
  const char* wordText(const uint16_t i) const { return words[i].c_str(); }
  uint16_t wordTextLen(const uint16_t i) const { return static_cast<uint16_t>(words[i].size()); }
  int16_t wordXposAt(const uint16_t i) const { return wordXpos[i]; }
  EpdFontFamily::Style wordStyle(const uint16_t i) const { return wordStyles[i]; }
  bool hasRuby() const { return false; }
  int getRubyShift(int) const { return 0; }
  // given a renderer works out where to break the words into lines
  void render(const GfxRenderer& renderer, int fontId, int x, int y) const;
  BlockType getType() override { return TEXT_BLOCK; }
  bool serialize(FsFile& file) const;
  static std::unique_ptr<TextBlock> deserialize(FsFile& file);
};
