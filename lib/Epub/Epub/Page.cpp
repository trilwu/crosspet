#include "Page.h"

#include <HardwareSerial.h>
#include <Serialization.h>

constexpr uint8_t PAGE_FILE_VERSION = 1;

void PageLine::render(GfxRenderer& renderer, const int fontId) { block->render(renderer, fontId, xPos, yPos); }

void PageLine::serialize(std::ostream& os) {
  serialization::writePod(os, xPos);
  serialization::writePod(os, yPos);

  // serialize TextBlock pointed to by PageLine
  block->serialize(os);
}

PageLine* PageLine::deserialize(std::istream& is) {
  int32_t xPos;
  int32_t yPos;
  serialization::readPod(is, xPos);
  serialization::readPod(is, yPos);

  const auto tb = TextBlock::deserialize(is);
  return new PageLine(tb, xPos, yPos);
}

void Page::render(GfxRenderer& renderer, const int fontId) const {
  const auto start = millis();
  for (const auto element : elements) {
    element->render(renderer, fontId);
  }
  Serial.printf("Rendered page elements (%u) in %dms\n", elements.size(), millis() - start);
}

void Page::serialize(std::ostream& os) const {
  serialization::writePod(os, PAGE_FILE_VERSION);

  const uint32_t count = elements.size();
  serialization::writePod(os, count);

  for (auto* el : elements) {
    // Only PageLine exists currently
    serialization::writePod(os, static_cast<uint8_t>(TAG_PageLine));
    static_cast<PageLine*>(el)->serialize(os);
  }
}

Page* Page::deserialize(std::istream& is) {
  uint8_t version;
  serialization::readPod(is, version);
  if (version != PAGE_FILE_VERSION) {
    Serial.printf("Page: Unknown version %u\n", version);
    return nullptr;
  }

  auto* page = new Page();

  uint32_t count;
  serialization::readPod(is, count);

  for (uint32_t i = 0; i < count; i++) {
    uint8_t tag;
    serialization::readPod(is, tag);

    if (tag == TAG_PageLine) {
      auto* pl = PageLine::deserialize(is);
      page->elements.push_back(pl);
    } else {
      throw std::runtime_error("Unknown PageElement tag");
    }
  }

  return page;
}
