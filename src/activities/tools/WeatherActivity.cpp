#include "WeatherActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include "CrossPointSettings.h"
#include "WifiCredentialStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "ble/BleRemoteManager.h"
#include "components/UITheme.h"
#include "components/icons/weather.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

extern BleRemoteManager bleManager;

// All 63 provinces/cities of Vietnam, sorted alphabetically
const CityCoord WeatherActivity::CITIES[CITY_COUNT] = {
    {"An Giang", "10.3899", "105.4353"},
    {"Bà Rịa-Vũng Tàu", "10.5417", "107.2430"},
    {"Bắc Giang", "21.2731", "106.1946"},
    {"Bắc Kạn", "22.1443", "105.8348"},
    {"Bạc Liêu", "9.2940", "105.7216"},
    {"Bắc Ninh", "21.1861", "106.0763"},
    {"Bến Tre", "10.2434", "106.3756"},
    {"Bình Định", "13.7820", "109.2197"},
    {"Bình Dương", "11.3254", "106.4770"},
    {"Bình Phước", "11.7512", "106.7235"},
    {"Bình Thuận", "10.9282", "108.1002"},
    {"Cà Mau", "9.1527", "105.1961"},
    {"Cần Thơ", "10.0452", "105.7469"},
    {"Cao Bằng", "22.6666", "106.2578"},
    {"Đà Nẵng", "16.0544", "108.2022"},
    {"Đắk Lắk", "12.7100", "108.2378"},
    {"Đắk Nông", "12.2646", "107.6098"},
    {"Điện Biên", "21.3860", "103.0230"},
    {"Đồng Nai", "10.9453", "106.8243"},
    {"Đồng Tháp", "10.4938", "105.6882"},
    {"Gia Lai", "13.9832", "108.0025"},
    {"Hà Giang", "22.8233", "104.9836"},
    {"Hà Nam", "20.5835", "105.9230"},
    {"Hà Nội", "21.0285", "105.8542"},
    {"Hà Tĩnh", "18.3559", "105.8877"},
    {"Hải Dương", "20.9373", "106.3147"},
    {"Hải Phòng", "20.8449", "106.6881"},
    {"Hậu Giang", "9.7579", "105.6413"},
    {"TP HCM", "10.8231", "106.6297"},
    {"Hòa Bình", "20.8171", "105.3382"},
    {"Hưng Yên", "20.6464", "106.0511"},
    {"Khánh Hòa", "12.2585", "109.0526"},
    {"Kiên Giang", "10.0125", "105.0809"},
    {"Kon Tum", "14.3498", "108.0005"},
    {"Lai Châu", "22.3862", "103.4703"},
    {"Lâm Đồng", "11.9465", "108.4419"},
    {"Lạng Sơn", "21.8539", "106.7615"},
    {"Lào Cai", "22.3380", "104.1487"},
    {"Long An", "10.5360", "106.4131"},
    {"Nam Định", "20.4388", "106.1621"},
    {"Nghệ An", "18.6727", "105.6929"},
    {"Ninh Bình", "20.2506", "105.9745"},
    {"Ninh Thuận", "11.5752", "108.9890"},
    {"Phú Thọ", "21.4225", "105.2296"},
    {"Phú Yên", "13.0882", "109.0929"},
    {"Quảng Bình", "17.4690", "106.6222"},
    {"Quảng Nam", "15.5394", "108.0191"},
    {"Quảng Ngãi", "15.1215", "108.8044"},
    {"Quảng Ninh", "21.0064", "107.2925"},
    {"Quảng Trị", "16.7500", "107.1856"},
    {"Sóc Trăng", "9.6037", "105.9800"},
    {"Sơn La", "21.3256", "103.9188"},
    {"Tây Ninh", "11.3352", "106.1099"},
    {"Thái Bình", "20.4463", "106.3366"},
    {"Thái Nguyên", "21.5942", "105.8482"},
    {"Thanh Hóa", "19.8067", "105.7852"},
    {"Thừa Thiên Huế", "16.4674", "107.5905"},
    {"Tiền Giang", "10.3600", "106.3630"},
    {"Trà Vinh", "9.9347", "106.3455"},
    {"Tuyên Quang", "21.8269", "105.2181"},
    {"Vĩnh Long", "10.2538", "105.9722"},
    {"Vĩnh Phúc", "21.3609", "105.5474"},
    {"Yên Bái", "21.7168", "104.8986"},
};

// Try silent WiFi connect using saved credentials (no UI)
static bool trySilentWifiConnect() {
  const auto& ssid = WIFI_STORE.getLastConnectedSsid();
  if (ssid.empty()) return false;

  const auto* cred = WIFI_STORE.findCredential(ssid);
  if (!cred) return false;

  WiFi.begin(cred->ssid.c_str(), cred->password.c_str());

  // Wait up to 10 seconds for connection
  for (int i = 0; i < 100; i++) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(100);
  }
  WiFi.disconnect(false);
  return false;
}

void WeatherActivity::onEnter() {
  Activity::onEnter();
  selectedCity = SETTINGS.weatherCity;
  if (selectedCity > CITY_COUNT) selectedCity = 0;  // 0=Auto, 1..3=manual
  lastUpdateTime[0] = '\0';
  detectedCityName[0] = '\0';
  detectedLat[0] = '\0';
  detectedLon[0] = '\0';

  bleManager.suspend();
  state = WIFI_CONNECTING;
  statusMessage = tr(STR_FETCHING_WEATHER);
  requestUpdate(true);

  if (WiFi.status() == WL_CONNECTED) {
    onWifiConnected();
  } else if (trySilentWifiConnect()) {
    onWifiConnected();
  } else {
    startActivityForResult(
        std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
        [this](const ActivityResult& r) {
          if (r.isCancelled) {
            finish();
            return;
          }
          onWifiConnected();
        });
  }
}

void WeatherActivity::onWifiConnected() {
  configTzTime("ICT-7", "pool.ntp.org", "time.google.com");

  // Auto-detect location by IP if selectedCity == 0
  if (selectedCity == 0) {
    state = FETCHING;
    statusMessage = tr(STR_FETCHING_WEATHER);
    requestUpdate(true);
    if (!detectLocation()) {
      // Fallback to first manual city if geolocation fails
      LOG_ERR("WEATHER", "IP geolocation failed, falling back to manual city");
      selectedCity = 1;
    }
  }

  state = FETCHING;
  statusMessage = tr(STR_FETCHING_WEATHER);
  requestUpdate(true);
  fetchWeather();
}

bool WeatherActivity::detectLocation() {
  std::string response;
  if (!HttpDownloader::fetchUrl("http://ip-api.com/json/?fields=lat,lon,city", response)) {
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) return false;

  float lat = doc["lat"] | 0.0f;
  float lon = doc["lon"] | 0.0f;
  const char* city = doc["city"] | "";

  if (lat == 0.0f && lon == 0.0f) return false;

  snprintf(detectedLat, sizeof(detectedLat), "%.4f", lat);
  snprintf(detectedLon, sizeof(detectedLon), "%.4f", lon);
  strncpy(detectedCityName, city, sizeof(detectedCityName) - 1);
  detectedCityName[sizeof(detectedCityName) - 1] = '\0';

  LOG_DBG("WEATHER", "IP geolocation: %s (%.4f, %.4f)", detectedCityName, lat, lon);
  return true;
}

const char* WeatherActivity::getCurrentCityName() const {
  if (selectedCity == 0) {
    return detectedCityName[0] ? detectedCityName : "Auto";
  }
  return CITIES[selectedCity - 1].name;
}

void WeatherActivity::fetchWeather() {
  const char* lat;
  const char* lon;

  if (selectedCity == 0 && detectedLat[0]) {
    lat = detectedLat;
    lon = detectedLon;
  } else {
    int idx = (selectedCity > 0) ? selectedCity - 1 : 0;
    lat = CITIES[idx].lat;
    lon = CITIES[idx].lon;
  }

  char url[384];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?"
           "latitude=%s&longitude=%s"
           "&current=temperature_2m,relative_humidity_2m,"
           "apparent_temperature,weather_code,wind_speed_10m"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min"
           "&forecast_days=6",
           lat, lon);

  std::string response;
  if (!HttpDownloader::fetchUrl(std::string(url), response)) {
    state = FETCH_ERROR;
    statusMessage = tr(STR_WEATHER_ERROR);
    requestUpdate(true);
    return;
  }

  if (!parseWeather(response)) {
    state = FETCH_ERROR;
    statusMessage = tr(STR_WEATHER_ERROR);
    requestUpdate(true);
    return;
  }

  time_t now;
  time(&now);
  struct tm ti;
  localtime_r(&now, &ti);
  snprintf(lastUpdateTime, sizeof(lastUpdateTime), "%02d:%02d", ti.tm_hour, ti.tm_min);

  saveWeatherCache();

  state = DISPLAYING;
  requestUpdate(true);
}

bool WeatherActivity::parseWeather(const std::string& json) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("WEATHER", "JSON parse error: %s", error.c_str());
    return false;
  }

  JsonObject current = doc["current"];
  if (current.isNull()) return false;

  weather.temperature = current["temperature_2m"] | 0.0f;
  weather.feelsLike = current["apparent_temperature"] | 0.0f;
  weather.humidity = current["relative_humidity_2m"] | 0;
  weather.weatherCode = current["weather_code"] | 0;
  weather.windSpeed = current["wind_speed_10m"] | 0.0f;

  // Parse daily forecast (skip day 0 = today, show next 5 days)
  forecastCount = 0;
  JsonObject daily = doc["daily"];
  if (!daily.isNull()) {
    static const char* DAY_NAMES[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    JsonArray codes = daily["weather_code"];
    JsonArray maxTemps = daily["temperature_2m_max"];
    JsonArray minTemps = daily["temperature_2m_min"];
    JsonArray dates = daily["time"];
    for (int i = 1; i < (int)codes.size() && forecastCount < FORECAST_DAYS; i++) {
      auto& f = forecast[forecastCount];
      f.weatherCode = codes[i] | 0;
      f.tempMax = maxTemps[i] | 0.0f;
      f.tempMin = minTemps[i] | 0.0f;
      // Parse day of week from date string "YYYY-MM-DD"
      const char* dateStr = dates[i] | "";
      struct tm tm = {};
      if (sscanf(dateStr, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
        tm.tm_year -= 1900; tm.tm_mon -= 1;
        mktime(&tm);
        strncpy(f.dayLabel, DAY_NAMES[tm.tm_wday], sizeof(f.dayLabel) - 1);
      }
      forecastCount++;
    }
  }
  return true;
}

const char* WeatherActivity::weatherCodeToString(int code) {
  if (code == 0) return tr(STR_WEATHER_CLEAR);
  if (code <= 3) return tr(STR_WEATHER_PARTLY_CLOUDY);
  if (code <= 48) return tr(STR_WEATHER_FOG);
  if (code <= 57) return tr(STR_WEATHER_DRIZZLE);
  if (code <= 67) return tr(STR_WEATHER_RAIN);
  if (code <= 77) return tr(STR_WEATHER_SNOW);
  if (code <= 82) return tr(STR_WEATHER_SHOWERS);
  return tr(STR_WEATHER_THUNDERSTORM);
}

void WeatherActivity::saveWeatherCache() {
  JsonDocument doc;
  doc["temp"] = weather.temperature;
  doc["feels"] = weather.feelsLike;
  doc["hum"] = weather.humidity;
  doc["code"] = weather.weatherCode;
  doc["wind"] = weather.windSpeed;
  doc["city"] = selectedCity;
  doc["time"] = lastUpdateTime;
  if (detectedCityName[0]) doc["autoCity"] = detectedCityName;
  if (detectedLat[0]) {
    doc["autoLat"] = detectedLat;
    doc["autoLon"] = detectedLon;
  }

  String json;
  serializeJson(doc, json);
  Storage.ensureDirectoryExists("/.crosspoint");
  Storage.writeFile("/.crosspoint/weather_cache.json", json);
}

bool WeatherActivity::loadWeatherCache(WeatherData& out, uint8_t& cityIdx, char* timeBuf, size_t timeBufLen) {
  String content = Storage.readFile("/.crosspoint/weather_cache.json");
  if (content.isEmpty()) return false;

  JsonDocument doc;
  if (deserializeJson(doc, content)) return false;

  out.temperature = doc["temp"] | 0.0f;
  out.feelsLike = doc["feels"] | 0.0f;
  out.humidity = doc["hum"] | 0;
  out.weatherCode = doc["code"] | 0;
  out.windSpeed = doc["wind"] | 0.0f;
  cityIdx = doc["city"] | (uint8_t)0;
  if (cityIdx > CITY_COUNT) cityIdx = 0;

  const char* t = doc["time"] | "";
  strncpy(timeBuf, t, timeBufLen - 1);
  timeBuf[timeBufLen - 1] = '\0';
  return true;
}

bool WeatherActivity::silentRefresh() {
  if (WiFi.status() != WL_CONNECTED && !trySilentWifiConnect()) return false;

  configTzTime("ICT-7", "pool.ntp.org", "time.google.com");

  uint8_t city = SETTINGS.weatherCity;
  if (city > CITY_COUNT) city = 0;

  // Determine coordinates
  const char* lat = nullptr;
  const char* lon = nullptr;
  char autoLat[16] = "";
  char autoLon[16] = "";
  char autoCity[32] = "";

  if (city == 0) {
    // Try IP geolocation
    std::string geoResponse;
    if (HttpDownloader::fetchUrl("http://ip-api.com/json/?fields=lat,lon,city", geoResponse)) {
      JsonDocument geoDoc;
      if (!deserializeJson(geoDoc, geoResponse)) {
        float gLat = geoDoc["lat"] | 0.0f;
        float gLon = geoDoc["lon"] | 0.0f;
        const char* gCity = geoDoc["city"] | "";
        if (gLat != 0.0f || gLon != 0.0f) {
          snprintf(autoLat, sizeof(autoLat), "%.4f", gLat);
          snprintf(autoLon, sizeof(autoLon), "%.4f", gLon);
          strncpy(autoCity, gCity, sizeof(autoCity) - 1);
          lat = autoLat;
          lon = autoLon;
        }
      }
    }
    if (!lat) { lat = CITIES[0].lat; lon = CITIES[0].lon; }
  } else {
    lat = CITIES[city - 1].lat;
    lon = CITIES[city - 1].lon;
  }

  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?"
           "latitude=%s&longitude=%s"
           "&current=temperature_2m,relative_humidity_2m,"
           "apparent_temperature,weather_code,wind_speed_10m",
           lat, lon);

  std::string response;
  if (!HttpDownloader::fetchUrl(std::string(url), response)) {
    WiFi.disconnect(false); delay(100); WiFi.mode(WIFI_OFF);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) { WiFi.disconnect(false); WiFi.mode(WIFI_OFF); return false; }
  JsonObject current = doc["current"];
  if (current.isNull()) { WiFi.disconnect(false); WiFi.mode(WIFI_OFF); return false; }

  JsonDocument out;
  out["temp"]  = current["temperature_2m"] | 0.0f;
  out["feels"] = current["apparent_temperature"] | 0.0f;
  out["hum"]   = current["relative_humidity_2m"] | 0;
  out["code"]  = current["weather_code"] | 0;
  out["wind"]  = current["wind_speed_10m"] | 0.0f;
  out["city"]  = city;
  if (autoCity[0]) out["autoCity"] = autoCity;
  if (autoLat[0]) { out["autoLat"] = autoLat; out["autoLon"] = autoLon; }

  time_t now; time(&now);
  struct tm ti; localtime_r(&now, &ti);
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
  out["time"] = timeBuf;

  String json;
  serializeJson(out, json);
  Storage.ensureDirectoryExists("/.crosspoint");
  Storage.writeFile("/.crosspoint/weather_cache.json", json);

  WiFi.disconnect(false); delay(100); WiFi.mode(WIFI_OFF);
  return true;
}

void WeatherActivity::onExit() {
  SETTINGS.weatherCity = selectedCity;
  SETTINGS.saveToFile();
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
  bleManager.resume();
  Activity::onExit();
}

void WeatherActivity::loop() {
  auto back = mappedInput.wasReleased(MappedInputManager::Button::Back);
  auto confirm = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  auto next = mappedInput.wasReleased(MappedInputManager::Button::Right);
  auto prev = mappedInput.wasReleased(MappedInputManager::Button::Left);
  auto up = mappedInput.wasReleased(MappedInputManager::Button::Up);
  auto down = mappedInput.wasReleased(MappedInputManager::Button::Down);

  if (state == SELECTING_CITY) {
    int total = CITY_COUNT + 1;  // 0=Auto + 63 cities
    if (back) {
      state = DISPLAYING;
      requestUpdate();
      return;
    }
    if (down || next) {
      cityCursor = (cityCursor + 1) % total;
      // Keep cursor visible in scroll window
      if (cityCursor < cityScrollTop) cityScrollTop = cityCursor;
      if (cityCursor >= cityScrollTop + 10) cityScrollTop = cityCursor - 9;
      requestUpdate();
    }
    if (up || prev) {
      cityCursor = (cityCursor + total - 1) % total;
      if (cityCursor < cityScrollTop) cityScrollTop = cityCursor;
      if (cityCursor >= cityScrollTop + 10) cityScrollTop = cityCursor - 9;
      requestUpdate();
    }
    if (confirm) {
      selectedCity = cityCursor;
      if (selectedCity == 0 && detectedLat[0] == '\0') detectLocation();
      state = FETCHING;
      statusMessage = tr(STR_FETCHING_WEATHER);
      requestUpdate(true);
      fetchWeather();
    }
    return;
  }

  if (back) {
    finish();
    return;
  }

  if (state == DISPLAYING || state == FETCH_ERROR) {
    if (confirm) {
      state = FETCHING;
      statusMessage = tr(STR_FETCHING_WEATHER);
      requestUpdate(true);
      fetchWeather();
      return;
    }
    if (next || prev) {
      // Open city selection list
      cityCursor = selectedCity;
      cityScrollTop = (cityCursor > 5) ? cityCursor - 5 : 0;
      state = SELECTING_CITY;
      requestUpdate();
    }
  }
}

void WeatherActivity::renderCityList() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CITY));

  int y = metrics.topPadding + metrics.headerHeight + 10;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  const int total = CITY_COUNT + 1;
  const int visibleCount = 10;

  for (int i = 0; i < visibleCount && (cityScrollTop + i) < total; i++) {
    int idx = cityScrollTop + i;
    const char* name = (idx == 0) ? "Auto" : CITIES[idx - 1].name;

    if (idx == cityCursor) {
      // Highlight selected row
      renderer.fillRect(5, y - 2, pageWidth - 10, lineH, true);
      renderer.drawText(UI_10_FONT_ID, 15, y, name, false);  // white text on black
      if (idx == selectedCity) {
        renderer.drawText(UI_10_FONT_ID, pageWidth - 40, y, "*", false);
      }
    } else {
      renderer.drawText(UI_10_FONT_ID, 15, y, name, true);
      if (idx == selectedCity) {
        renderer.drawText(UI_10_FONT_ID, pageWidth - 40, y, "*", true);
      }
    }
    y += lineH;
  }

  // Scroll indicators
  if (cityScrollTop > 0) {
    renderer.drawCenteredText(SMALL_FONT_ID, metrics.topPadding + metrics.headerHeight, "▲");
  }
  if (cityScrollTop + visibleCount < total) {
    renderer.drawCenteredText(SMALL_FONT_ID, y + 2, "▼");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

// Draw a 32x32 icon scaled 2x (64x64) using pixel doubling
static void drawIconScaled2x(GfxRenderer& r, const uint8_t* icon, int x, int y) {
  for (int row = 0; row < 32; row++) {
    for (int col = 0; col < 32; col++) {
      int byteIdx = row * 4 + col / 8;
      int bitIdx = 7 - (col % 8);
      bool isBlack = !((icon[byteIdx] >> bitIdx) & 1);
      if (isBlack) {
        int dx = x + col * 2;
        int dy = y + row * 2;
        r.drawPixel(dx, dy);
        r.drawPixel(dx + 1, dy);
        r.drawPixel(dx, dy + 1);
        r.drawPixel(dx + 1, dy + 1);
      }
    }
  }
}

void WeatherActivity::render(RenderLock&&) {
  if (state == SELECTING_CITY) {
    renderCityList();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();

  if (state == FETCHING || state == WIFI_CONNECTING || state == FETCH_ERROR) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_WEATHER));
    int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
    renderer.drawCenteredText(UI_12_FONT_ID, y, statusMessage.c_str());
  } else {
    char buf[64];
    int y = metrics.topPadding + 16;

    // City name — centered bold
    renderer.drawCenteredText(UI_12_FONT_ID, y, getCurrentCityName(), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 2;

    // Weather description
    const char* desc = weatherCodeToString(weather.weatherCode);
    renderer.drawCenteredText(UI_10_FONT_ID, y, desc);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 12;

    // Large weather icon (64x64 via 2x scaling) — centered
    constexpr int ICON_DISPLAY_SIZE = 64;
    const uint8_t* icon = getWeatherIcon(weather.weatherCode);
    drawIconScaled2x(renderer, icon, (pageWidth - ICON_DISPLAY_SIZE) / 2, y);
    y += ICON_DISPLAY_SIZE + 8;

    // Large temperature
    snprintf(buf, sizeof(buf), "%.0f°", weather.temperature);
    renderer.drawCenteredText(NOTOSANS_18_FONT_ID, y, buf, true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(NOTOSANS_18_FONT_ID) + 4;

    // Feels like
    snprintf(buf, sizeof(buf), "%s %.0f°", tr(STR_FEELS_LIKE), weather.feelsLike);
    renderer.drawCenteredText(UI_10_FONT_ID, y, buf);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 16;

    // Details card — humidity & wind in rounded rect
    const int cardX = 20;
    const int cardW = pageWidth - 40;
    const int cardH = 80;
    renderer.drawRoundedRect(cardX, y, cardW, cardH, 1, 8, true);
    const int colW = cardW / 2;
    const int detailY = y + 12;

    // Humidity
    snprintf(buf, sizeof(buf), "%s", tr(STR_HUMIDITY));
    int tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
    renderer.drawText(SMALL_FONT_ID, cardX + (colW - tw) / 2, detailY, buf);
    snprintf(buf, sizeof(buf), "%d%%", weather.humidity);
    tw = renderer.getTextWidth(UI_12_FONT_ID, buf);
    renderer.drawText(UI_12_FONT_ID, cardX + (colW - tw) / 2, detailY + 26, buf, true, EpdFontFamily::BOLD);

    // Wind
    snprintf(buf, sizeof(buf), "%s", tr(STR_WIND));
    tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
    renderer.drawText(SMALL_FONT_ID, cardX + colW + (colW - tw) / 2, detailY, buf);
    snprintf(buf, sizeof(buf), "%.1f km/h", weather.windSpeed);
    tw = renderer.getTextWidth(UI_12_FONT_ID, buf);
    renderer.drawText(UI_12_FONT_ID, cardX + colW + (colW - tw) / 2, detailY + 26, buf, true, EpdFontFamily::BOLD);

    renderer.drawLine(cardX + colW, y + 8, cardX + colW, y + cardH - 8, true);
    y += cardH + 12;

    // 5-day forecast row
    if (forecastCount > 0) {
      renderer.drawLine(cardX, y, cardX + cardW, y, true);
      y += 8;
      const int cols = forecastCount;
      const int cellW = cardW / cols;
      for (int i = 0; i < cols; i++) {
        const auto& f = forecast[i];
        int cx = cardX + i * cellW + cellW / 2;

        // Day label centered
        tw = renderer.getTextWidth(SMALL_FONT_ID, f.dayLabel);
        renderer.drawText(SMALL_FONT_ID, cx - tw / 2, y, f.dayLabel);

        // Small icon centered (32x32)
        const uint8_t* fIcon = getWeatherIcon(f.weatherCode);
        renderer.drawIcon(fIcon, cx - WEATHER_ICON_SIZE / 2, y + 20, WEATHER_ICON_SIZE, WEATHER_ICON_SIZE);

        // High/Low temp
        snprintf(buf, sizeof(buf), "%.0f°", f.tempMax);
        tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
        renderer.drawText(SMALL_FONT_ID, cx - tw / 2, y + 56, buf, true, EpdFontFamily::BOLD);

        snprintf(buf, sizeof(buf), "%.0f°", f.tempMin);
        tw = renderer.getTextWidth(SMALL_FONT_ID, buf);
        renderer.drawText(SMALL_FONT_ID, cx - tw / 2, y + 74, buf);
      }
      y += 96;
    }

    // Last updated
    if (lastUpdateTime[0]) {
      snprintf(buf, sizeof(buf), "%s: %s", tr(STR_LAST_UPDATED), lastUpdateTime);
      renderer.drawCenteredText(SMALL_FONT_ID, y, buf);
    }
  }

  // Button hints: Back | Refresh | Location (Left+Right both open city picker)
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_LOCATION), tr(STR_LOCATION));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
