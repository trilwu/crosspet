#pragma once

#include <cstdint>
#include <string>

// POD struct for per-book reader settings, stored in .crosspoint/epub_<hash>/reader_settings.bin
// Captures the reader-relevant subset of CrossPointSettings for per-book overrides.
struct BookReaderSettings {
  static constexpr uint8_t VERSION = 1;

  uint8_t version = VERSION;
  uint8_t fontFamily;
  uint8_t fontSize;
  uint8_t lineSpacing;
  uint8_t paragraphAlignment;
  uint8_t extraParagraphSpacing;
  uint8_t textAntiAliasing;
  uint8_t hyphenationEnabled;
  uint8_t screenMargin;
  uint8_t embeddedStyle;
  uint8_t imageRendering;
  uint8_t _padding = 0;  // align to 12 bytes

  // Capture current global reader settings
  static BookReaderSettings fromGlobal();
  // Apply these settings to global SETTINGS
  void applyToGlobal() const;

  // Binary I/O
  static bool load(const std::string& path, BookReaderSettings& out);
  bool save(const std::string& path) const;
};
