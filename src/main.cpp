#include <Arduino.h>
#include <Epub.h>
#include <FontCacheManager.h>
#include <FontDecompressor.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <HalSystem.h>
#include <I18n.h>
#include <Logging.h>
#include <SPI.h>
#include <builtinFonts/all.h>

#include <cstring>
#include <sys/time.h>

#include <esp_private/esp_clk.h>
#include <esp_sntp.h>

#include "CrossPetSettings.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "KOReaderCredentialStore.h"
#include "GameScores.h"
#include "ReadingStats.h"
#include "CrossPetSettings.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"
#include "util/ScreenshotUtil.h"
#include "activities/tools/ReadingStatsActivity.h"
#include "activities/tools/WeatherActivity.h"
#include "util/PowerButtonClickDetector.h"
#include <ArduinoJson.h>

HalDisplay display;
HalGPIO gpio;
// Global multi-click detector for power button (accessible via extern in reader activities)
PowerButtonClickDetector pwrClickDetector;
// Resolved power button action for the current frame (set in loop, read by reader activities)
static uint8_t currentPwrAction_ = 0;

uint8_t getPowerClickAction() {
  return currentPwrAction_;
}
MappedInputManager mappedInputManager(gpio);
GfxRenderer renderer(display);
ActivityManager activityManager(renderer, mappedInputManager);
FontDecompressor fontDecompressor;
FontCacheManager fontCacheManager(renderer.getFontMap());

// Fonts
EpdFont bookerly14RegularFont(&bookerly_14_regular);
EpdFont bookerly14BoldFont(&bookerly_14_bold);
EpdFont bookerly14ItalicFont(&bookerly_14_italic);
EpdFont bookerly14BoldItalicFont(&bookerly_14_bolditalic);
EpdFontFamily bookerly14FontFamily(&bookerly14RegularFont, &bookerly14BoldFont, &bookerly14ItalicFont,
                                   &bookerly14BoldItalicFont);
#ifndef OMIT_FONTS
EpdFont bookerly12RegularFont(&bookerly_12_regular);
EpdFont bookerly12BoldFont(&bookerly_12_bold);
EpdFont bookerly12ItalicFont(&bookerly_12_italic);
EpdFont bookerly12BoldItalicFont(&bookerly_12_bolditalic);
EpdFontFamily bookerly12FontFamily(&bookerly12RegularFont, &bookerly12BoldFont, &bookerly12ItalicFont,
                                   &bookerly12BoldItalicFont);
EpdFont bookerly16RegularFont(&bookerly_16_regular);
EpdFont bookerly16BoldFont(&bookerly_16_bold);
EpdFont bookerly16ItalicFont(&bookerly_16_italic);
EpdFont bookerly16BoldItalicFont(&bookerly_16_bolditalic);
EpdFontFamily bookerly16FontFamily(&bookerly16RegularFont, &bookerly16BoldFont, &bookerly16ItalicFont,
                                   &bookerly16BoldItalicFont);
EpdFont bookerly18RegularFont(&bookerly_18_regular);
EpdFont bookerly18BoldFont(&bookerly_18_bold);
EpdFont bookerly18ItalicFont(&bookerly_18_italic);
EpdFont bookerly18BoldItalicFont(&bookerly_18_bolditalic);
EpdFontFamily bookerly18FontFamily(&bookerly18RegularFont, &bookerly18BoldFont, &bookerly18ItalicFont,
                                   &bookerly18BoldItalicFont);

// Lexend fonts (no Italic available — uses Regular as fallback)
EpdFont lexend12RegularFont(&lexend_12_regular);
EpdFont lexend12BoldFont(&lexend_12_bold);
EpdFontFamily lexend12FontFamily(&lexend12RegularFont, &lexend12BoldFont, &lexend12RegularFont, &lexend12BoldFont);
EpdFont lexend14RegularFont(&lexend_14_regular);
EpdFont lexend14BoldFont(&lexend_14_bold);
EpdFontFamily lexend14FontFamily(&lexend14RegularFont, &lexend14BoldFont, &lexend14RegularFont, &lexend14BoldFont);
EpdFont lexend16RegularFont(&lexend_16_regular);
EpdFont lexend16BoldFont(&lexend_16_bold);
EpdFontFamily lexend16FontFamily(&lexend16RegularFont, &lexend16BoldFont, &lexend16RegularFont, &lexend16BoldFont);
EpdFont lexend18RegularFont(&lexend_18_regular);
EpdFont lexend18BoldFont(&lexend_18_bold);
EpdFontFamily lexend18FontFamily(&lexend18RegularFont, &lexend18BoldFont, &lexend18RegularFont, &lexend18BoldFont);

#endif  // OMIT_FONTS

// Bokerlam reader fonts (no BoldItalic available — uses Italic as fallback)
EpdFont bokerlam12RegularFont(&bokerlam_12_regular);
EpdFont bokerlam12BoldFont(&bokerlam_12_bold);
EpdFont bokerlam12ItalicFont(&bokerlam_12_italic);
EpdFontFamily bokerlam12FontFamily(&bokerlam12RegularFont, &bokerlam12BoldFont, &bokerlam12ItalicFont, &bokerlam12ItalicFont);
EpdFont bokerlam14RegularFont(&bokerlam_14_regular);
EpdFont bokerlam14BoldFont(&bokerlam_14_bold);
EpdFont bokerlam14ItalicFont(&bokerlam_14_italic);
EpdFontFamily bokerlam14FontFamily(&bokerlam14RegularFont, &bokerlam14BoldFont, &bokerlam14ItalicFont, &bokerlam14ItalicFont);
EpdFont bokerlam16RegularFont(&bokerlam_16_regular);
EpdFont bokerlam16BoldFont(&bokerlam_16_bold);
EpdFont bokerlam16ItalicFont(&bokerlam_16_italic);
EpdFontFamily bokerlam16FontFamily(&bokerlam16RegularFont, &bokerlam16BoldFont, &bokerlam16ItalicFont, &bokerlam16ItalicFont);
EpdFont bokerlam18RegularFont(&bokerlam_18_regular);
EpdFont bokerlam18BoldFont(&bokerlam_18_bold);
EpdFont bokerlam18ItalicFont(&bokerlam_18_italic);
EpdFontFamily bokerlam18FontFamily(&bokerlam18RegularFont, &bokerlam18BoldFont, &bokerlam18ItalicFont, &bokerlam18ItalicFont);

EpdFont smallFont(&notosans_8_regular);
EpdFontFamily smallFontFamily(&smallFont);

// UI fonts use NotoSans 12 (clean sans-serif, lighter than Lexend)
EpdFont notoUI12Regular(&notosans_12_regular);
EpdFont notoUI12Bold(&notosans_12_bold);
EpdFontFamily ui12FontFamily(&notoUI12Regular, &notoUI12Bold);

// RTC memory persists across deep sleep — used to restore clock with elapsed time correction.
// Magic sentinel confirms the values were set by a clean sleep entry (not stale/garbage).
static constexpr uint32_t SLEEP_RTC_MAGIC = 0x43524C4B;  // "CRLK"
RTC_DATA_ATTR static uint32_t g_rtcSleepMagic = 0;
RTC_DATA_ATTR static int64_t  g_rtcUsBeforeSleep = 0;    // esp_clk_rtc_time() before sleep
RTC_DATA_ATTR static uint32_t g_unixBeforeSleep = 0;     // time(nullptr) before sleep

// True when system clock was restored from backup (may have drift from deep sleep).
// Reset to false after a fresh NTP sync. Used by clock UI to show "~" approximate indicator.
bool g_clockApproximate = false;

// SNTP callback — clears approximate flag when NTP sync completes
static void onNtpSyncComplete(struct timeval* tv) {
  g_clockApproximate = false;
  LOG_DBG("NTP", "Time synced, clock is now accurate");
}

// SD-based clock backup — reliable fallback when RTC_DATA_ATTR is lost on ESP32-C3.
// Saves unix timestamp + RTC timer value. On wake, RTC timer is still running so we can
// compute elapsed sleep duration even without RTC memory.
static constexpr char CLOCK_BACKUP_PATH[] = "/.crosspoint/clock.bin";

struct ClockBackup {
  uint32_t unixTime;    // time(nullptr) at sleep entry
  int64_t  rtcTimeUs;   // esp_clk_rtc_time() at sleep entry
};

void saveClockToSD() {
  uint32_t now = (uint32_t)time(nullptr);
  if (now < 1700000000UL) return;  // don't save invalid time (before ~2023)
  ClockBackup backup = {now, esp_clk_rtc_time()};
  FsFile f;
  if (Storage.openFileForWrite("CLK", CLOCK_BACKUP_PATH, f)) {
    f.write((const uint8_t*)&backup, sizeof(backup));
    f.close();
  }
}

bool restoreClockFromSD() {
  FsFile f;
  if (!Storage.openFileForRead("CLK", CLOCK_BACKUP_PATH, f)) return false;
  ClockBackup backup = {};
  bool ok = (f.read((uint8_t*)&backup, sizeof(backup)) == sizeof(backup));
  f.close();
  if (!ok || backup.unixTime < 1700000000UL) return false;

  // Compute elapsed time using RTC timer (keeps running during deep sleep)
  int64_t rtcNow = esp_clk_rtc_time();
  uint32_t elapsedSec = 0;
  if (rtcNow > backup.rtcTimeUs) {
    elapsedSec = (uint32_t)((rtcNow - backup.rtcTimeUs) / 1000000LL);
  }
  time_t corrected = (time_t)backup.unixTime + elapsedSec;
  struct timeval tv = {corrected, 0};
  settimeofday(&tv, nullptr);
  LOG_DBG("MAIN", "Clock restored from SD: base=%lu + %us elapsed", (unsigned long)backup.unixTime, elapsedSec);
  return true;
}

// measurement of power button press duration calibration value
unsigned long t1 = 0;
unsigned long t2 = 0;

// Forward declaration — defined later in setup section
void setupDisplayAndFonts();

// Verify power button press duration on wake-up from deep sleep
// Pre-condition: isWakeupByPowerButton() == true
void verifyPowerButtonDuration() {
  if (SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::SLEEP) {
    // Fast path for short press
    // Needed because inputManager.isPressed() may take up to ~500ms to return the correct state
    return;
  }

  // Give the user up to 1000ms to start holding the power button, and must hold for SETTINGS.getPowerButtonDuration()
  const auto start = millis();
  bool abort = false;
  // Subtract the current time, because inputManager only starts counting the HeldTime from the first update()
  // This way, we remove the time we already took to reach here from the duration,
  // assuming the button was held until now from millis()==0 (i.e. device start time).
  const uint16_t calibration = start;
  const uint16_t calibratedPressDuration =
      (calibration < SETTINGS.getPowerButtonDuration()) ? SETTINGS.getPowerButtonDuration() - calibration : 1;

  gpio.update();
  // Needed because inputManager.isPressed() may take up to ~500ms to return the correct state
  while (!gpio.isPressed(HalGPIO::BTN_POWER) && millis() - start < 1000) {
    delay(10);  // only wait 10ms each iteration to not delay too much in case of short configured duration.
    gpio.update();
  }

  t2 = millis();
  if (gpio.isPressed(HalGPIO::BTN_POWER)) {
    do {
      delay(10);
      gpio.update();
    } while (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() < calibratedPressDuration);
    abort = gpio.getHeldTime() < calibratedPressDuration;
  } else {
    abort = true;
  }

  if (abort) {
    // Button released too early. Returning to sleep.
    // Refresh dynamic sleep screens (clock, reading stats) so the display isn't stale.
    const auto mode = SETTINGS.sleepScreen;
    if (mode == CrossPointSettings::SLEEP_SCREEN_MODE::CLOCK ||
        mode == CrossPointSettings::SLEEP_SCREEN_MODE::READING_STATS) {
      setupDisplayAndFonts();
      activityManager.goToSleep();
    }
    // IMPORTANT: Re-arm the wakeup trigger before sleeping again
    powerManager.startDeepSleep(gpio, SETTINGS.keepClockAlive != 0 && PET_SETTINGS.appClock,
                                CrossPointSettings::getSleepRefreshMinutes(SETTINGS.sleepRefreshInterval));
  }
}

void waitForPowerRelease() {
  gpio.update();
  while (gpio.isPressed(HalGPIO::BTN_POWER)) {
    delay(50);
    gpio.update();
  }
}

// Enter deep sleep mode
void enterDeepSleep() {
  HalPowerManager::Lock powerLock;  // Ensure we are at normal CPU frequency for sleep preparation
  APP_STATE.lastSleepFromReader = activityManager.isReaderActivity();
  APP_STATE.saveToFile();

  activityManager.goToSleep();


  display.deepSleep();
  LOG_DBG("MAIN", "Power button press calibration value: %lu ms", t2 - t1);
  LOG_DBG("MAIN", "Entering deep sleep");

  // Snapshot time + RTC counter for accurate clock restoration on wake.
  // The LP/RTC timer (esp_clk_rtc_time) keeps running during deep sleep.
  g_unixBeforeSleep  = (uint32_t)time(nullptr);
  g_rtcUsBeforeSleep = esp_clk_rtc_time();
  g_rtcSleepMagic    = SLEEP_RTC_MAGIC;

  // Also save to SD as reliable fallback (RTC_DATA_ATTR can be lost on some ESP32-C3 boards)
  saveClockToSD();

  powerManager.startDeepSleep(gpio, SETTINGS.keepClockAlive != 0 && PET_SETTINGS.appClock,
                                CrossPointSettings::getSleepRefreshMinutes(SETTINGS.sleepRefreshInterval));
}

void setupDisplayAndFonts() {
  display.begin();
  renderer.begin();
  activityManager.begin();
  LOG_DBG("MAIN", "Display initialized");

  // Initialize font decompressor for compressed reader fonts
  if (!fontDecompressor.init()) {
    LOG_ERR("MAIN", "Font decompressor init failed");
  }
  fontCacheManager.setFontDecompressor(&fontDecompressor);
  renderer.setFontCacheManager(&fontCacheManager);
  renderer.insertFont(BOOKERLY_14_FONT_ID, bookerly14FontFamily);
#ifndef OMIT_FONTS
  renderer.insertFont(BOOKERLY_12_FONT_ID, bookerly12FontFamily);
  renderer.insertFont(BOOKERLY_16_FONT_ID, bookerly16FontFamily);
  renderer.insertFont(BOOKERLY_18_FONT_ID, bookerly18FontFamily);

  renderer.insertFont(LEXEND_12_FONT_ID, lexend12FontFamily);
  renderer.insertFont(LEXEND_14_FONT_ID, lexend14FontFamily);
  renderer.insertFont(LEXEND_16_FONT_ID, lexend16FontFamily);
  renderer.insertFont(LEXEND_18_FONT_ID, lexend18FontFamily);
  renderer.insertFont(BOKERLAM_12_FONT_ID, bokerlam12FontFamily);
  renderer.insertFont(BOKERLAM_14_FONT_ID, bokerlam14FontFamily);
  renderer.insertFont(BOKERLAM_16_FONT_ID, bokerlam16FontFamily);
  renderer.insertFont(BOKERLAM_18_FONT_ID, bokerlam18FontFamily);
#endif  // OMIT_FONTS
  renderer.insertFont(UI_10_FONT_ID, ui12FontFamily);
  renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);
  renderer.insertFont(SMALL_FONT_ID, smallFontFamily);
  LOG_DBG("MAIN", "Fonts setup");
}

void setup() {
  t1 = millis();

  HalSystem::begin();
  gpio.begin();
  powerManager.begin();

  // Only start serial if USB connected
  if (gpio.isUsbConnected()) {
    Serial.begin(115200);
    // Wait up to 3 seconds for Serial to be ready to catch early logs
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
      delay(10);
    }
  }

  // SD Card Initialization
  // We need 6 open files concurrently when parsing a new chapter
  if (!Storage.begin()) {
    LOG_ERR("MAIN", "SD card initialization failed");
    setupDisplayAndFonts();
    activityManager.goToFullScreenMessage("SD card error", EpdFontFamily::BOLD);
    return;
  }

  HalSystem::checkPanic();
  HalSystem::clearPanic();  // TODO: move this to an activity when we have one to display the panic info

  SETTINGS.loadFromFile();
  PET_SETTINGS.loadFromFile();
  I18N.loadSettings();
  KOREADER_STORE.loadFromFile();
  READ_STATS.loadFromFile();   // loaded early so abort-to-sleep paths show correct stats
  GAME_SCORES.loadFromFile();
  UITheme::getInstance().reload();
  ButtonNavigator::setMappedInputManager(mappedInputManager);

  // Register NTP sync callback to clear approximate clock flag
  sntp_set_time_sync_notification_cb(onNtpSyncComplete);

  // Restore system clock after deep sleep.
  // Strategy 1: RTC memory + LP timer elapsed correction (most accurate)
  // Strategy 2: SD card backup (reliable fallback when RTC_DATA_ATTR lost on ESP32-C3)
  // Strategy 3: Pet state savedTime (last resort, handled in PetPersistence::load())
  {
    time_t now = time(nullptr);
    struct tm check;
    gmtime_r(&now, &check);
    bool clockValid = (check.tm_year >= 125);  // year >= 2025

    if (!clockValid && g_rtcSleepMagic == SLEEP_RTC_MAGIC && g_unixBeforeSleep > 0) {
      // Strategy 1: RTC memory with elapsed time correction
      int64_t rtcNow      = esp_clk_rtc_time();
      uint32_t elapsedSec = 0;
      if (rtcNow > g_rtcUsBeforeSleep) {
        elapsedSec = (uint32_t)((rtcNow - g_rtcUsBeforeSleep) / 1000000LL);
      }
      time_t corrected = (time_t)g_unixBeforeSleep + elapsedSec;
      struct timeval tv = {corrected, 0};
      settimeofday(&tv, nullptr);
      clockValid = true;
      g_clockApproximate = true;  // RTC timer drifts during deep sleep
      LOG_DBG("MAIN", "Clock restored via RTC: base=%lu + %us elapsed", g_unixBeforeSleep, elapsedSec);
    }

    if (!clockValid) {
      // Strategy 2: SD card backup (time will be slightly behind by sleep duration)
      clockValid = restoreClockFromSD();
      if (clockValid) g_clockApproximate = true;
    }

    g_rtcSleepMagic = 0;  // consume — next boot treats as cold boot unless we sleep again
  }

  // Load cached timezone from weather data, fallback to ICT-7 (Indochina Time).
  // This must happen after settimeofday() and before any localtime_r() calls.
  {
    char cachedTz[16] = "ICT-7";
    String content = Storage.readFile("/.crosspoint/weather_cache.json");
    if (!content.isEmpty()) {
      JsonDocument tzDoc;
      if (!deserializeJson(tzDoc, content)) {
        const char* tz = tzDoc["tz"] | "";
        if (tz[0]) strncpy(cachedTz, tz, sizeof(cachedTz) - 1);
      }
    }
    setenv("TZ", cachedTz, 1);
    tzset();
  }


  switch (gpio.getWakeupReason()) {
    case HalGPIO::WakeupReason::PowerButton:
      // For normal wakeups, verify power button press duration
      LOG_DBG("MAIN", "Verifying power button press duration");
      verifyPowerButtonDuration();
      break;
    case HalGPIO::WakeupReason::TimerWake: {
      // Periodic sleep screen refresh — minimal boot path, no full UI
      LOG_DBG("MAIN", "Timer wake: refreshing sleep screen");
      const auto mode = SETTINGS.sleepScreen;
      if (mode == CrossPointSettings::SLEEP_SCREEN_MODE::CLOCK) {
        setupDisplayAndFonts();
        activityManager.goToSleep();
      }
      saveClockToSD();
      powerManager.startDeepSleep(gpio, SETTINGS.keepClockAlive != 0 && PET_SETTINGS.appClock,
                                  CrossPointSettings::getSleepRefreshMinutes(SETTINGS.sleepRefreshInterval));
      break;  // unreachable — startDeepSleep never returns
    }
    case HalGPIO::WakeupReason::AfterUSBPower:
      // If USB power caused a cold boot, go back to sleep
      LOG_DBG("MAIN", "Wakeup reason: After USB Power");
      powerManager.startDeepSleep(gpio, SETTINGS.keepClockAlive != 0 && PET_SETTINGS.appClock,
                                  CrossPointSettings::getSleepRefreshMinutes(SETTINGS.sleepRefreshInterval));
      break;
    case HalGPIO::WakeupReason::AfterFlash:
      // After flashing, just proceed to boot
    case HalGPIO::WakeupReason::Other:
    default:
      break;
  }

  // First serial output only here to avoid timing inconsistencies for power button press duration verification
  LOG_DBG("MAIN", "Starting CrossPoint version " CROSSPOINT_VERSION);

  setupDisplayAndFonts();

  activityManager.goToBoot();

  APP_STATE.loadFromFile();
  RECENT_BOOKS.loadFromFile();

  // Boot to home screen if no book is open, last sleep was not from reader, back button is held, or reader activity
  // crashed (indicated by readerActivityLoadCount > 0)
  if (APP_STATE.openEpubPath.empty() || !APP_STATE.lastSleepFromReader ||
      mappedInputManager.isPressed(MappedInputManager::Button::Back) || APP_STATE.readerActivityLoadCount > 0) {
    activityManager.goHome();
  } else {
    // Clear app state to avoid getting into a boot loop if the epub doesn't load
    const auto path = APP_STATE.openEpubPath;
    APP_STATE.openEpubPath = "";
    APP_STATE.readerActivityLoadCount++;
    APP_STATE.saveToFile();
    activityManager.goToReader(path);
  }

  // Ensure we're not still holding the power button before leaving setup.
  waitForPowerRelease();
}

void loop() {
  static unsigned long maxLoopDuration = 0;
  const unsigned long loopStartTime = millis();
  static unsigned long lastMemPrint = 0;

  gpio.update();

  renderer.setFadingFix(SETTINGS.fadingFix);
  {
    static uint8_t lastDarkMode = 0xFF;
    if (SETTINGS.darkMode != lastDarkMode) {
      renderer.setDarkMode(SETTINGS.darkMode);
      lastDarkMode = SETTINGS.darkMode;
    }
    // textDarkness is only applied in reader activities, not globally,
    // so UI text stays at normal weight (0).
  }

  if (Serial && millis() - lastMemPrint >= 10000) {
    LOG_INF("MEM", "Free: %d bytes, Total: %d bytes, Min Free: %d bytes, MaxAlloc: %d bytes", ESP.getFreeHeap(),
            ESP.getHeapSize(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    lastMemPrint = millis();
  }

  // Handle incoming serial commands,
  // nb: we use logSerial from logging to avoid deprecation warnings
  if (logSerial.available() > 0) {
    String line = logSerial.readStringUntil('\n');
    if (line.startsWith("CMD:")) {
      String cmd = line.substring(4);
      cmd.trim();
      if (cmd == "SCREENSHOT") {
        logSerial.printf("SCREENSHOT_START:%d\n", HalDisplay::BUFFER_SIZE);
        uint8_t* buf = display.getFrameBuffer();
        logSerial.write(buf, HalDisplay::BUFFER_SIZE);
        logSerial.printf("SCREENSHOT_END\n");
      }
    }
  }

  // Check for any user activity (button press or release) or active background work
  static unsigned long lastActivityTime = millis();
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || activityManager.preventAutoSleep()) {
    lastActivityTime = millis();         // Reset inactivity timer
    powerManager.setPowerSaving(false);  // Restore normal CPU frequency on user activity
  }

  static bool screenshotButtonsReleased = true;
  if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.isPressed(HalGPIO::BTN_DOWN)) {
    if (screenshotButtonsReleased) {
      screenshotButtonsReleased = false;
      {
        RenderLock lock;
        ScreenshotUtil::takeScreenshot(renderer);
      }
    }
    return;
  } else {
    screenshotButtonsReleased = true;
  }

  const unsigned long sleepTimeoutMs = SETTINGS.getSleepTimeoutMs();
  if (millis() - lastActivityTime >= sleepTimeoutMs) {
    LOG_DBG("SLP", "Auto-sleep triggered after %lu ms of inactivity", sleepTimeoutMs);
    enterDeepSleep();
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() > SETTINGS.getPowerButtonDuration()) {
    // If the screenshot combination is potentially being pressed, don't sleep
    if (gpio.isPressed(HalGPIO::BTN_DOWN)) {
      return;
    }
    enterDeepSleep();
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  // Multi-click power button detection (single/double/triple click)
  // Resolve action once per frame — reader activities read it via getPowerClickAction()
  pwrClickDetector.update(gpio.wasReleased(HalGPIO::BTN_POWER));
  {
    const uint8_t clicks = pwrClickDetector.getClickCount();
    currentPwrAction_ = (clicks == 0) ? CrossPointSettings::SHORT_PWRBTN::IGNORE
                       : (clicks == 1) ? SETTINGS.shortPwrBtn
                       : (clicks == 2) ? SETTINGS.shortPwrBtn2Click
                                       : SETTINGS.shortPwrBtn3Click;
  }
  // Handle global power button actions (reader-specific ones handled inside reader activities)
  switch (currentPwrAction_) {
    case CrossPointSettings::SHORT_PWRBTN::SCREEN_REFRESH:
      renderer.requestNextHalfRefresh();
      activityManager.requestUpdate();
      break;
    case CrossPointSettings::SHORT_PWRBTN::READING_STATS_VIEW:
      activityManager.pushActivity(std::make_unique<ReadingStatsActivity>(renderer, mappedInputManager));
      break;
    case CrossPointSettings::SHORT_PWRBTN::SYNC_WEATHER_TIME: {
      LOG_INF("PWR", "Sync weather/time triggered");
      int syncResult = WeatherActivity::silentRefresh();
      LOG_INF("PWR", "Sync result: %d (0=ok, 1=no creds, 2=timeout, 4=api, 5=parse)", syncResult);
      // Refresh screen after time/weather sync to show updated clock
      renderer.requestNextHalfRefresh();
      activityManager.requestUpdate();
      break;
    }
    default:
      break;  // PAGE_TURN, BLOCK_FRONT, STAR_PAGE handled by reader activities
  }

  // Refresh battery icon when USB is plugged or unplugged
  if (gpio.wasUsbStateChanged()) {
    activityManager.requestUpdate();
  }

  const unsigned long activityStartTime = millis();
  activityManager.loop();
  const unsigned long activityDuration = millis() - activityStartTime;

  const unsigned long loopDuration = millis() - loopStartTime;
  if (loopDuration > maxLoopDuration) {
    maxLoopDuration = loopDuration;
    if (maxLoopDuration > 50) {
      LOG_DBG("LOOP", "New max loop duration: %lu ms (activity: %lu ms)", maxLoopDuration, activityDuration);
    }
  }

  // Add delay at the end of the loop to prevent tight spinning
  // When an activity requests skip loop delay (e.g., webserver running), use yield() for faster response
  // Otherwise, use longer delay to save power
  if (activityManager.skipLoopDelay()) {
    powerManager.setPowerSaving(false);  // Make sure we're at full performance when skipLoopDelay is requested
    yield();                             // Give FreeRTOS a chance to run tasks, but return immediately
  } else {
    if (millis() - lastActivityTime >= HalPowerManager::IDLE_POWER_SAVING_MS) {
      // If we've been inactive for a while, increase the delay to save power
      powerManager.setPowerSaving(true);  // Lower CPU frequency after extended inactivity
      delay(50);
    } else {
      // Short delay to prevent tight loop while still being responsive
      delay(10);
    }
  }
}