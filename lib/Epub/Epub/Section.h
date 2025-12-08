#pragma once
#include "Epub.h"

class Page;
class GfxRenderer;

class Section {
  Epub* epub;
  const int spineIndex;
  GfxRenderer& renderer;
  std::string cachePath;

  void writeCacheMetadata(int fontId, float lineCompression, int marginTop, int marginRight, int marginBottom,
                          int marginLeft) const;
  void onPageComplete(const Page* page);

 public:
  int pageCount = 0;
  int currentPage = 0;

  explicit Section(Epub* epub, const int spineIndex, GfxRenderer& renderer)
      : epub(epub), spineIndex(spineIndex), renderer(renderer) {
    cachePath = epub->getCachePath() + "/" + std::to_string(spineIndex);
  }
  ~Section() = default;
  bool loadCacheMetadata(int fontId, float lineCompression, int marginTop, int marginRight, int marginBottom,
                         int marginLeft);
  void setupCacheDir() const;
  void clearCache() const;
  bool persistPageDataToSD(int fontId, float lineCompression, int marginTop, int marginRight, int marginBottom,
                           int marginLeft);
  Page* loadPageFromSD() const;
};
