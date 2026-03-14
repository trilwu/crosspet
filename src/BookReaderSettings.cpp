#include "BookReaderSettings.h"

#include <HalStorage.h>
#include <Logging.h>

#include "CrossPointSettings.h"

BookReaderSettings BookReaderSettings::fromGlobal() {
  BookReaderSettings s;
  s.fontFamily = SETTINGS.fontFamily;
  s.fontSize = SETTINGS.fontSize;
  s.lineSpacing = SETTINGS.lineSpacing;
  s.paragraphAlignment = SETTINGS.paragraphAlignment;
  s.extraParagraphSpacing = SETTINGS.extraParagraphSpacing;
  s.textAntiAliasing = SETTINGS.textAntiAliasing;
  s.hyphenationEnabled = SETTINGS.hyphenationEnabled;
  s.screenMargin = SETTINGS.screenMargin;
  s.embeddedStyle = SETTINGS.embeddedStyle;
  s.imageRendering = SETTINGS.imageRendering;
  return s;
}

void BookReaderSettings::applyToGlobal() const {
  SETTINGS.fontFamily = fontFamily;
  SETTINGS.fontSize = fontSize;
  SETTINGS.lineSpacing = lineSpacing;
  SETTINGS.paragraphAlignment = paragraphAlignment;
  SETTINGS.extraParagraphSpacing = extraParagraphSpacing;
  SETTINGS.textAntiAliasing = textAntiAliasing;
  SETTINGS.hyphenationEnabled = hyphenationEnabled;
  SETTINGS.screenMargin = screenMargin;
  SETTINGS.embeddedStyle = embeddedStyle;
  SETTINGS.imageRendering = imageRendering;
}

bool BookReaderSettings::load(const std::string& path, BookReaderSettings& out) {
  FsFile file;
  if (!Storage.openFileForRead("BKRS", path.c_str(), file)) return false;

  // Read field-by-field to avoid alignment issues
  uint8_t buf[12];
  if (file.read(buf, sizeof(buf)) != sizeof(buf)) {
    file.close();
    return false;
  }
  file.close();

  out.version = buf[0];
  if (out.version != VERSION) {
    LOG_DBG("BKRS", "Version mismatch: %u != %u", out.version, VERSION);
    return false;
  }

  out.fontFamily = buf[1];
  out.fontSize = buf[2];
  out.lineSpacing = buf[3];
  out.paragraphAlignment = buf[4];
  out.extraParagraphSpacing = buf[5];
  out.textAntiAliasing = buf[6];
  out.hyphenationEnabled = buf[7];
  out.screenMargin = buf[8];
  out.embeddedStyle = buf[9];
  out.imageRendering = buf[10];

  LOG_DBG("BKRS", "Loaded per-book settings from %s", path.c_str());
  return true;
}

bool BookReaderSettings::save(const std::string& path) const {
  FsFile file;
  if (!Storage.openFileForWrite("BKRS", path.c_str(), file)) {
    LOG_ERR("BKRS", "Failed to save per-book settings");
    return false;
  }

  uint8_t buf[12] = {version, fontFamily, fontSize, lineSpacing, paragraphAlignment, extraParagraphSpacing,
                      textAntiAliasing, hyphenationEnabled, screenMargin, embeddedStyle, imageRendering, 0};
  file.write(buf, sizeof(buf));
  file.close();
  return true;
}
