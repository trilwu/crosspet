#pragma once
#include "../Activity.h"

#include <string>

#include "CrossPointSettings.h"

struct CityCoord {
  const char* name;
  const char* lat;
  const char* lon;
};

struct WeatherData {
  float temperature = 0;
  float feelsLike = 0;
  int humidity = 0;
  int weatherCode = 0;
  float windSpeed = 0;
};

struct DailyForecast {
  int weatherCode = 0;
  float tempMax = 0;
  float tempMin = 0;
  char dayLabel[4] = "";  // "Mon", "Tue", etc.
};

class WeatherActivity final : public Activity {
 public:
  enum State { WIFI_CONNECTING, FETCHING, DISPLAYING, FETCH_ERROR, SELECTING_CITY };

  // selectedCity index: 0 = Auto (IP geolocation), 1..CITY_COUNT = manual cities
  static constexpr int CITY_COUNT = 63;
  static const CityCoord CITIES[CITY_COUNT];

  explicit WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Weather", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

  // Static cache access for sleep screen (reads from SD)
  static bool loadWeatherCache(WeatherData& out, uint8_t& cityIdx, char* timeBuf, size_t timeBufLen,
                               char* autoCityBuf = nullptr, size_t autoCityBufLen = 0);
  static const char* weatherCodeToString(int code);

  // Temperature unit helpers — respect SETTINGS.temperatureUnit (0=°C, 1=°F)
  static float convertTemp(float celsius) {
    if (SETTINGS.temperatureUnit == 1) return celsius * 9.0f / 5.0f + 32.0f;
    return celsius;
  }
  static const char* tempUnitSuffix() {
    return SETTINGS.temperatureUnit == 1 ? "°F" : "°C";
  }

  // Silent background refresh: WiFi connect → fetch → cache → WiFi off.
  // Returns: 0=ok, 1=no saved wifi creds, 2=wifi connect timeout, 3=geo fail, 4=api fail, 5=parse fail
  static int silentRefresh();

 private:
  static constexpr int FORECAST_DAYS = 5;
  State state = WIFI_CONNECTING;
  WeatherData weather;
  DailyForecast forecast[FORECAST_DAYS];
  int forecastCount = 0;
  uint8_t selectedCity = 0;  // 0 = Auto, 1..63 = manual
  std::string statusMessage;
  char lastUpdateTime[8] = "";
  char detectedCityName[32] = "";
  char detectedLat[16] = "";
  char detectedLon[16] = "";
  char detectedTz[16] = "ICT-7";  // POSIX TZ string from ip-api offset

  int cityCursor = 0;      // Cursor in city list (0=Auto, 1..63=cities)
  int cityScrollTop = 0;   // First visible item in city list

  void onWifiConnected();
  void fetchWeather();
  bool parseWeather(const std::string& json);
  void saveWeatherCache();
  bool detectLocation();
  const char* getCurrentCityName() const;
  void renderCityList();
};
