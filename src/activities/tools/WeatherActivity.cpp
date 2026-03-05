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

const CityCoord WeatherActivity::CITIES[CITY_COUNT] = {
    {"Hà Nội", "21.0285", "105.8542"},
    {"TP HCM", "10.8231", "106.6297"},
    {"Đà Nẵng", "16.0544", "108.2022"},
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
  if (selectedCity >= CITY_COUNT) selectedCity = 0;
  lastUpdateTime[0] = '\0';

  bleManager.suspend();
  state = WIFI_CONNECTING;
  statusMessage = tr(STR_FETCHING_WEATHER);
  requestUpdate(true);

  if (WiFi.status() == WL_CONNECTED) {
    onWifiConnected();
  } else if (trySilentWifiConnect()) {
    onWifiConnected();
  } else {
    // Fallback to WiFi selection UI if silent connect fails
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
  // Sync NTP time with UTC+7 (ICT — Indochina Time)
  configTzTime("ICT-7", "pool.ntp.org", "time.google.com");

  state = FETCHING;
  statusMessage = tr(STR_FETCHING_WEATHER);
  requestUpdate(true);
  fetchWeather();
}

void WeatherActivity::fetchWeather() {
  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?"
           "latitude=%s&longitude=%s"
           "&current=temperature_2m,relative_humidity_2m,"
           "apparent_temperature,weather_code,wind_speed_10m",
           CITIES[selectedCity].lat, CITIES[selectedCity].lon);

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

  // Record update time
  time_t now;
  time(&now);
  struct tm ti;
  localtime_r(&now, &ti);
  snprintf(lastUpdateTime, sizeof(lastUpdateTime), "%02d:%02d", ti.tm_hour, ti.tm_min);

  // Cache weather data to SD for sleep screen display
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
  if (cityIdx >= CITY_COUNT) cityIdx = 0;

  const char* t = doc["time"] | "";
  strncpy(timeBuf, t, timeBufLen - 1);
  timeBuf[timeBufLen - 1] = '\0';
  return true;
}

bool WeatherActivity::silentRefresh() {
  // Try connecting to saved WiFi silently
  if (WiFi.status() != WL_CONNECTED && !trySilentWifiConnect()) return false;

  // Sync NTP time with UTC+7
  configTzTime("ICT-7", "pool.ntp.org", "time.google.com");

  // Fetch weather for the configured city
  uint8_t city = SETTINGS.weatherCity;
  if (city >= CITY_COUNT) city = 0;

  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?"
           "latitude=%s&longitude=%s"
           "&current=temperature_2m,relative_humidity_2m,"
           "apparent_temperature,weather_code,wind_speed_10m",
           CITIES[city].lat, CITIES[city].lon);

  std::string response;
  if (!HttpDownloader::fetchUrl(std::string(url), response)) {
    WiFi.disconnect(false);
    delay(100);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  // Parse JSON
  JsonDocument doc;
  if (deserializeJson(doc, response)) { WiFi.disconnect(false); WiFi.mode(WIFI_OFF); return false; }
  JsonObject current = doc["current"];
  if (current.isNull()) { WiFi.disconnect(false); WiFi.mode(WIFI_OFF); return false; }

  // Build cache JSON and save
  JsonDocument out;
  out["temp"]  = current["temperature_2m"] | 0.0f;
  out["feels"] = current["apparent_temperature"] | 0.0f;
  out["hum"]   = current["relative_humidity_2m"] | 0;
  out["code"]  = current["weather_code"] | 0;
  out["wind"]  = current["wind_speed_10m"] | 0.0f;
  out["city"]  = city;

  time_t now; time(&now);
  struct tm ti; localtime_r(&now, &ti);
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
  out["time"] = timeBuf;

  String json;
  serializeJson(out, json);
  Storage.ensureDirectoryExists("/.crosspoint");
  Storage.writeFile("/.crosspoint/weather_cache.json", json);

  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
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
      if (next) selectedCity = (selectedCity + 1) % CITY_COUNT;
      else selectedCity = (selectedCity + CITY_COUNT - 1) % CITY_COUNT;
      state = FETCHING;
      statusMessage = tr(STR_FETCHING_WEATHER);
      requestUpdate(true);
      fetchWeather();
    }
  }
}

void WeatherActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();

  // Header: "Weather - City Name"
  char header[64];
  snprintf(header, sizeof(header), "%s - %s", tr(STR_WEATHER), CITIES[selectedCity].name);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);

  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

  if (state == FETCHING || state == WIFI_CONNECTING) {
    renderer.drawText(UI_12_FONT_ID, 20, y, statusMessage.c_str());
  } else if (state == FETCH_ERROR) {
    renderer.drawText(UI_12_FONT_ID, 20, y, statusMessage.c_str());
  } else {
    char buf[64];

    // Draw weather condition icon (32x32) to the left of the temperature
    const uint8_t* icon = getWeatherIcon(weather.weatherCode);
    renderer.drawIcon(icon, 20, y, WEATHER_ICON_SIZE, WEATHER_ICON_SIZE);

    // Temperature text to the right of the icon
    snprintf(buf, sizeof(buf), "%.1f°C", weather.temperature);
    renderer.drawText(UI_12_FONT_ID, 20 + WEATHER_ICON_SIZE + 10, y + 4, buf);
    // Weather description below temperature, aligned with icon right edge
    renderer.drawText(UI_10_FONT_ID, 20 + WEATHER_ICON_SIZE + 10, y + 32, weatherCodeToString(weather.weatherCode));
    y += WEATHER_ICON_SIZE + 20;

    snprintf(buf, sizeof(buf), "%s: %.1f°C", tr(STR_FEELS_LIKE), weather.feelsLike);
    renderer.drawText(UI_10_FONT_ID, 20, y, buf);
    y += 40;

    snprintf(buf, sizeof(buf), "%s: %d%%", tr(STR_HUMIDITY), weather.humidity);
    renderer.drawText(UI_10_FONT_ID, 20, y, buf);
    y += 40;

    snprintf(buf, sizeof(buf), "%s: %.1f km/h", tr(STR_WIND), weather.windSpeed);
    renderer.drawText(UI_10_FONT_ID, 20, y, buf);
    y += 40;

    // Last updated timestamp
    if (lastUpdateTime[0]) {
      snprintf(buf, sizeof(buf), "%s: %s", tr(STR_LAST_UPDATED), lastUpdateTime);
      renderer.drawText(SMALL_FONT_ID, 20, y, buf);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), tr(STR_CITY), tr(STR_CITY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
