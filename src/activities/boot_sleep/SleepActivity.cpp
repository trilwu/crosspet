#include "SleepActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Txt.h>
#include <Xtc.h>

#include <ctime>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/Logo120.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"
#include "util/StringUtils.h"

#include <HalPowerManager.h>

void SleepActivity::onEnter() {
  Activity::onEnter();
  GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
  // Persist current time to SD so it survives power cycles
  PET_MANAGER.save();

  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::BLANK):
      return renderBlankSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM):
      return renderCustomSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER):
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      return renderCoverSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::CLOCK):
      return renderClockSleepScreen();
    default:
      return renderDefaultSleepScreen();
  }
}

void SleepActivity::renderCustomSleepScreen() const {
  // Check if we have a /sleep directory
  auto dir = Storage.open("/sleep");
  if (dir && dir.isDirectory()) {
    std::vector<std::string> files;
    char name[500];
    // collect all valid BMP files
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

      if (filename.substr(filename.length() - 4) != ".bmp") {
        LOG_DBG("SLP", "Skipping non-.bmp file name: %s", name);
        file.close();
        continue;
      }
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() != BmpReaderError::Ok) {
        LOG_DBG("SLP", "Skipping invalid BMP file: %s", name);
        file.close();
        continue;
      }
      files.emplace_back(filename);
      file.close();
    }
    const auto numFiles = files.size();
    if (numFiles > 0) {
      // Generate a random number between 1 and numFiles
      auto randomFileIndex = random(numFiles);
      // If we picked the same image as last time, reroll
      while (numFiles > 1 && randomFileIndex == APP_STATE.lastSleepImage) {
        randomFileIndex = random(numFiles);
      }
      APP_STATE.lastSleepImage = randomFileIndex;
      APP_STATE.saveToFile();
      const auto filename = "/sleep/" + files[randomFileIndex];
      FsFile file;
      if (Storage.openFileForRead("SLP", filename, file)) {
        LOG_DBG("SLP", "Randomly loading: /sleep/%s", files[randomFileIndex].c_str());
        delay(100);
        Bitmap bitmap(file, true);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderBitmapSleepScreen(bitmap);
          file.close();
          dir.close();
          return;
        }
        file.close();
      }
    }
  }
  if (dir) dir.close();

  // Look for sleep.bmp on the root of the sd card to determine if we should
  // render a custom sleep screen instead of the default.
  FsFile file;
  if (Storage.openFileForRead("SLP", "/sleep.bmp", file)) {
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      LOG_DBG("SLP", "Loading: /sleep.bmp");
      renderBitmapSleepScreen(bitmap);
      file.close();
      return;
    }
    file.close();
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
    const PetMood petMood = PET_MANAGER.isAlive()
                                ? (PET_MANAGER.getState().hunger <= 30 ? PetMood::SAD : PetMood::SLEEPING)
                                : PetMood::DEAD;
    PetSpriteRenderer::drawMini(renderer, pageWidth - PetSpriteRenderer::MINI_W - 10,
                                pageHeight - PetSpriteRenderer::MINI_H - 10,
                                PET_MANAGER.getState().stage, petMood);
  }

  // Make sleep screen dark unless light is selected in settings
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void SleepActivity::renderBitmapSleepScreen(const Bitmap& bitmap) const {
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
  renderer.clearScreen();

  const bool hasGreyscale = bitmap.hasGreyscale() &&
                            SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::NO_FILTER;

  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);

  if (SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);

  if (hasGreyscale) {
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

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  }
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
  if (StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".xtc") ||
      StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".xtch")) {
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
  } else if (StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".txt")) {
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
  } else if (StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".epub")) {
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

  FsFile file;
  if (Storage.openFileForRead("SLP", coverBmpPath, file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      LOG_DBG("SLP", "Rendering sleep cover: %s", coverBmpPath.c_str());
      renderBitmapSleepScreen(bitmap);
      file.close();
      return;
    }
    file.close();
  }

  return (this->*renderNoCoverSleepScreen)();
}

// Draw a single 7-segment digit at (x, y) with width dw and height dh.
// digit=-1 draws a dash (middle segment only).
// Segments have a 2px corner gap so each bar appears visually separated —
// this is the key detail that gives a modern vs old-school digital look.
static void drawSegDigit(GfxRenderer& renderer, int x, int y, int dw, int dh, int digit) {
  static const uint8_t SEGS[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  const uint8_t s = (digit >= 0 && digit <= 9) ? SEGS[digit] : 0x40;  // 0x40 = dash (g only)
  const int t = dh / 14;  // segment thickness — thinner than classic for a cleaner look
  const int g = 2;         // corner gap: adjacent segments don't touch at joints
  const int mh = dh / 2;
  // Horizontal segments — inset by (t+g) on both ends
  if (s & 0x01) renderer.fillRect(x + t + g, y,          dw - 2*(t+g), t);            // a top
  if (s & 0x40) renderer.fillRect(x + t + g, y + mh,     dw - 2*(t+g), t);            // g middle
  if (s & 0x08) renderer.fillRect(x + t + g, y + dh - t, dw - 2*(t+g), t);            // d bottom
  // Vertical top-half — starts g below top bar, ends g above middle bar
  if (s & 0x20) renderer.fillRect(x,          y + t + g, t, mh - t - 2*g);            // f top-left
  if (s & 0x02) renderer.fillRect(x + dw - t, y + t + g, t, mh - t - 2*g);            // b top-right
  // Vertical bottom-half — starts g below middle bar, ends g above bottom bar
  if (s & 0x10) renderer.fillRect(x,          y + mh + t + g, t, dh - mh - 2*t - 2*g); // e bot-left
  if (s & 0x04) renderer.fillRect(x + dw - t, y + mh + t + g, t, dh - mh - 2*t - 2*g); // c bot-right
}

static int sleepDaysInMonth(int month, int year) {
  if (month == 1) {
    int y = year + 1900;
    return ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : 28;
  }
  static const int days[] = {31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return days[month];
}

void SleepActivity::renderClockSleepScreen() const {
  // Use time()/localtime_r() directly instead of getLocalTime() to avoid
  // its internal delay(10) which can cause scheduler issues during sleep flow
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  static const char* const DAY_NAMES[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                          "Thursday", "Friday", "Saturday"};
  static const char* const MONTH_NAMES[] = {"January",   "February", "March",    "April",
                                            "May",       "June",     "July",     "August",
                                            "September", "October",  "November", "December"};
  static const char* const DAY_HDR[] = {"S", "M", "T", "W", "T", "F", "S"};

  // Valid time = year >= 2025; show placeholder text if not synced yet
  const bool timeValid = (timeinfo.tm_year >= 125);

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Battery percentage — top-right corner
  char battBuf[8];
  snprintf(battBuf, sizeof(battBuf), "%u%%", (unsigned)powerManager.getBatteryPercentage());
  const int battW = renderer.getTextWidth(SMALL_FONT_ID, battBuf);
  renderer.drawText(SMALL_FONT_ID, pageWidth - battW - 10, 10, battBuf);

  // Time — show placeholder if clock not yet synced
  char timeBuf[16];
  if (timeValid) {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "--:--");
  }

  // Date line
  char dateBuf[64];
  if (timeValid) {
    snprintf(dateBuf, sizeof(dateBuf), "%s, %d %s", DAY_NAMES[timeinfo.tm_wday],
             timeinfo.tm_mday, MONTH_NAMES[timeinfo.tm_mon]);
  } else {
    snprintf(dateBuf, sizeof(dateBuf), "Sync time via WiFi");
  }

  // Calendar month header
  char monthBuf[32];
  if (timeValid) {
    snprintf(monthBuf, sizeof(monthBuf), "%s %d", MONTH_NAMES[timeinfo.tm_mon],
             timeinfo.tm_year + 1900);
  } else {
    snprintf(monthBuf, sizeof(monthBuf), "---");
  }

  // 7-segment clock digits: 80×128 each, wider spacing for a modern layout
  constexpr int DW = 80;
  constexpr int DH = 128;
  constexpr int DGAP = 10;  // gap between digits in same HH or MM group
  constexpr int CW = 28;    // colon area width
  const int clockW = 4 * DW + 2 * DGAP + CW;
  const int clockX = (pageWidth - clockW) / 2;

  const int timeHeight = DH;
  const int dateHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int monthHdrH = renderer.getLineHeight(SMALL_FONT_ID);
  const int cellH = renderer.getLineHeight(SMALL_FONT_ID) + 6;
  const int calH = monthHdrH + 6 + cellH * 7;  // month header + day headers + max 6 body rows

  const int blockH = timeHeight + 14 + dateHeight + 16 + calH;
  const int startY = (pageHeight - blockH) / 2;

  // Draw large 7-segment time digits
  if (timeValid) {
    const int h0 = timeinfo.tm_hour / 10, h1 = timeinfo.tm_hour % 10;
    const int m0 = timeinfo.tm_min  / 10, m1 = timeinfo.tm_min  % 10;
    drawSegDigit(renderer, clockX,                        startY, DW, DH, h0);
    drawSegDigit(renderer, clockX + DW + DGAP,            startY, DW, DH, h1);
    // Colon: two square dots, evenly spaced in the upper and lower thirds
    constexpr int DOT = 12;
    const int colonX = clockX + 2 * DW + 2 * DGAP + (CW - DOT) / 2;
    renderer.fillRect(colonX, startY + DH / 3 - DOT / 2,     DOT, DOT);
    renderer.fillRect(colonX, startY + 2 * DH / 3 - DOT / 2, DOT, DOT);
    drawSegDigit(renderer, clockX + 2 * DW + 2 * DGAP + CW,           startY, DW, DH, m0);
    drawSegDigit(renderer, clockX + 3 * DW + 2 * DGAP + CW + DGAP,    startY, DW, DH, m1);
  } else {
    // No time sync — show dashes
    for (int i = 0; i < 4; i++) {
      int xd = clockX + i * (DW + DGAP) + (i >= 2 ? CW : 0);
      drawSegDigit(renderer, xd, startY, DW, DH, -1);
    }
  }
  renderer.drawCenteredText(UI_10_FONT_ID, startY + timeHeight + 14, dateBuf);

  // Calendar grid
  const int margin = 20;
  const int calW = pageWidth - margin * 2;
  const int cellW = calW / 7;
  const int x0 = margin;
  int calTop = startY + timeHeight + 14 + dateHeight + 16;

  // Thin separator between time area and calendar
  renderer.fillRect(margin + 20, calTop - 8, calW - 40, 1);

  // Month header above calendar
  renderer.drawCenteredText(SMALL_FONT_ID, calTop, monthBuf);
  calTop += monthHdrH + 6;

  // Calendar grid — only rendered when time is synced
  if (timeValid) {
    // Day-of-week header
    for (int d = 0; d < 7; d++) {
      int tw = renderer.getTextWidth(SMALL_FONT_ID, DAY_HDR[d]);
      renderer.drawText(SMALL_FONT_ID, x0 + d * cellW + (cellW - tw) / 2, calTop, DAY_HDR[d]);
    }

    // First weekday of month
    struct tm fm = timeinfo;
    fm.tm_mday = 1; fm.tm_hour = 0; fm.tm_min = 0; fm.tm_sec = 0;
    mktime(&fm);

    int col = fm.tm_wday;
    int maxDay = sleepDaysInMonth(timeinfo.tm_mon, timeinfo.tm_year);
    int rowY = calTop + cellH;

    for (int day = 1; day <= maxDay; day++) {
      char buf[3];
      snprintf(buf, sizeof(buf), "%d", day);
      int tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
      int cx = x0 + col * cellW;
      int tx = cx + (cellW - tw) / 2;

      if (day == timeinfo.tm_mday) {
        renderer.fillRect(cx + 2, rowY - 2, cellW - 4, cellH - 2);
        renderer.drawText(SMALL_FONT_ID, tx, rowY, buf, false);  // white text
      } else {
        renderer.drawText(SMALL_FONT_ID, tx, rowY, buf);
      }
      if (++col == 7) { col = 0; rowY += cellH; }
    }
  }

  // Draw cat in bottom-right corner (2x scale = 96x96) with a speech bubble above it
  if (PET_MANAGER.exists()) {
    const PetMood petMood = PET_MANAGER.isAlive()
                                ? (PET_MANAGER.getState().hunger <= 30 ? PetMood::SAD : PetMood::SLEEPING)
                                : PetMood::DEAD;
    constexpr int PET_SCALE = 2;
    const int pSize = PetSpriteRenderer::displaySize(PET_SCALE);
    const int petX = pageWidth - pSize - 10;
    const int petY = pageHeight - pSize - 10;
    PetSpriteRenderer::drawPet(renderer, petX, petY, PET_MANAGER.getState().stage, petMood, PET_SCALE);

    // Speech bubble: pick a message based on mood, varied by hour
    const char* msg = nullptr;
    if (petMood == PetMood::SLEEPING) {
      static const char* const SLEEP_MSGS[] = {"Zzz...", "Purr~", "Sweet dreams", "Dreaming..."};
      msg = SLEEP_MSGS[(uint32_t)time(nullptr) / 3600 % 4];
    } else if (petMood == PetMood::SAD) {
      static const char* const SAD_MSGS[] = {"Hungry...", "Read more!", "Feed me~"};
      msg = SAD_MSGS[(uint32_t)time(nullptr) / 3600 % 3];
    }
    if (msg != nullptr) {
      const int lh = renderer.getLineHeight(SMALL_FONT_ID);
      const int msgW = renderer.getTextWidth(SMALL_FONT_ID, msg);
      // Center the text over the pet sprite
      const int msgX = petX + (pSize - msgW) / 2;
      const int msgY = petY - lh - 4;
      renderer.drawText(SMALL_FONT_ID, msgX, msgY, msg);
    }
  }

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void SleepActivity::renderBlankSleepScreen() const {
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
