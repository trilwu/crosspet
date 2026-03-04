#pragma once
#include "../Activity.h"
#include "ble/BlePresenterManager.h"

// BLE presentation clicker activity.
// Advertises as a HID keyboard; connected host receives slide-navigation keys.
// Right=Next, Left=Prev, Confirm=F5 (start), Back=Esc (end slideshow).
// Back exits the activity only while in ADVERTISING state.
class PresenterActivity final : public Activity {
 public:
  enum class State { ADVERTISING, CONNECTED };

  explicit PresenterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Presenter", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  BlePresenterManager presenter;
  State state = State::ADVERTISING;

  void cleanup();
};
