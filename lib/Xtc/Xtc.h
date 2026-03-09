/**
 * Xtc.h
 *
 * Main XTC ebook class for CrossPoint Reader
 * Provides EPUB-like interface for XTC file handling
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Xtc/XtcParser.h"
#include "Xtc/XtcTypes.h"

/**
 * XTC Ebook Handler
 *
 * Handles XTC file loading, page access, and cover image generation.
 * Interface is designed to be similar to Epub class for easy integration.
 */
class Xtc {
  std::string filepath;
  std::string cachePath;
  std::unique_ptr<xtc::XtcParser> parser;
  bool loaded;

 public:
  explicit Xtc(std::string filepath, const std::string& cacheDir) : filepath(std::move(filepath)), loaded(false) {
    // Create cache key based on filepath (same as Epub)
    cachePath = cacheDir + "/xtc_" + std::to_string(std::hash<std::string>{}(this->filepath));
  }
  ~Xtc() = default;

  /**
   * Load XTC file
   * @return true on success
   */
  bool load();

  /**
   * Clear cached data
   * @return true on success
   */
  bool clearCache() const;

  /**
   * Setup cache directory
   */
  void setupCacheDir() const;

  // Path accessors
  const std::string& getCachePath() const { return cachePath; }
  const std::string& getPath() const { return filepath; }

  // Metadata
  std::string getTitle() const;
  std::string getAuthor() const;
  bool hasChapters() const;
  const std::vector<xtc::ChapterInfo>& getChapters() const;

  // Cover image support (for sleep screen)
  std::string getCoverBmpPath() const;
  bool generateCoverBmp() const;
  // Thumbnail support (for Continue Reading card)
  std::string getThumbBmpPath() const;
  std::string getThumbBmpPath(int height) const;
  bool generateThumbBmp(int height) const;

  // Page access
  uint32_t getPageCount() const;
  uint16_t getPageWidth() const;
  uint16_t getPageHeight() const;
  uint8_t getBitDepth() const;  // 1 = XTC (1-bit), 2 = XTCH (2-bit)

  /**
   * Load page bitmap data
   * @param pageIndex Page index (0-based)
   * @param buffer Output buffer
   * @param bufferSize Buffer size
   * @return Number of bytes read
   */
  size_t loadPage(uint32_t pageIndex, uint8_t* buffer, size_t bufferSize) const;

  /**
   * Load page with streaming callback
   * @param pageIndex Page index
   * @param callback Callback for each chunk
   * @param chunkSize Chunk size
   * @return Error code
   */
  xtc::XtcError loadPageStreaming(uint32_t pageIndex,
                                  std::function<void(const uint8_t* data, size_t size, size_t offset)> callback,
                                  size_t chunkSize = 1024) const;

  /**
   * Load a strip of columns from one bit-plane of a 2-bit page.
   * Memory-efficient alternative to loading the full 96KB page buffer.
   * Only valid for 2-bit (XTCH) files.
   *
   * @param pageIndex  Page index (0-based)
   * @param plane      0 = Bit1 plane, 1 = Bit2 plane
   * @param colStart   Starting file column index (0 = rightmost screen pixel)
   * @param colCount   Number of columns to read
   * @param colBytes   Bytes per column = (height+7)/8
   * @param buffer     Caller-provided buffer of at least colCount*colBytes bytes
   * @return true on success
   */
  bool loadPagePlaneStrip(uint32_t pageIndex, uint8_t plane, size_t colStart, size_t colCount,
                          size_t colBytes, uint8_t* buffer) const;

  // Progress calculation
  uint8_t calculateProgress(uint32_t currentPage) const;

  // Check if file is loaded
  bool isLoaded() const { return loaded; }

  // Error information
  xtc::XtcError getLastError() const;
};
