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
    LEARN_KEYS
  };

  enum class LearnStep {
    WAIT_PREV,
    WAIT_NEXT,
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
  std::string getSignalStrengthIndicator(const int32_t rssi) const;
};
#endif // ENABLE_BLE
