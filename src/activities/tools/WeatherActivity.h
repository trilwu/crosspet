#pragma once
#include "../Activity.h"

#include <string>

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

class WeatherActivity final : public Activity {
 public:
  enum State { WIFI_CONNECTING, FETCHING, DISPLAYING, FETCH_ERROR };

  explicit WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Weather", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

  // Static cache access for sleep screen (reads from SD)
  static constexpr int CITY_COUNT = 3;
  static const CityCoord CITIES[CITY_COUNT];
  static bool loadWeatherCache(WeatherData& out, uint8_t& cityIdx, char* timeBuf, size_t timeBufLen);
  static const char* weatherCodeToString(int code);

  // Silent background refresh: WiFi connect → fetch → cache → WiFi off.
  // Blocks for up to ~15s. Returns true if cache was updated.
  static bool silentRefresh();

 private:
  State state = WIFI_CONNECTING;
  WeatherData weather;
  uint8_t selectedCity = 0;
  std::string statusMessage;
  char lastUpdateTime[8] = "";

  void onWifiConnected();
  void fetchWeather();
  bool parseWeather(const std::string& json);
  void saveWeatherCache();
};
