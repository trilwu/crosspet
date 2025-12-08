#pragma once
#include "blocks/TextBlock.h"

enum PageElementTag : uint8_t {
  TAG_PageLine = 1,
};

// represents something that has been added to a page
class PageElement {
 public:
  int xPos;
  int yPos;
  explicit PageElement(const int xPos, const int yPos) : xPos(xPos), yPos(yPos) {}
  virtual ~PageElement() = default;
  virtual void render(GfxRenderer& renderer, int fontId) = 0;
  virtual void serialize(std::ostream& os) = 0;
};

// a line from a block element
class PageLine final : public PageElement {
  const TextBlock* block;

 public:
  PageLine(const TextBlock* block, const int xPos, const int yPos) : PageElement(xPos, yPos), block(block) {}
  ~PageLine() override { delete block; }
  void render(GfxRenderer& renderer, int fontId) override;
  void serialize(std::ostream& os) override;
  static PageLine* deserialize(std::istream& is);
};

class Page {
 public:
  ~Page() {
    for (const auto element : elements) {
      delete element;
    }
  }

  // the list of block index and line numbers on this page
  std::vector<PageElement*> elements;
  void render(GfxRenderer& renderer, int fontId) const;
  void serialize(std::ostream& os) const;
  static Page* deserialize(std::istream& is);
};
