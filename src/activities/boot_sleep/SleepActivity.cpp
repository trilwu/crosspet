#include "SleepActivity.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <PNGdec.h>
#include <Txt.h>
#include <Xtc.h>
#include <esp_random.h>

#include <algorithm>
#include <ctime>
#include <new>

#include "../reader/EpubReaderActivity.h"
#include "../reader/TxtReaderActivity.h"
#include "../reader/XtcReaderActivity.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "BookStats.h"
#include "ReadingStats.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/Logo120.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"
#include "util/StringUtils.h"

#include <HalPowerManager.h>

namespace {

uint32_t hashSleepFileSet(const char* sleepDir, const std::vector<std::string>& files) {
  // FNV-1a over the selected directory plus the sorted file names. This keeps
  // the shuffle state stable across boots while automatically resetting it when
  // the user adds, removes, or renames sleep images.
  uint32_t hash = 2166136261u;
  for (const char* p = sleepDir; *p != '\0'; ++p) {
    hash ^= static_cast<uint8_t>(*p);
    hash *= 16777619u;
  }
  for (const auto& file : files) {
    for (char c : file) {
      hash ^= static_cast<uint8_t>(c);
      hash *= 16777619u;
    }
    hash ^= static_cast<uint8_t>('/');
    hash *= 16777619u;
  }
  return hash;
}

uint32_t nextShuffleRand(uint32_t& state) {
  if (state == 0) {
    state = 0x6d2b79f5u;
  }
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

void buildShuffledFileOrder(const size_t count, const uint32_t seed, std::vector<uint32_t>& order) {
  order.resize(count);
  for (size_t i = 0; i < count; ++i) {
    order[i] = static_cast<uint32_t>(i);
  }

  uint32_t state = seed;
  for (size_t i = count; i > 1; --i) {
    const size_t j = nextShuffleRand(state) % i;
    std::swap(order[i - 1], order[j]);
  }
}

void startNewCustomSleepShuffleCycle(const uint32_t fileSetHash) {
  APP_STATE.customSleepShuffleHash = fileSetHash;
  APP_STATE.customSleepShuffleSeed = esp_random();
  if (APP_STATE.customSleepShuffleSeed == 0) {
    APP_STATE.customSleepShuffleSeed = 1;
  }
  APP_STATE.customSleepShuffleCursor = 0;
}

// Context passed through PNGdec's decode() user-pointer to the per-scanline draw callback.
struct PngOverlayCtx {
  const GfxRenderer* renderer;
  int screenW;
  int screenH;
  int srcWidth;
  int dstWidth;
  int dstX;
  int dstY;
  float yScale;
  int lastDstY;
  // Color-key transparency (tRNS chunk) for TRUECOLOR and GRAYSCALE images.
  // Initialized lazily on the first draw callback because tRNS is processed during decode(),
  // not during open() — so hasAlpha()/getTransparentColor() are only valid once decode() starts.
  // -2 = not yet read; -1 = no color key; >=0 = 0x00RRGGBB (TRUECOLOR) or low-byte gray.
  int32_t transparentColor;
  PNG* pngObj;  // for lazy-init of transparentColor on first callback
};

// PNGdec file I/O callbacks — mirror the pattern in PngToFramebufferConverter.cpp.
void* pngSleepOpen(const char* filename, int32_t* size) {
  FsFile* f = new FsFile();
  if (!Storage.openFileForRead("SLP", std::string(filename), *f)) {
    delete f;
    return nullptr;
  }
  *size = f->size();
  return f;
}
void pngSleepClose(void* handle) {
  FsFile* f = reinterpret_cast<FsFile*>(handle);
  if (f) {
    f->close();
    delete f;
  }
}
int32_t pngSleepRead(PNGFILE* pFile, uint8_t* pBuf, int32_t len) {
  FsFile* f = reinterpret_cast<FsFile*>(pFile->fHandle);
  return f ? f->read(pBuf, len) : 0;
}
int32_t pngSleepSeek(PNGFILE* pFile, int32_t pos) {
  FsFile* f = reinterpret_cast<FsFile*>(pFile->fHandle);
  if (!f) return -1;
  return f->seek(pos);
}

// Per-scanline draw callback for PNG overlay compositing.
// Transparent pixels (alpha < 128) are skipped so the reader page shows through.
// Opaque pixels are drawn in their grayscale brightness (dark → black, light → white).
int pngOverlayDraw(PNGDRAW* pDraw) {
  PngOverlayCtx* ctx = reinterpret_cast<PngOverlayCtx*>(pDraw->pUser);

  // Lazy-init: tRNS chunk is processed during decode() before any IDAT data, so by the time
  // the first draw callback fires, hasAlpha() / getTransparentColor() are already valid.
  if (ctx->transparentColor == -2) {
    const int pt = pDraw->iPixelType;
    ctx->transparentColor = (pDraw->iHasAlpha && (pt == PNG_PIXEL_TRUECOLOR || pt == PNG_PIXEL_GRAYSCALE))
                                ? (int32_t)ctx->pngObj->getTransparentColor()
                                : -1;
  }

  const int destY = ctx->dstY + (int)(pDraw->y * ctx->yScale);
  if (destY == ctx->lastDstY) return 1;  // skip duplicate rows from Y scaling
  ctx->lastDstY = destY;
  if (destY < 0 || destY >= ctx->screenH) return 1;

  const int srcWidth = ctx->srcWidth;
  const int dstWidth = ctx->dstWidth;
  const uint8_t* pixels = pDraw->pPixels;
  const int pixelType = pDraw->iPixelType;
  const int hasAlpha = pDraw->iHasAlpha;

  int srcX = 0, error = 0;
  for (int dstX = 0; dstX < dstWidth; dstX++) {
    const int outX = ctx->dstX + dstX;
    if (outX >= 0 && outX < ctx->screenW) {
      uint8_t alpha = 255, gray = 0;
      switch (pixelType) {
        case PNG_PIXEL_TRUECOLOR_ALPHA: {
          const uint8_t* p = &pixels[srcX * 4];
          alpha = p[3];
          gray = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
          break;
        }
        case PNG_PIXEL_GRAY_ALPHA:
          gray = pixels[srcX * 2];
          alpha = pixels[srcX * 2 + 1];
          break;
        case PNG_PIXEL_TRUECOLOR: {
          const uint8_t* p = &pixels[srcX * 3];
          gray = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
          // tRNS color-key: if pixel matches the designated transparent color, skip it
          if (ctx->transparentColor >= 0 && p[0] == (uint8_t)((ctx->transparentColor >> 16) & 0xFF) &&
              p[1] == (uint8_t)((ctx->transparentColor >> 8) & 0xFF) &&
              p[2] == (uint8_t)(ctx->transparentColor & 0xFF)) {
            alpha = 0;
          }
          break;
        }
        case PNG_PIXEL_GRAYSCALE:
          gray = pixels[srcX];
          // tRNS color-key: transparent gray value stored in low byte
          if (ctx->transparentColor >= 0 && gray == (uint8_t)(ctx->transparentColor & 0xFF)) {
            alpha = 0;
          }
          break;
        case PNG_PIXEL_INDEXED:
          if (pDraw->pPalette) {
            const uint8_t idx = pixels[srcX];
            const uint8_t* p = &pDraw->pPalette[idx * 3];
            gray = (uint8_t)((p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8);
            if (hasAlpha) alpha = pDraw->pPalette[768 + idx];
          }
          break;
        default:
          gray = pixels[srcX];
          break;
      }

      if (alpha >= 128) {
        ctx->renderer->drawPixel(outX, destY, gray < 128);  // true = black, false = white
      }
      // alpha < 128: transparent — leave the reader page pixel intact
    }

    // Bresenham-style X stepping (handles downscaling; 1:1 when srcWidth == dstWidth)
    error += srcWidth;
    while (error >= dstWidth) {
      error -= dstWidth;
      srcX++;
    }
  }
  return 1;
}

}  // namespace

void SleepActivity::onEnter() {
  Activity::onEnter();
  // Persist current time to SD so it survives power cycles
  PET_MANAGER.save();
  // For OVERLAY mode the popup is suppressed so the frame buffer (reader page) stays intact
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::OVERLAY) {
    GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
  }

  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::BLANK):
      return renderBlankSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM):
      return renderCustomSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER):
      return renderCoverSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      if (APP_STATE.lastSleepFromReader) {
        return renderCoverSleepScreen();
      } else {
        return renderCustomSleepScreen();
      }
    case (CrossPointSettings::SLEEP_SCREEN_MODE::READING_STATS):
      return renderReadingStatsSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::OVERLAY):
      return renderOverlaySleepScreen();
    default:
      return renderDefaultSleepScreen();
  }
}

bool SleepActivity::displayCachedSleepScreen(const std::string& sourcePath) const {
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  if (!SleepScreenCache::load(renderer, sourcePath)) {
    return false;
  }
  renderer.setFadingFix(true);
  // A cached sleep screen bypasses bitmap decode/draw work and jumps straight to the
  // final update. Use a full refresh here so the panel fully settles on the lock image
  // before deep sleep, instead of leaving the reader page faintly visible underneath.
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  renderer.setFadingFix(SETTINGS.fadingFix);
  return true;
}

void SleepActivity::renderCustomSleepScreen() const {
  // If a specific sleep image is pinned, load it directly
  if (SETTINGS.sleepImagePath[0] != '\0') {
    const std::string pinnedPath(SETTINGS.sleepImagePath);
    // Try cache first
    if (displayCachedSleepScreen(pinnedPath)) {
      return;
    }
    FsFile pinnedFile;
    if (Storage.openFileForRead("SLP", SETTINGS.sleepImagePath, pinnedFile)) {
      Bitmap bitmap(pinnedFile, true);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        LOG_DBG("SLP", "Loading pinned sleep image: %s", SETTINGS.sleepImagePath);
        renderBitmapSleepScreen(bitmap, pinnedPath);
        pinnedFile.close();
        return;
      }
      pinnedFile.close();
    }
    LOG_ERR("SLP", "Pinned sleep image not found: %s", SETTINGS.sleepImagePath);
    // Fall through to random selection
  }

  // Check if we have a /.sleep (preferred) or /sleep directory
  const char* sleepDir = nullptr;
  auto dir = Storage.open("/.sleep");
  if (dir && dir.isDirectory()) {
    sleepDir = "/.sleep";
  } else {
    if (dir) dir.close();
    dir = Storage.open("/sleep");
    if (dir && dir.isDirectory()) {
      sleepDir = "/sleep";
    }
  }

  if (sleepDir) {
    std::vector<std::string> files;
    char name[500];
    // Collect BMP files by extension only (skip parseHeaders during scan)
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (file.isDirectory()) {
        file.close();
        continue;
      }
      file.getName(name, sizeof(name));
      auto filename = std::string(name);
      if (filename[0] == '.') {
        file.close();
        continue;
      }

      if (!FsHelpers::hasBmpExtension(filename)) {
        LOG_DBG("SLP", "Skipping non-.bmp file name: %s", name);
        file.close();
        continue;
      }
      files.emplace_back(filename);
      file.close();
    }
    std::sort(files.begin(), files.end());
    const auto numFiles = files.size();
    if (numFiles > 0) {
      const uint32_t fileSetHash = hashSleepFileSet(sleepDir, files);
      if (APP_STATE.customSleepShuffleHash != fileSetHash || APP_STATE.customSleepShuffleSeed == 0 ||
          APP_STATE.customSleepShuffleCursor >= numFiles) {
        startNewCustomSleepShuffleCycle(fileSetHash);
      }

      std::vector<uint32_t> shuffledOrder;
      buildShuffledFileOrder(numFiles, APP_STATE.customSleepShuffleSeed, shuffledOrder);

      // Walk only the unseen tail of the current shuffled cycle. This guarantees
      // every image appears once before the next reshuffle, while keeping only a
      // small index vector in RAM instead of persisting the whole sequence.
      for (uint32_t orderPos = APP_STATE.customSleepShuffleCursor; orderPos < numFiles; ++orderPos) {
        const uint32_t randomFileIndex = shuffledOrder[orderPos];
        const auto sourcePath = std::string(sleepDir) + "/" + files[randomFileIndex];

        // Try cache first
        if (displayCachedSleepScreen(sourcePath)) {
          APP_STATE.customSleepShuffleCursor = orderPos + 1;
          APP_STATE.saveToFile();
          dir.close();
          return;
        }

        FsFile file;
        if (Storage.openFileForRead("SLP", sourcePath, file)) {
          LOG_DBG("SLP", "Shuffled loading: %s/%s (%lu/%lu)", sleepDir, files[randomFileIndex].c_str(),
                  (unsigned long)(orderPos + 1), (unsigned long)numFiles);
          delay(100);
          Bitmap bitmap(file, true);
          if (bitmap.parseHeaders() == BmpReaderError::Ok) {
            renderBitmapSleepScreen(bitmap, sourcePath);
            APP_STATE.customSleepShuffleCursor = orderPos + 1;
            APP_STATE.saveToFile();
            file.close();
            dir.close();
            return;
          }
          file.close();
        }
      }

      // All remaining images in this cycle failed to render. Start a fresh cycle
      // for the next sleep attempt instead of getting stuck on the exhausted tail.
      startNewCustomSleepShuffleCycle(fileSetHash);
      APP_STATE.saveToFile();
    }
  }
  if (dir) dir.close();

  // Look for sleep.bmp on the root of the sd card to determine if we should
  // render a custom sleep screen instead of the default.
  {
    const std::string rootSleepPath = "/sleep.bmp";
    // Try cache first
    if (displayCachedSleepScreen(rootSleepPath)) {
      return;
    }
    FsFile file;
    if (Storage.openFileForRead("SLP", "/sleep.bmp", file)) {
      Bitmap bitmap(file, true);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        LOG_DBG("SLP", "Loading: /sleep.bmp");
        renderBitmapSleepScreen(bitmap, rootSleepPath);
        file.close();
        return;
      }
      file.close();
    }
  }

  renderDefaultSleepScreen();
}

void SleepActivity::renderDefaultSleepScreen() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, tr(STR_CROSSPOINT), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, tr(STR_SLEEPING));

  // Draw mini pet in bottom-right corner if a pet exists
  if (PET_MANAGER.exists()) {
    const auto& petState = PET_MANAGER.getState();
    const PetMood petMood = PET_MANAGER.isAlive()
                                ? (petState.attentionCall ? PetMood::NEEDY
                                   : petState.isSick ? PetMood::SICK
                                   : petState.hunger <= 30 ? PetMood::SAD
                                   : PetMood::SLEEPING)
                                : PetMood::DEAD;
    const int miniX = pageWidth - PetSpriteRenderer::MINI_W - 10;
    const int miniY = pageHeight - PetSpriteRenderer::MINI_H - 10;
    PetSpriteRenderer::drawMini(renderer, miniX, miniY, petState.stage, petMood,
                                petState.evolutionVariant, petState.petType);
    // Attention indicator: "!" above the mini pet
    if (petState.attentionCall) {
      renderer.drawText(SMALL_FONT_ID, miniX + PetSpriteRenderer::MINI_W / 2 - 2,
                        miniY - renderer.getLineHeight(SMALL_FONT_ID) - 1, "!");
    }
  }

  // Make sleep screen dark unless light is selected in settings
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT) {
    renderer.invertScreen();
  }

  // Power off analog drivers after refresh to prevent charge drift fading.
  renderer.setFadingFix(true);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  renderer.setFadingFix(SETTINGS.fadingFix);
}

void SleepActivity::renderBitmapSleepScreen(const Bitmap& bitmap, const std::string& sourcePath) const {
  int x, y;
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  float cropX = 0, cropY = 0;

  LOG_DBG("SLP", "bitmap %d x %d, screen %d x %d", bitmap.getWidth(), bitmap.getHeight(), pageWidth, pageHeight);
  if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
    // image will scale, make sure placement is right
    float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
    const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

    LOG_DBG("SLP", "bitmap ratio: %f, screen ratio: %f", ratio, screenRatio);
    if (ratio > screenRatio) {
      // image wider than viewport ratio, scaled down image needs to be centered vertically
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropX = 1.0f - (screenRatio / ratio);
        LOG_DBG("SLP", "Cropping bitmap x: %f", cropX);
        ratio = (1.0f - cropX) * static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
      }
      x = 0;
      y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
      LOG_DBG("SLP", "Centering with ratio %f to y=%d", ratio, y);
    } else {
      // image taller than viewport ratio, scaled down image needs to be centered horizontally
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropY = 1.0f - (ratio / screenRatio);
        LOG_DBG("SLP", "Cropping bitmap y: %f", cropY);
        ratio = static_cast<float>(bitmap.getWidth()) / ((1.0f - cropY) * static_cast<float>(bitmap.getHeight()));
      }
      x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
      y = 0;
      LOG_DBG("SLP", "Centering with ratio %f to x=%d", ratio, x);
    }
  } else {
    // center the image
    x = (pageWidth - bitmap.getWidth()) / 2;
    y = (pageHeight - bitmap.getHeight()) / 2;
  }

  LOG_DBG("SLP", "drawing to %d x %d", x, y);

  // Pre-clear: push a blank white frame to reset e-ink panel charge state.
  // Without this, black pixels fade lighter after ~1s due to residual charge.
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);

  renderer.clearScreen();

  const bool hasGreyscale = bitmap.hasGreyscale() &&
                            SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::NO_FILTER;

  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);

  if (SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE) {
    renderer.invertScreen();
  }

  // Save BW framebuffer to cache (greyscale needs 3 buffers, skip caching)
  if (!hasGreyscale && !sourcePath.empty()) {
    SleepScreenCache::save(renderer, sourcePath);
  }

  // Power off analog drivers after final refresh to prevent charge drift fading.
  renderer.setFadingFix(true);

  if (hasGreyscale) {
    // BW layer first (no fadingFix yet — grayscale layer is the final render)
    renderer.setFadingFix(false);
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleLsbBuffers();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleMsbBuffers();

    // Grayscale is the final render — apply fadingFix here to power off display
    renderer.setFadingFix(true);
    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  } else {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  }

  renderer.setFadingFix(SETTINGS.fadingFix);
}

void SleepActivity::renderCoverSleepScreen() const {
  void (SleepActivity::*renderNoCoverSleepScreen)() const;
  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      renderNoCoverSleepScreen = &SleepActivity::renderCustomSleepScreen;
      break;
    default:
      renderNoCoverSleepScreen = &SleepActivity::renderDefaultSleepScreen;
      break;
  }

  if (APP_STATE.openEpubPath.empty()) {
    return (this->*renderNoCoverSleepScreen)();
  }

  std::string coverBmpPath;
  bool cropped = SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP;

  // Check if the current book is XTC, TXT, or EPUB
  if (FsHelpers::hasXtcExtension(APP_STATE.openEpubPath)) {
    // Handle XTC file
    Xtc lastXtc(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastXtc.load()) {
      LOG_ERR("SLP", "Failed to load last XTC");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastXtc.generateCoverBmp()) {
      LOG_ERR("SLP", "Failed to generate XTC cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastXtc.getCoverBmpPath();
  } else if (FsHelpers::hasTxtExtension(APP_STATE.openEpubPath)) {
    // Handle TXT file - looks for cover image in the same folder
    Txt lastTxt(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastTxt.load()) {
      LOG_ERR("SLP", "Failed to load last TXT");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastTxt.generateCoverBmp()) {
      LOG_ERR("SLP", "No cover image found for TXT file");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastTxt.getCoverBmpPath();
  } else if (FsHelpers::hasEpubExtension(APP_STATE.openEpubPath)) {
    // Handle EPUB file
    Epub lastEpub(APP_STATE.openEpubPath, "/.crosspoint");
    // Skip loading css since we only need metadata here
    if (!lastEpub.load(true, true)) {
      LOG_ERR("SLP", "Failed to load last epub");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastEpub.generateCoverBmp(cropped)) {
      LOG_ERR("SLP", "Failed to generate cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastEpub.getCoverBmpPath(cropped);
  } else {
    return (this->*renderNoCoverSleepScreen)();
  }

  // Try cache first — avoids opening the source BMP entirely
  if (displayCachedSleepScreen(coverBmpPath)) {
    return;
  }
  FsFile file;
  if (Storage.openFileForRead("SLP", coverBmpPath, file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      LOG_DBG("SLP", "Rendering sleep cover: %s", coverBmpPath.c_str());
      renderBitmapSleepScreen(bitmap, coverBmpPath);
      file.close();
      return;
    }
    file.close();
  }

  return (this->*renderNoCoverSleepScreen)();
}

void SleepActivity::renderBlankSleepScreen() const {
  renderer.clearScreen();
  renderer.setFadingFix(true);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  renderer.setFadingFix(SETTINGS.fadingFix);
}

// Format a duration in seconds into a human-readable string.
// Results: "< 1 min", "X min", "X hr Y min", "X days Y hr"
static void formatDuration(char* buf, size_t size, uint32_t secs) {
  const uint32_t mins  = secs / 60;
  const uint32_t hours = mins / 60;
  const uint32_t days  = hours / 24;
  if (mins < 1) {
    snprintf(buf, size, "< 1 min");
  } else if (hours < 1) {
    snprintf(buf, size, "%lu min", (unsigned long)mins);
  } else if (days < 1) {
    snprintf(buf, size, "%lu hr %lu min", (unsigned long)hours, (unsigned long)(mins % 60));
  } else {
    snprintf(buf, size, "%lu days %lu hr", (unsigned long)days, (unsigned long)(hours % 24));
  }
}

void SleepActivity::renderReadingStatsSleepScreen() const {
  const int pageWidth  = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  constexpr int MARGIN = 28;

  // Pre-clear: push a blank white frame to wipe previous content ghosting
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);

  renderer.clearScreen();

  // Battery — top-right corner
  char battBuf[8];
  snprintf(battBuf, sizeof(battBuf), "%u%%", (unsigned)powerManager.getBatteryPercentage());
  const int battW = renderer.getTextWidth(SMALL_FONT_ID, battBuf);
  renderer.drawText(SMALL_FONT_ID, pageWidth - battW - 10, 10, battBuf);

  // Header: "Reading Stats"
  const int lhSmall = renderer.getLineHeight(SMALL_FONT_ID);
  const int lhUi10  = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhUi12  = renderer.getLineHeight(UI_12_FONT_ID);
  const int sepMargin = MARGIN + 20;
  const int sepW = pageWidth - 2 * sepMargin;

  int y = 60;
  renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_READING_STATS), true, EpdFontFamily::BOLD);
  y += lhUi12 + 6;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- TODAY ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_TODAY));
  y += lhSmall + 4;

  char todayBuf[32];
  formatDuration(todayBuf, sizeof(todayBuf), READ_STATS.todayReadSeconds);
  renderer.drawText(UI_10_FONT_ID, MARGIN, y, todayBuf, true, EpdFontFamily::BOLD);
  y += lhUi10 + 14;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- ALL TIME ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_ALL_TIME));
  y += lhSmall + 4;

  char totalBuf[32];
  formatDuration(totalBuf, sizeof(totalBuf), READ_STATS.totalReadSeconds);
  renderer.drawText(UI_10_FONT_ID, MARGIN, y, totalBuf, true, EpdFontFamily::BOLD);
  y += lhUi10 + 14;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- STATS GRID: Sessions | Books | Streak | Best ---
  {
    const int colW = (pageWidth - MARGIN * 2) / 4;
    const char* labels[] = {tr(STR_STATS_SESSIONS), tr(STR_STATS_BOOKS_DONE),
                            tr(STR_STATS_STREAK), tr(STR_STATS_BEST_STREAK)};
    char values[4][16];
    snprintf(values[0], sizeof(values[0]), "%u", (unsigned)READ_STATS.totalSessions);
    snprintf(values[1], sizeof(values[1]), "%u", (unsigned)READ_STATS.booksFinished);
    snprintf(values[2], sizeof(values[2]), tr(STR_STATS_DAYS), (unsigned)READ_STATS.currentStreak);
    snprintf(values[3], sizeof(values[3]), tr(STR_STATS_DAYS), (unsigned)READ_STATS.longestStreak);

    for (int i = 0; i < 4; i++) {
      int cx = MARGIN + i * colW + colW / 2;
      // Value (bold, centered in column)
      int vw = renderer.getTextWidth(UI_10_FONT_ID, values[i]);
      renderer.drawText(UI_10_FONT_ID, cx - vw / 2, y, values[i], true, EpdFontFamily::BOLD);
      // Label (small, centered below)
      int lw = renderer.getTextWidth(SMALL_FONT_ID, labels[i]);
      renderer.drawText(SMALL_FONT_ID, cx - lw / 2, y + lhUi10 + 2, labels[i]);
    }
    y += lhUi10 + lhSmall + 14;
  }
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- RECENT BOOKS (per-book time + progress) ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_LAST_BOOK));
  y += lhSmall + 6;

  const auto& allBooks = BOOK_STATS.getBooks();
  if (!allBooks.empty()) {
    // Sort by lastReadTimestamp descending, show up to 5 recent books
    struct BookRef { const char* title; uint32_t secs; uint8_t progress; uint32_t ts; };
    BookRef recent[5];
    int count = 0;
    for (const auto& kv : allBooks) {
      const auto& e = kv.second;
      if (count < 5) {
        recent[count++] = {e.title, e.totalSeconds, e.progress, e.lastReadTimestamp};
      } else {
        // Replace the oldest in our list if this one is newer
        int minIdx = 0;
        for (int j = 1; j < 5; j++) {
          if (recent[j].ts < recent[minIdx].ts) minIdx = j;
        }
        if (e.lastReadTimestamp > recent[minIdx].ts) {
          recent[minIdx] = {e.title, e.totalSeconds, e.progress, e.lastReadTimestamp};
        }
      }
    }
    // Sort descending by timestamp (simple insertion sort)
    for (int i = 1; i < count; i++) {
      BookRef tmp = recent[i];
      int j = i - 1;
      while (j >= 0 && recent[j].ts < tmp.ts) { recent[j + 1] = recent[j]; j--; }
      recent[j + 1] = tmp;
    }

    constexpr int BAR_H = 4;
    const int barW = pageWidth - MARGIN * 2;
    const int maxY = pageHeight - 100;  // Reserve space for pet sprite

    for (int i = 0; i < count && y < maxY; i++) {
      // Title (truncated) + reading time on right
      char timeBuf[24];
      formatDuration(timeBuf, sizeof(timeBuf), recent[i].secs);
      const int timeW = renderer.getTextWidth(SMALL_FONT_ID, timeBuf);
      const int titleMaxW = pageWidth - MARGIN * 2 - timeW - 8;
      const std::string titleStr = renderer.truncatedText(
          SMALL_FONT_ID, recent[i].title, titleMaxW, EpdFontFamily::BOLD);
      renderer.drawText(SMALL_FONT_ID, MARGIN, y, titleStr.c_str(), true, EpdFontFamily::BOLD);
      renderer.drawText(SMALL_FONT_ID, pageWidth - MARGIN - timeW, y, timeBuf);
      y += lhSmall + 3;

      // Progress bar + percentage
      char progBuf[8];
      snprintf(progBuf, sizeof(progBuf), "%u%%", (unsigned)recent[i].progress);
      const int progW = renderer.getTextWidth(SMALL_FONT_ID, progBuf);
      const int progBarW = barW - progW - 8;

      // Draw progress bar outline
      renderer.fillRect(MARGIN, y, progBarW, 1);
      renderer.fillRect(MARGIN, y + BAR_H - 1, progBarW, 1);
      renderer.fillRect(MARGIN, y, 1, BAR_H);
      renderer.fillRect(MARGIN + progBarW - 1, y, 1, BAR_H);
      const int filledW = static_cast<int>((progBarW - 2) * recent[i].progress / 100);
      if (filledW > 0) {
        renderer.fillRect(MARGIN + 1, y + 1, filledW, BAR_H - 2);
      }
      // Percentage text right of bar
      renderer.drawText(SMALL_FONT_ID, MARGIN + progBarW + 6, y - (lhSmall - BAR_H) / 2, progBuf);
      y += BAR_H + 10;
    }
  } else {
    renderer.drawText(UI_10_FONT_ID, MARGIN, y, tr(STR_STATS_NO_BOOKS), true, EpdFontFamily::REGULAR);
  }

  // Pet sprite — bottom-right corner (same as other screens)
  if (PET_MANAGER.exists()) {
    const auto& petState = PET_MANAGER.getState();
    const PetMood petMood = PET_MANAGER.isAlive()
                                ? (petState.attentionCall ? PetMood::NEEDY
                                   : petState.isSick      ? PetMood::SICK
                                   : petState.hunger <= 30 ? PetMood::SAD
                                   : PetMood::SLEEPING)
                                : PetMood::DEAD;
    constexpr int PET_SCALE = 2;
    const int pSize = PetSpriteRenderer::displaySize(PET_SCALE);
    const int petX  = pageWidth  - pSize - 10;
    const int petY  = pageHeight - pSize - 10;
    PetSpriteRenderer::drawPet(renderer, petX, petY, petState.stage, petMood, PET_SCALE,
                               petState.evolutionVariant, petState.petType);
    if (petState.attentionCall) {
      renderer.drawText(SMALL_FONT_ID, petX + pSize / 2 - 2,
                        petY - lhSmall - 2, "!");
    }
  }

  renderer.setFadingFix(true);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  renderer.setFadingFix(SETTINGS.fadingFix);
}

void SleepActivity::renderOverlaySleepScreen() const {
  // Overlay pictures always use portrait orientation regardless of the reader's orientation preference.
  const auto savedOrientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Portrait);
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  // Step 1: Ensure the frame buffer contains the reader page.
  // When coming from a reader activity the frame buffer already holds the page.
  // When coming from a non-reader activity we re-render it from the saved progress.
  if (!APP_STATE.lastSleepFromReader && !APP_STATE.openEpubPath.empty()) {
    const auto& path = APP_STATE.openEpubPath;
    bool rendered = false;

    if (FsHelpers::checkFileExtension(path, ".xtc") || FsHelpers::checkFileExtension(path, ".xtch")) {
      rendered = XtcReaderActivity::drawCurrentPageToBuffer(path, renderer);
    } else if (FsHelpers::checkFileExtension(path, ".txt")) {
      rendered = TxtReaderActivity::drawCurrentPageToBuffer(path, renderer);
    } else if (FsHelpers::checkFileExtension(path, ".epub")) {
      rendered = EpubReaderActivity::drawCurrentPageToBuffer(path, renderer);
    }

    if (!rendered) {
      LOG_DBG("SLP", "Page re-render failed, using white background");
      renderer.clearScreen();
    }
  }

  // Step 2: Load the overlay image using the same selection logic as renderCustomSleepScreen.
  // BMP: white pixels are skipped (transparent via drawBitmap), black pixels composited on top.
  // PNG: pixels with alpha < 128 are skipped; opaque pixels are drawn with their grayscale value.
  auto tryDrawOverlay = [&](const std::string& filename) -> bool {
    FsFile file;
    if (!Storage.openFileForRead("SLP", filename, file)) return false;
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() != BmpReaderError::Ok) {
      file.close();
      return false;
    }

    int x, y;
    float cropX = 0, cropY = 0;
    if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
      float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
      const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);
      if (ratio > screenRatio) {
        x = 0;
        y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
      } else {
        x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
        y = 0;
      }
    } else {
      x = (pageWidth - bitmap.getWidth()) / 2;
      y = (pageHeight - bitmap.getHeight()) / 2;
    }

    // Draw without clearScreen so the reader page remains in the frame buffer beneath
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    file.close();
    return true;
  };

  auto tryDrawPngOverlay = [&](const std::string& filename) -> bool {
    constexpr size_t MIN_FREE_HEAP = 60 * 1024;  // PNG decoder ~42 KB + overhead
    if (ESP.getFreeHeap() < MIN_FREE_HEAP) {
      LOG_ERR("SLP", "Not enough heap for PNG overlay decoder");
      return false;
    }
    PNG* png = new (std::nothrow) PNG();
    if (!png) return false;

    int rc = png->open(filename.c_str(), pngSleepOpen, pngSleepClose, pngSleepRead, pngSleepSeek, pngOverlayDraw);
    if (rc != PNG_SUCCESS) {
      LOG_DBG("SLP", "PNG open failed: %s (%d)", filename.c_str(), rc);
      delete png;
      return false;
    }

    const int srcW = png->getWidth(), srcH = png->getHeight();
    float yScale = 1.0f;
    int dstW = srcW, dstH = srcH;
    if (srcW > pageWidth || srcH > pageHeight) {
      const float scaleX = (float)pageWidth / srcW, scaleY = (float)pageHeight / srcH;
      const float scale = (scaleX < scaleY) ? scaleX : scaleY;
      dstW = (int)(srcW * scale);
      dstH = (int)(srcH * scale);
      yScale = (float)dstH / srcH;
    }

    PngOverlayCtx ctx;
    ctx.renderer = &renderer;
    ctx.screenW = pageWidth;
    ctx.screenH = pageHeight;
    ctx.srcWidth = srcW;
    ctx.dstWidth = dstW;
    ctx.dstX = (pageWidth - dstW) / 2;
    ctx.dstY = (pageHeight - dstH) / 2;
    ctx.yScale = yScale;
    ctx.lastDstY = -1;
    ctx.transparentColor = -2;  // will be resolved on first draw callback (after tRNS is parsed)
    ctx.pngObj = png;

    rc = png->decode(&ctx, 0);
    png->close();
    delete png;
    return rc == PNG_SUCCESS;
  };

  // Try /sleep/ directory first (random selection, same as renderCustomSleepScreen).
  // Accepts both .bmp and .png files; .bmp headers are validated during the scan.
  bool overlayDrawn = false;
  auto dir = Storage.open("/sleep");
  if (dir && dir.isDirectory()) {
    std::vector<std::string> files;
    char name[500];
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (file.isDirectory()) {
        file.close();
        continue;
      }
      file.getName(name, sizeof(name));
      auto filename = std::string(name);
      if (filename[0] == '.') {
        file.close();
        continue;
      }
      const bool isBmp = FsHelpers::checkFileExtension(filename, ".bmp");
      const bool isPng = FsHelpers::checkFileExtension(filename, ".png");
      if (!isBmp && !isPng) {
        file.close();
        continue;
      }
      if (isBmp) {
        Bitmap bmp(file);
        if (bmp.parseHeaders() != BmpReaderError::Ok) {
          file.close();
          continue;
        }
      }
      files.emplace_back(filename);
      file.close();
    }
    const auto numFiles = files.size();
    if (numFiles > 0) {
      auto randomFileIndex = random(numFiles);
      while (numFiles > 1 && randomFileIndex == APP_STATE.lastSleepImage) {
        randomFileIndex = random(numFiles);
      }
      APP_STATE.lastSleepImage = randomFileIndex;
      APP_STATE.saveToFile();
      const std::string selected = "/sleep/" + files[randomFileIndex];
      if (FsHelpers::checkFileExtension(selected, ".png")) {
        overlayDrawn = tryDrawPngOverlay(selected);
      } else {
        overlayDrawn = tryDrawOverlay(selected);
      }
    }
  }
  if (dir) dir.close();

  if (!overlayDrawn) {
    overlayDrawn = tryDrawOverlay("/sleep.bmp");
  }
  if (!overlayDrawn) {
    overlayDrawn = tryDrawPngOverlay("/sleep.png");
  }

  if (!overlayDrawn) {
    LOG_DBG("SLP", "No overlay image found, displaying page without overlay");
  }

  renderer.setOrientation(savedOrientation);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
