#pragma once

#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// UI states for BLE remote pairing flow
enum class BleUiState {
  IDLE,           // Show paired device info or prompt to scan
  SCANNING,       // Actively scanning for BLE HID devices
  SCAN_COMPLETE,  // Show discovered device list
  CONNECTING,     // Connecting to selected device
  CONNECTED,      // Success — auto-finish after brief display
  ERROR           // Connection failed
};

class BleRemotePairingActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  BleUiState state = BleUiState::IDLE;
  int selectedDeviceIndex = 0;
  std::string errorMessage;
  unsigned long stateEnteredAt = 0;

  // Idle state menu: 0 = scan/pair, 1 = forget (only shown when already paired)
  int idleMenuSelection = 0;

  void startScanning();
  void connectToSelected();
  void forgetDevice();

  // Render helpers for each state
  void renderIdle() const;
  void renderScanning() const;
  void renderScanComplete() const;
  void renderConnecting() const;
  void renderConnected() const;
  void renderError() const;

  std::string getSignalStrengthIndicator(int rssi) const;

 public:
  explicit BleRemotePairingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BLE Pairing", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
