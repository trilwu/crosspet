#pragma once

#include <esp_wifi.h>

#include <algorithm>
#include <vector>

#include "../Activity.h"
#include "components/CtosRenderer.h"

class WifiScannerActivity final : public Activity {
  CtosRenderer ctos;
  std::vector<wifi_ap_record_t> apRecords;
  int scrollOffset = 0;
  bool scanning = false;
  unsigned long scanTimeMs = 0;

  static constexpr int MAX_APS = 30;
  static constexpr int VISIBLE_ROWS = 12;

  void startScan();
  const char* authModeStr(wifi_auth_mode_t mode) const;

 public:
  explicit WifiScannerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("WiFiScanner", renderer, mappedInput), ctos(renderer) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
