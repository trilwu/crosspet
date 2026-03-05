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
#include "ReadingStats.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/Logo120.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"
#include "activities/tools/WeatherActivity.h"
#include "util/LunarCalendar.h"
#include "util/StringUtils.h"

#include <HalPowerManager.h>

void SleepActivity::onEnter() {
  Activity::onEnter();
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
    case (CrossPointSettings::SLEEP_SCREEN_MODE::READING_STATS):
      return renderReadingStatsSleepScreen();
    default:
      return renderDefaultSleepScreen();
  }
}

void SleepActivity::renderCustomSleepScreen() const {
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
      const auto filename = std::string(sleepDir) + "/" + files[randomFileIndex];
      FsFile file;
      if (Storage.openFileForRead("SLP", filename, file)) {
        LOG_DBG("SLP", "Randomly loading: %s/%s", sleepDir, files[randomFileIndex].c_str());
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

  const char* const DAY_NAMES[] = {tr(STR_DAY_SUN), tr(STR_DAY_MON), tr(STR_DAY_TUE), tr(STR_DAY_WED),
                                    tr(STR_DAY_THU), tr(STR_DAY_FRI), tr(STR_DAY_SAT)};
  const char* const MONTH_NAMES[] = {tr(STR_MONTH_JAN), tr(STR_MONTH_FEB), tr(STR_MONTH_MAR), tr(STR_MONTH_APR),
                                      tr(STR_MONTH_MAY), tr(STR_MONTH_JUN), tr(STR_MONTH_JUL), tr(STR_MONTH_AUG),
                                      tr(STR_MONTH_SEP), tr(STR_MONTH_OCT), tr(STR_MONTH_NOV), tr(STR_MONTH_DEC)};
  const char* const DAY_HDR[] = {tr(STR_CAL_SUN), tr(STR_CAL_MON), tr(STR_CAL_TUE), tr(STR_CAL_WED),
                                  tr(STR_CAL_THU), tr(STR_CAL_FRI), tr(STR_CAL_SAT)};

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

  // Date line — prefix with "~" when clock may have drifted from deep sleep
  extern bool g_clockApproximate;
  char dateBuf[64];
  if (timeValid) {
    snprintf(dateBuf, sizeof(dateBuf), "%s%s, %d %s", g_clockApproximate ? "~" : "",
             DAY_NAMES[timeinfo.tm_wday], timeinfo.tm_mday, MONTH_NAMES[timeinfo.tm_mon]);
  } else {
    snprintf(dateBuf, sizeof(dateBuf), "%s", tr(STR_SYNC_TIME));
  }

  // Vietnamese lunar date
  char lunarBuf[40] = "";
  if (timeValid) {
    LunarDate lunar = solarToLunar(timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    if (lunar.isLeapMonth) {
      snprintf(lunarBuf, sizeof(lunarBuf), "Ngày %d tháng nhuận %d ÂL", lunar.day, lunar.month);
    } else {
      snprintf(lunarBuf, sizeof(lunarBuf), "Ngày %d tháng %d ÂL", lunar.day, lunar.month);
    }
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

  // Pre-load weather cache to include in vertical layout calculation
  char weatherLine[80] = "";
  int weatherLineH = 0;
  {
    WeatherData wData;
    uint8_t wCity = 0;
    char wTime[8] = "";
    if (WeatherActivity::loadWeatherCache(wData, wCity, wTime, sizeof(wTime))) {
      snprintf(weatherLine, sizeof(weatherLine), "%s: %.0f°C  %s  %d%%",
               WeatherActivity::CITIES[wCity].name,
               wData.temperature,
               WeatherActivity::weatherCodeToString(wData.weatherCode),
               wData.humidity);
      weatherLineH = renderer.getLineHeight(SMALL_FONT_ID) + 4;
    }
  }

  const int timeHeight = DH;
  const int dateHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int lunarHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int monthHdrH = renderer.getLineHeight(SMALL_FONT_ID);
  const int cellH = renderer.getLineHeight(SMALL_FONT_ID) + 6;
  const int calH = monthHdrH + 6 + cellH * 7;  // month header + day headers + max 6 body rows

  const int blockH = timeHeight + 14 + dateHeight + 6 + lunarHeight + weatherLineH + 10 + calH;
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
  if (lunarBuf[0]) {
    renderer.drawCenteredText(SMALL_FONT_ID, startY + timeHeight + 14 + dateHeight + 6, lunarBuf);
  }

  // Weather line below lunar date
  if (weatherLine[0]) {
    renderer.drawCenteredText(SMALL_FONT_ID,
                              startY + timeHeight + 14 + dateHeight + 6 + lunarHeight + 2,
                              weatherLine);
  }

  // Calendar grid
  const int margin = 20;
  const int calW = pageWidth - margin * 2;
  const int cellW = calW / 7;
  const int x0 = margin;
  int calTop = startY + timeHeight + 14 + dateHeight + 6 + lunarHeight + weatherLineH + 10;

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
    const auto& petState = PET_MANAGER.getState();
    const PetMood petMood = PET_MANAGER.isAlive()
                                ? (petState.attentionCall ? PetMood::NEEDY
                                   : petState.isSick ? PetMood::SICK
                                   : petState.hunger <= 30 ? PetMood::SAD
                                   : PetMood::SLEEPING)
                                : PetMood::DEAD;
    constexpr int PET_SCALE = 2;
    const int pSize = PetSpriteRenderer::displaySize(PET_SCALE);
    const int petX = pageWidth - pSize - 10;
    const int petY = pageHeight - pSize - 10;
    PetSpriteRenderer::drawPet(renderer, petX, petY, petState.stage, petMood, PET_SCALE,
                               petState.evolutionVariant, petState.petType);

    // Attention "!" indicator above the pet sprite
    if (petState.attentionCall) {
      renderer.drawText(SMALL_FONT_ID, petX + pSize / 2 - 2,
                        petY - renderer.getLineHeight(SMALL_FONT_ID) - 2, "!");
    }

    // Speech bubble: need-specific messages, varied by hour for sleep/sad states
    const char* msg = nullptr;
    if (petMood == PetMood::NEEDY) {
      switch (petState.currentNeed) {
        case PetNeed::HUNGRY: msg = tr(STR_PET_SLEEP_FEED_ME); break;
        case PetNeed::SICK:   msg = tr(STR_PET_SLEEP_MEDICINE); break;
        case PetNeed::DIRTY:  msg = tr(STR_PET_SLEEP_DIRTY); break;
        case PetNeed::BORED:  msg = tr(STR_PET_SLEEP_BORED); break;
        default:              msg = tr(STR_PET_SLEEP_HEY); break;
      }
    } else if (petMood == PetMood::SICK) {
      msg = tr(STR_PET_SLEEP_SICK);
    } else if (petMood == PetMood::SLEEPING) {
      const char* const SLEEP_MSGS[] = {tr(STR_PET_SLEEP_ZZZ), tr(STR_PET_SLEEP_PURR),
                                         tr(STR_PET_SLEEP_SWEET), tr(STR_PET_SLEEP_DREAMING)};
      msg = SLEEP_MSGS[(uint32_t)time(nullptr) / 3600 % 4];
    } else if (petMood == PetMood::SAD) {
      const char* const SAD_MSGS[] = {tr(STR_PET_SLEEP_HUNGRY), tr(STR_PET_SLEEP_READ_MORE),
                                       tr(STR_PET_SLEEP_FEED_ME)};
      msg = SAD_MSGS[(uint32_t)time(nullptr) / 3600 % 3];
    }
    if (msg != nullptr) {
      const int lh = renderer.getLineHeight(SMALL_FONT_ID);
      const int msgW = renderer.getTextWidth(SMALL_FONT_ID, msg);
      const int msgX = petX + (pSize - msgW) / 2;
      const int msgY = petY - lh - 4;
      renderer.drawText(SMALL_FONT_ID, msgX, msgY, msg);
    }
  }

  // Daily quote — changes each day, shown at the bottom of the screen.
  // Positioned within the left portion to avoid the pet in the bottom-right corner.
  if (timeValid) {
    static const char* const QUOTE_EN[] = {
      "A reader lives a thousand lives.",
      "Not all those who wander are lost.",
      "A book is a dream you hold in your hands.",
      "So many books, so little time.",
      "Books are a uniquely portable magic.",
      "There is no friend as loyal as a book.",
      "A word after a word after a word is power.",
      "Read, read, read. Read everything.",
      "A good book has no ending.",
      "Books are mirrors of the soul.",
      "Think before you speak. Read before you think.",
      "Good books don't give up all their secrets at once.",
      "Today a reader, tomorrow a leader.",
      "Literature is news that stays news.",
      "I have always imagined Paradise as a kind of library.",
      "Sleep is good; books are better.",
      "A house without books is like a room without windows.",
      "No two persons ever read the same book.",
      "One book, one pen can change the world.",
      "Books permit us to voyage through time.",
      "The reading of good books is a conversation.",
      "Reading is to the mind what exercise is to the body.",
      "You don't have to burn books to destroy a culture.",
      "Classic: a book people praise and don't read.",
      "One must always be careful of books.",
      "I declare after all there is no enjoyment like reading!",
      "Where is human nature so weak as in a bookshop?",
      "It is what you read when you don't have to that matters.",
    };
    static const char* const QUOTE_VI[] = {
      "Người đọc sống ngàn cuộc đời.",
      "Không phải ai lang thang cũng lạc lối.",
      "Sách là giấc mơ cầm trên tay.",
      "Nhiều sách quá, ít thời gian quá.",
      "Sách là phép màu bỏ túi.",
      "Không bạn nào trung thành như sách.",
      "Từ nối từ nối từ là sức mạnh.",
      "Đọc, đọc, đọc. Đọc mọi thứ.",
      "Sách hay không có hồi kết.",
      "Sách là tấm gương tâm hồn.",
      "Nghĩ trước khi nói. Đọc trước khi nghĩ.",
      "Sách hay không tiết lộ hết bí mật.",
      "Hôm nay đọc sách, mai dẫn đầu.",
      "Văn chương là tin tức còn mãi.",
      "Tôi luôn hình dung Thiên đường như thư viện.",
      "Ngủ thì tốt, sách thì tuyệt hơn.",
      "Nhà không sách như phòng không cửa sổ.",
      "Không ai đọc cùng một cuốn sách giống nhau.",
      "Một cuốn sách, một cây bút đổi thay thế giới.",
      "Sách cho ta du hành qua thời gian.",
      "Đọc sách hay là một cuộc trò chuyện.",
      "Đọc sách là tập thể dục cho trí óc.",
      "Không cần đốt sách để hủy nền văn hóa.",
      "Kinh điển: sách người ta khen mà không đọc.",
      "Luôn phải cẩn thận với sách.",
      "Không có niềm vui nào bằng đọc sách!",
      "Bản tính con người yếu đuối nhất ở hiệu sách.",
      "Điều bạn đọc khi không bắt buộc mới quan trọng.",
    };
    static const char* const QUOTE_AUTHOR[] = {
      "G.R.R. Martin",    "J.R.R. Tolkien",   "Neil Gaiman",
      "Frank Zappa",      "Stephen King",      "Hemingway",
      "Margaret Atwood",  "Faulkner",          "R.D. Cumming",
      "Virginia Woolf",   "Fran Lebowitz",     "Stephen King",
      "W. Fusselman",     "Ezra Pound",        "Jorge L. Borges",
      "G.R.R. Martin",    "Horace Mann",       "Edmund Wilson",
      "Malala Yousafzai", "Carl Sagan",        "Descartes",
      "Richard Steele",   "Ray Bradbury",      "Mark Twain",
      "Cassandra Clare",  "Jane Austen",       "H.W. Beecher",
      "Oscar Wilde",
    };
    constexpr int QUOTE_COUNT = 28;
    const int qIdx = timeinfo.tm_yday % QUOTE_COUNT;
    const bool isVi = I18N.getLanguage() == Language::VIETNAMESE;
    const char* const* QUOTE_TEXT = isVi ? QUOTE_VI : QUOTE_EN;

    const int lhQ = renderer.getLineHeight(SMALL_FONT_ID);
    // Safe width: leave ~108px on the right for the pet (96px sprite + padding)
    constexpr int PET_RESERVE = 108;
    const int qLeft = margin;
    const int qRight = pageWidth - PET_RESERVE;
    const int qCenterX = (qLeft + qRight) / 2;
    const int qMaxW = qRight - qLeft;

    // Author line: "— Author", second line from bottom
    char authorBuf[32];
    snprintf(authorBuf, sizeof(authorBuf), "— %s", QUOTE_AUTHOR[qIdx]);

    const int authorY = pageHeight - 12;
    const int quoteY  = authorY - lhQ - 3;

    // Truncate quote if it exceeds the available width
    const std::string qStr =
        renderer.truncatedText(SMALL_FONT_ID, QUOTE_TEXT[qIdx], qMaxW, EpdFontFamily::REGULAR);

    const int qW = renderer.getTextWidth(SMALL_FONT_ID, qStr.c_str());
    renderer.drawText(SMALL_FONT_ID, std::max(qLeft, qCenterX - qW / 2), quoteY, qStr.c_str());

    const int aW = renderer.getTextWidth(SMALL_FONT_ID, authorBuf);
    renderer.drawText(SMALL_FONT_ID, std::max(qLeft, qCenterX - aW / 2), authorY, authorBuf);
  }

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void SleepActivity::renderBlankSleepScreen() const {
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
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

  // --- LAST BOOK ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_LAST_BOOK));
  y += lhSmall + 4;

  if (READ_STATS.lastBookTitle[0] != '\0') {
    constexpr int BADGE_RESERVE = 64;
    const int titleMaxW = pageWidth - MARGIN * 2 - BADGE_RESERVE;
    const std::string titleStr = renderer.truncatedText(
        UI_10_FONT_ID, READ_STATS.lastBookTitle, titleMaxW, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, MARGIN, y, titleStr.c_str(), true, EpdFontFamily::BOLD);

    char progBuf[8];
    snprintf(progBuf, sizeof(progBuf), "%u%%", (unsigned)READ_STATS.lastBookProgress);
    const int progW = renderer.getTextWidth(SMALL_FONT_ID, progBuf);
    renderer.drawText(SMALL_FONT_ID, pageWidth - MARGIN - progW, y + (lhUi10 - lhSmall) / 2, progBuf);

    y += lhUi10 + 6;

    // Progress bar
    constexpr int BAR_H = 6;
    const int barW = pageWidth - MARGIN * 2;
    renderer.fillRect(MARGIN,             y,             barW, 1);
    renderer.fillRect(MARGIN,             y + BAR_H - 1, barW, 1);
    renderer.fillRect(MARGIN,             y,             1,    BAR_H);
    renderer.fillRect(MARGIN + barW - 1,  y,             1,    BAR_H);
    const int filledW = static_cast<int>((barW - 2) * READ_STATS.lastBookProgress / 100);
    if (filledW > 0) {
      renderer.fillRect(MARGIN + 1, y + 1, filledW, BAR_H - 2);
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

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
