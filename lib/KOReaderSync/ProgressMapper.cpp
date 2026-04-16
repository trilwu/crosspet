#include "ProgressMapper.h"

#include <Logging.h>

#include <cmath>
#include <cstdlib>

KOReaderPosition ProgressMapper::toKOReader(const std::shared_ptr<Epub>& epub, const CrossPointPosition& pos) {
  KOReaderPosition result;

  // Calculate page progress within current spine item
  float intraSpineProgress = 0.0f;
  if (pos.totalPages > 0) {
    intraSpineProgress = static_cast<float>(pos.pageNumber) / static_cast<float>(pos.totalPages);
  }

  // Calculate overall book progress (0.0-1.0)
  result.percentage = epub->calculateProgress(pos.spineIndex, intraSpineProgress);

  // Generate XPath with estimated paragraph position based on page
  result.xpath = generateXPath(pos.spineIndex, pos.pageNumber, pos.totalPages);

  // Get chapter info for logging
  const int tocIndex = epub->getTocIndexForSpineIndex(pos.spineIndex);
  const std::string chapterName = (tocIndex >= 0) ? epub->getTocItem(tocIndex).title : "unknown";

  LOG_DBG("ProgressMapper", "CrossPoint -> KOReader: chapter='%s', page=%d/%d -> %.2f%% at %s", chapterName.c_str(),
          pos.pageNumber, pos.totalPages, result.percentage * 100, result.xpath.c_str());

  return result;
}

CrossPointPosition ProgressMapper::toCrossPoint(const std::shared_ptr<Epub>& epub, const KOReaderPosition& koPos,
                                                int currentSpineIndex, int totalPagesInCurrentSpine) {
  CrossPointPosition result;
  result.spineIndex = 0;
  result.pageNumber = 0;
  result.totalPages = 0;

  const size_t bookSize = epub->getBookSize();
  if (bookSize == 0) {
    return result;
  }

  const int spineCount = epub->getSpineItemsCount();

  // Try to extract spine index from XPath (1-based DocFragment index)
  int xpathSpineIndex = -1;
  const char* dfTag = "DocFragment[";
  auto dfPos = koPos.xpath.find(dfTag);
  if (dfPos != std::string::npos) {
    int oneBasedIdx = std::atoi(koPos.xpath.c_str() + dfPos + strlen(dfTag));
    if (oneBasedIdx >= 1 && oneBasedIdx <= spineCount) {
      xpathSpineIndex = oneBasedIdx - 1;  // convert to 0-based
    }
  }

  // Use percentage to find spine item by byte position
  const size_t targetBytes = static_cast<size_t>(bookSize * koPos.percentage);
  int percentageSpineIndex = -1;
  for (int i = 0; i < spineCount; i++) {
    if (epub->getCumulativeSpineItemSize(i) >= targetBytes) {
      percentageSpineIndex = i;
      break;
    }
  }
  if (percentageSpineIndex < 0 && spineCount > 0) {
    percentageSpineIndex = spineCount - 1;
  }

  // Prefer XPath hint when available; fall back to percentage-based lookup.
  // XPath gives exact chapter, percentage gives position within chapter.
  if (xpathSpineIndex >= 0) {
    result.spineIndex = xpathSpineIndex;
  } else {
    result.spineIndex = (percentageSpineIndex >= 0) ? percentageSpineIndex : 0;
  }

  // Estimate page number within the spine item using percentage
  if (result.spineIndex < epub->getSpineItemsCount()) {
    const size_t prevCumSize = (result.spineIndex > 0) ? epub->getCumulativeSpineItemSize(result.spineIndex - 1) : 0;
    const size_t currentCumSize = epub->getCumulativeSpineItemSize(result.spineIndex);
    const size_t spineSize = currentCumSize - prevCumSize;

    int estimatedTotalPages = 0;

    // If we are in the same spine, use the known total pages
    if (result.spineIndex == currentSpineIndex && totalPagesInCurrentSpine > 0) {
      estimatedTotalPages = totalPagesInCurrentSpine;
    }
    // Otherwise try to estimate based on density from current spine
    else if (currentSpineIndex >= 0 && currentSpineIndex < epub->getSpineItemsCount() && totalPagesInCurrentSpine > 0) {
      const size_t prevCurrCumSize =
          (currentSpineIndex > 0) ? epub->getCumulativeSpineItemSize(currentSpineIndex - 1) : 0;
      const size_t currCumSize = epub->getCumulativeSpineItemSize(currentSpineIndex);
      const size_t currSpineSize = currCumSize - prevCurrCumSize;

      if (currSpineSize > 0) {
        float ratio = static_cast<float>(spineSize) / static_cast<float>(currSpineSize);
        estimatedTotalPages = static_cast<int>(totalPagesInCurrentSpine * ratio);
        if (estimatedTotalPages < 1) estimatedTotalPages = 1;
      }
    }

    result.totalPages = estimatedTotalPages;

    if (spineSize > 0 && estimatedTotalPages > 0) {
      const size_t bytesIntoSpine = (targetBytes > prevCumSize) ? (targetBytes - prevCumSize) : 0;
      const float intraSpineProgress = static_cast<float>(bytesIntoSpine) / static_cast<float>(spineSize);
      const float clampedProgress = std::max(0.0f, std::min(1.0f, intraSpineProgress));
      result.pageNumber = static_cast<int>(clampedProgress * estimatedTotalPages);
      result.pageNumber = std::max(0, std::min(result.pageNumber, estimatedTotalPages - 1));
    }
  }

  LOG_DBG("ProgressMapper", "KOReader -> CrossPoint: %.2f%% at %s -> spine=%d, page=%d", koPos.percentage * 100,
          koPos.xpath.c_str(), result.spineIndex, result.pageNumber);

  return result;
}

std::string ProgressMapper::generateXPath(int spineIndex, int pageNumber, int totalPages) {
  // KOReader uses 1-based DocFragment indices in XPath
  return "/body/DocFragment[" + std::to_string(spineIndex + 1) + "]/body";
}
