#include "SleepImagePickerActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <PNGdec.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "activities/reader/EpubReaderActivity.h"
#include "activities/reader/TxtReaderActivity.h"
#include "activities/reader/XtcReaderActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/SleepScreenCache.h"

static constexpr int MAX_FILES = 30;
// Must match home screen's CP_COVER_H for shared thumbnail cache
static constexpr int THUMB_H = 188;

// ── PNG overlay callbacks ───────────────────────────────────────────────────

namespace {

struct PngCtx {
  const GfxRenderer* renderer;
  int screenW, screenH, srcWidth, dstWidth, dstX, dstY;
  float yScale;
  int lastDstY;
  int32_t transparentColor;
  PNG* pngObj;
};

void* pngOpen(const char* filename, int32_t* size) {
  FsFile* f = new FsFile();
  if (!Storage.openFileForRead("SIP", std::string(filename), *f)) { delete f; return nullptr; }
  *size = f->size();
  return f;
}
void pngClose(void* h) { auto* f = reinterpret_cast<FsFile*>(h); if (f) { f->close(); delete f; } }
int32_t pngRead(PNGFILE* p, uint8_t* buf, int32_t len) {
  auto* f = reinterpret_cast<FsFile*>(p->fHandle); return f ? f->read(buf, len) : 0;
}
int32_t pngSeek(PNGFILE* p, int32_t pos) {
  auto* f = reinterpret_cast<FsFile*>(p->fHandle); return f ? f->seek(pos) : -1;
}

int pngOverlayDraw(PNGDRAW* pDraw) {
  auto* ctx = reinterpret_cast<PngCtx*>(pDraw->pUser);
  if (ctx->transparentColor == -2) {
    const int pt = pDraw->iPixelType;
    ctx->transparentColor = (pDraw->iHasAlpha && (pt == PNG_PIXEL_TRUECOLOR || pt == PNG_PIXEL_GRAYSCALE))
                                ? (int32_t)ctx->pngObj->getTransparentColor() : -1;
  }
  const int destY = ctx->dstY + (int)(pDraw->y * ctx->yScale);
  if (destY == ctx->lastDstY) return 1;
  ctx->lastDstY = destY;
  if (destY < 0 || destY >= ctx->screenH) return 1;

  const uint8_t* pixels = pDraw->pPixels;
  const int pixelType = pDraw->iPixelType;
  const int hasAlpha = pDraw->iHasAlpha;
  int srcX = 0, error = 0;
  for (int dstX = 0; dstX < ctx->dstWidth; dstX++) {
    const int outX = ctx->dstX + dstX;
    if (outX >= 0 && outX < ctx->screenW) {
      uint8_t alpha = 255, gray = 0;
      switch (pixelType) {
        case PNG_PIXEL_TRUECOLOR_ALPHA: { const uint8_t* p = &pixels[srcX * 4]; alpha = p[3]; gray = (uint8_t)((p[0]*77+p[1]*150+p[2]*29)>>8); break; }
        case PNG_PIXEL_GRAY_ALPHA: gray = pixels[srcX*2]; alpha = pixels[srcX*2+1]; break;
        case PNG_PIXEL_TRUECOLOR: { const uint8_t* p = &pixels[srcX*3]; gray = (uint8_t)((p[0]*77+p[1]*150+p[2]*29)>>8);
          if (ctx->transparentColor>=0 && p[0]==(uint8_t)((ctx->transparentColor>>16)&0xFF) && p[1]==(uint8_t)((ctx->transparentColor>>8)&0xFF) && p[2]==(uint8_t)(ctx->transparentColor&0xFF)) alpha=0; break; }
        case PNG_PIXEL_GRAYSCALE: gray = pixels[srcX]; if (ctx->transparentColor>=0 && gray==(uint8_t)(ctx->transparentColor&0xFF)) alpha=0; break;
        case PNG_PIXEL_INDEXED: if (pDraw->pPalette) { const uint8_t idx=pixels[srcX]; const uint8_t* p=&pDraw->pPalette[idx*3]; gray=(uint8_t)((p[0]*77+p[1]*150+p[2]*29)>>8); if (hasAlpha) alpha=pDraw->pPalette[768+idx]; } break;
        default: gray = pixels[srcX]; break;
      }
      if (alpha >= 128) ctx->renderer->drawPixel(outX, destY, gray < 128);
    }
    error += ctx->srcWidth;
    while (error >= ctx->dstWidth) { error -= ctx->dstWidth; srcX++; }
  }
  return 1;
}

// Sleep mode display names (matches SLEEP_SCREEN_MODE enum order)
const char* modeNames[] = {"Dark", "Light", "Custom", "Cover", "None", "Cover+Custom", "Clock", "Reading Stats", "Page Overlay", "Keep Screen"};

}  // namespace

// ── Helpers ─────────────────────────────────────────────────────────────────

bool SleepImagePickerActivity::modeUsesImages() const {
  const auto m = SETTINGS.sleepScreen;
  return m == CrossPointSettings::CUSTOM || m == CrossPointSettings::COVER_CUSTOM || m == CrossPointSettings::OVERLAY;
}

int SleepImagePickerActivity::totalItems() const {
  // Mode selector + (images if applicable) + clear cache
  return 1 + (modeUsesImages() ? static_cast<int>(fileList.size()) : 0) + 1;
}

const char* SleepImagePickerActivity::currentModeName() const {
  const uint8_t m = SETTINGS.sleepScreen;
  return (m < CrossPointSettings::SLEEP_SCREEN_MODE_COUNT) ? modeNames[m] : "?";
}

const char* SleepImagePickerActivity::modeDescription() const {
  switch (SETTINGS.sleepScreen) {
    case CrossPointSettings::DARK: return "Black screen";
    case CrossPointSettings::LIGHT: return "White screen";
    case CrossPointSettings::CUSTOM: return "Image from /sleep/ folder";
    case CrossPointSettings::COVER: return "Book cover art";
    case CrossPointSettings::BLANK: return "Blank white screen";
    case CrossPointSettings::COVER_CUSTOM: return "Cover + image overlay";
    case CrossPointSettings::CLOCK: return "Digital clock + calendar";
    case CrossPointSettings::READING_STATS: return "Reading statistics";
    case CrossPointSettings::OVERLAY: return "Image on book page";
    case CrossPointSettings::KEEP_SCREEN: return "Keep last screen + sleep icon";
    default: return "";
  }
}

bool SleepImagePickerActivity::isCurrentImageSelection(int idx) const {
  if (!isFileItem(idx)) return false;
  const int fileIdx = idx - firstFileIndex();
  const std::string fullPath = sleepDir + "/" + fileList[fileIdx];
  return fullPath == SETTINGS.sleepImagePath;
}

// ── File loading ────────────────────────────────────────────────────────────

void SleepImagePickerActivity::loadFiles() {
  fileList.clear();
  sleepDir.clear();

  auto dir = Storage.open("/.sleep");
  if (dir && dir.isDirectory()) { sleepDir = "/.sleep"; }
  else { if (dir) dir.close(); dir = Storage.open("/sleep"); if (dir && dir.isDirectory()) sleepDir = "/sleep"; }
  if (sleepDir.empty()) { if (dir) dir.close(); return; }

  char name[256];
  while (static_cast<int>(fileList.size()) < MAX_FILES) {
    auto file = dir.openNextFile();
    if (!file) break;
    file.getName(name, sizeof(name));
    if (name[0] != '.' && !file.isDirectory()) {
      std::string_view sv{name};
      if (FsHelpers::hasBmpExtension(sv) || FsHelpers::checkFileExtension(sv, ".png")) {
        fileList.emplace_back(name);
      }
    }
    file.close();
  }
  dir.close();
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

void SleepImagePickerActivity::onEnter() {
  Activity::onEnter();
  loadFiles();
  selectorIndex = 0;
  slideshowActive = false;
  requestUpdate();
}

void SleepImagePickerActivity::onExit() {
  fileList.clear();
  fileList.shrink_to_fit();
  Activity::onExit();
}

// ── Actions ─────────────────────────────────────────────────────────────────

void SleepImagePickerActivity::cycleSleepMode() {
  SETTINGS.sleepScreen = (SETTINGS.sleepScreen + 1) % CrossPointSettings::SLEEP_SCREEN_MODE_COUNT;
  SETTINGS.saveToFile();
  SleepScreenCache::invalidateAll();
  // Reset selector to mode item when mode changes (file list may appear/disappear)
  selectorIndex = 0;
  requestUpdate();
}

void SleepImagePickerActivity::saveImageSelection() {
  if (!isFileItem(selectorIndex)) return;
  const int fileIdx = selectorIndex - firstFileIndex();
  const std::string fullPath = sleepDir + "/" + fileList[fileIdx];
  strncpy(SETTINGS.sleepImagePath, fullPath.c_str(), sizeof(SETTINGS.sleepImagePath) - 1);
  SETTINGS.sleepImagePath[sizeof(SETTINGS.sleepImagePath) - 1] = '\0';
  SETTINGS.saveToFile();
  SleepScreenCache::invalidateAll();
}

void SleepImagePickerActivity::clearCache() {
  SleepScreenCache::invalidateAll();
  renderer.clearScreen();
  renderer.drawCenteredText(UI_10_FONT_ID,
                            renderer.getScreenHeight() / 2 - renderer.getLineHeight(UI_10_FONT_ID) / 2,
                            tr(STR_SLEEP_CACHE_CLEARED));
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  delay(800);
  requestUpdate();
}

// ── Overlay slideshow ───────────────────────────────────────────────────────

bool SleepImagePickerActivity::renderBookPage() {
  const auto& path = APP_STATE.openEpubPath;
  if (path.empty()) return false;
  if (FsHelpers::checkFileExtension(path, ".xtc") || FsHelpers::checkFileExtension(path, ".xtch"))
    return XtcReaderActivity::drawCurrentPageToBuffer(path, renderer);
  if (FsHelpers::checkFileExtension(path, ".txt"))
    return TxtReaderActivity::drawCurrentPageToBuffer(path, renderer);
  if (FsHelpers::checkFileExtension(path, ".epub"))
    return EpubReaderActivity::drawCurrentPageToBuffer(path, renderer);
  return false;
}

bool SleepImagePickerActivity::drawOverlayImage(const std::string& path) {
  const int pw = renderer.getScreenWidth(), ph = renderer.getScreenHeight();

  if (FsHelpers::checkFileExtension(path, ".png")) {
    PNG* png = new (std::nothrow) PNG();
    if (!png) return false;
    int rc = png->open(path.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngOverlayDraw);
    if (rc != PNG_SUCCESS) { delete png; return false; }
    const int srcW = png->getWidth(), srcH = png->getHeight();
    float yScale = 1.0f; int dstW = srcW, dstH = srcH;
    if (srcW > pw || srcH > ph) {
      const float s = std::min((float)pw/srcW, (float)ph/srcH);
      dstW = (int)(srcW*s); dstH = (int)(srcH*s); yScale = (float)dstH/srcH;
    }
    PngCtx ctx{&renderer, pw, ph, srcW, dstW, (pw-dstW)/2, (ph-dstH)/2, yScale, -1, -2, png};
    rc = png->decode(&ctx, 0); png->close(); delete png;
    return rc == PNG_SUCCESS;
  }

  // BMP overlay
  FsFile file;
  if (!Storage.openFileForRead("SIP", path, file)) return false;
  Bitmap bmp(file, true);
  if (bmp.parseHeaders() != BmpReaderError::Ok) { file.close(); return false; }
  const int bw = bmp.getWidth(), bh = bmp.getHeight();
  const int x = (pw - std::min(bw, pw)) / 2, y = (ph - std::min(bh, ph)) / 2;
  renderer.drawBitmap(bmp, x, y, std::min(bw, pw), std::min(bh, ph));
  file.close();
  return true;
}

void SleepImagePickerActivity::renderSlideshow() {
  const auto savedOr = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Portrait);

  if (!renderBookPage()) renderer.clearScreen();
  if (slideshowIndex >= 0 && slideshowIndex < static_cast<int>(fileList.size())) {
    drawOverlayImage(sleepDir + "/" + fileList[slideshowIndex]);
  }

  const int sw = renderer.getScreenWidth(), sh = renderer.getScreenHeight();
  renderer.fillRect(0, sh - 32, sw, 32, true);
  char info[64];
  snprintf(info, sizeof(info), "%d/%d  %s", slideshowIndex + 1, (int)fileList.size(), fileList[slideshowIndex].c_str());
  renderer.drawCenteredText(SMALL_FONT_ID, sh - 28, info, false);

  renderer.setOrientation(savedOr);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

// ── Input ───────────────────────────────────────────────────────────────────

void SleepImagePickerActivity::loop() {
  if (slideshowActive) {
    const int total = static_cast<int>(fileList.size());
    buttonNavigator.onNext([this, total] { slideshowIndex = (slideshowIndex + 1) % total; renderSlideshow(); });
    buttonNavigator.onPrevious([this, total] { slideshowIndex = (slideshowIndex - 1 + total) % total; renderSlideshow(); });
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      selectorIndex = slideshowIndex + firstFileIndex();
      saveImageSelection();
      slideshowActive = false;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { slideshowActive = false; requestUpdate(); }
    return;
  }

  const int items = totalItems();
  buttonNavigator.onNext([this, items] { selectorIndex = ButtonNavigator::nextIndex(selectorIndex, items); requestUpdate(); });
  buttonNavigator.onPrevious([this, items] { selectorIndex = ButtonNavigator::previousIndex(selectorIndex, items); requestUpdate(); });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex == MODE_ITEM) {
      cycleSleepMode();
    } else if (selectorIndex == cacheItemIndex()) {
      clearCache();
    } else if (isFileItem(selectorIndex)) {
      // For overlay mode: enter slideshow preview. For custom: save + brief preview.
      if (SETTINGS.sleepScreen == CrossPointSettings::OVERLAY) {
        slideshowIndex = selectorIndex - firstFileIndex();
        slideshowActive = true;
        renderSlideshow();
      } else {
        saveImageSelection();
        // Brief preview
        const int fileIdx = selectorIndex - firstFileIndex();
        const std::string fullPath = sleepDir + "/" + fileList[fileIdx];
        renderer.clearScreen();
        FsFile f;
        if (Storage.openFileForRead("SIP", fullPath, f)) {
          Bitmap bmp(f, false);
          if (bmp.parseHeaders() == BmpReaderError::Ok) {
            const int sw = renderer.getScreenWidth(), sh = renderer.getScreenHeight();
            const int x = std::max(0, (sw - (int)bmp.getWidth()) / 2);
            const int y = std::max(0, (sh - (int)bmp.getHeight()) / 2);
            renderer.drawBitmap(bmp, x, y, sw, sh, 0, 0);
          }
          f.close();
        }
        renderer.displayBuffer(HalDisplay::FAST_REFRESH);
        delay(1500);
        requestUpdate();
      }
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (isFileItem(selectorIndex) && isCurrentImageSelection(selectorIndex)) {
      // Clear pinned image — revert to random selection
      SETTINGS.sleepImagePath[0] = '\0';
      SETTINGS.saveToFile();
      SleepScreenCache::invalidateAll();
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finish(); }
}

// ── Rendering ───────────────────────────────────────────────────────────────

void SleepImagePickerActivity::renderList(int pageWidth, int pageHeight) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SLEEP_IMAGE_PICKER));

  const int menuTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int menuH = pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const int items = totalItems();
  GUI.drawList(renderer, Rect{0, menuTop, pageWidth, menuH}, items, selectorIndex,
    [this](int idx) -> std::string {
      if (idx == MODE_ITEM) return std::string(tr(STR_SLEEP_SCREEN)) + ": " + currentModeName();
      if (idx == cacheItemIndex()) return std::string(tr(STR_RELOAD_SLEEP_IMAGE));
      if (isFileItem(idx)) {
        const int fi = idx - firstFileIndex();
        const bool cur = isCurrentImageSelection(idx);
        return cur ? std::string("* ") + fileList[fi] : fileList[fi];
      }
      return "";
    },
    // Subtitle callback: show description for mode item
    [this](int idx) -> std::string {
      if (idx == MODE_ITEM) return std::string(modeDescription());
      return "";
    });

  const bool canUnpin = isFileItem(selectorIndex) && isCurrentImageSelection(selectorIndex);
  const char* leftLabel = canUnpin ? tr(STR_UNPIN) : "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), leftLabel, "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void SleepImagePickerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderList(renderer.getScreenWidth(), renderer.getScreenHeight());
  renderer.displayBuffer();
}
