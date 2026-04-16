#pragma once
#ifdef ENABLE_BLE

#include <BluetoothHIDManager.h>
#include <GfxRenderer.h>
#include <string>

#include "activities/Activity.h"
#include "MappedInputManager.h"

class BluetoothSettingsActivity : public Activity {
 private:
  enum class ViewMode {
    MAIN_MENU,
    DEVICE_LIST,
    LEARN_KEYS,
    DEBUG_MONITOR,   // Raw HID byte monitor
  };

  enum class LearnStep {
    WAIT_PREV,
    WAIT_NEXT,
    WAIT_TEST,       // Press both to confirm
    DONE
  };

  ViewMode viewMode = ViewMode::MAIN_MENU;
  int selectedIndex = 0;
  BluetoothHIDManager* btMgr = nullptr;
  std::string lastError = "";
  unsigned long lastScanTime = 0;
  LearnStep learnStep = LearnStep::WAIT_PREV;
  uint8_t pendingLearnKey = 0;
  uint8_t learnedPrevKey = 0;
  uint8_t learnedNextKey = 0;
  uint8_t learnedReportIndex = 0;
  uint8_t debugRawBytes[8] = {};
  size_t debugRawLength = 0;
  unsigned long debugLastEventMs = 0;
  unsigned long learnTestDeadlineMs = 0;
  bool learnTestPrevPressed = false;
  bool learnTestNextPressed = false;

 public:
  explicit BluetoothSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("BluetoothSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void handleMainMenuInput();
  void handleDeviceListInput();
  void renderMainMenu();
  void renderDeviceList();
  void handleLearnInput();
  void renderLearnKeys();
  void handleDebugMonitorInput();
  void renderDebugMonitor();
  std::string getSignalStrengthIndicator(const int32_t rssi) const;
};
#endif // ENABLE_BLE
