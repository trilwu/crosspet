#ifdef ENABLE_BLE
#pragma once
#include <NimBLEDevice.h>
#include <string>
#include <vector>

#include "../Activity.h"
#include "components/CtosRenderer.h"
#include "util/ButtonNavigator.h"

struct BleDeviceInfo {
  std::string name;
  std::string addr;
  int rssi;
  const char* category;    // "TRACKER", "AUDIO", "WEAR", "PHONE", "???"
  const char* trackerName; // non-null if identified tracker
};

class BleScannerActivity final : public Activity {
  CtosRenderer ctos;
  ButtonNavigator buttonNavigator;
  std::vector<BleDeviceInfo> devices;
  int scrollOffset = 0;
  bool scanning = false;
  bool trackersOnly = false;

  static constexpr int MAX_BLE_DEVICES = 50;
  static constexpr int SCAN_DURATION_SEC = 5;
  static constexpr int VISIBLE_ROWS = 12;

  void startScan();
  void classifyDevice(BleDeviceInfo& info, const NimBLEAdvertisedDevice* dev);
  int filteredCount() const;

 public:
  explicit BleScannerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                              bool filterTrackers = false)
      : Activity("BLEScanner", renderer, mappedInput),
        ctos(renderer),
        trackersOnly(filterTrackers) {}

  // Allow GhostHomeActivity to set tracker-only filter after construction
  void setTrackerMode(bool trackerOnly) { trackersOnly = trackerOnly; }

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
#endif  // ENABLE_BLE
