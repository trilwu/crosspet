#pragma once

#include <NimBLEDevice.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class HalGPIO;
struct DeviceProfile;

// Info about a discovered BLE HID device during scanning
struct BleDiscoveredDevice {
  std::string name;
  NimBLEAddress address;
  int rssi = 0;
  bool advertisesHid = false;
};

// Manages BLE Central connection to HID keyboards/page-turners.
// Ported from thedrunkpenguin/crosspoint-reader-ble, adapted for NimBLE 2.3.6.
class BluetoothHIDManager : public NimBLEScanCallbacks {
 public:
  explicit BluetoothHIDManager(HalGPIO& gpio);

  // Lifecycle
  bool init();
  void deinit();
  bool isEnabled() const { return enabled; }
  bool isConnected() const { return connected; }
  bool isSuspended() const { return suspended; }

  // Scanning (async, non-blocking via NimBLE 2.3.6)
  void startScan(uint32_t durationSec = 10);
  void stopScan();
  bool isScanning() const { return scanning; }

  // Connection
  bool connectToDevice(const NimBLEAddress& addr);
  void disconnect();
  const std::string& getLastError() const { return lastError; }

  // Discovered devices (thread-safe)
  std::vector<BleDiscoveredDevice> getDiscoveredDevices();
  void clearDiscoveredDevices();

  // Auto-reconnect to bonded device
  void autoReconnect(const char* bondedAddr, uint8_t addrType = 0);

  // Pairing lock — prevents main loop deinit during pairing UI
  void setPairingActive(bool active) { pairingActive = active; }
  bool isPairingActive() const { return pairingActive; }

  // WiFi mutual exclusion
  void suspend();
  void resume();

  // Background tick for auto-reconnect + inactivity check (call from main loop)
  void tick();

  // Check if BLE had recent activity (for power management)
  bool hasRecentActivity() const;

 private:
  HalGPIO& gpio;
  bool enabled = false;
  volatile bool connected = false;
  volatile bool scanning = false;
  bool suspended = false;
  bool pairingActive = false;
  volatile bool reconnectPending = false;
  bool shouldAutoReconnect = false;
  NimBLEAddress reconnectAddress;
  NimBLEClient* client = nullptr;
  std::string lastError;

  // Connected device metadata
  std::string connectedDeviceMac;
  std::string connectedDeviceName;
  const DeviceProfile* activeProfile = nullptr;
  unsigned long lastActivityTime = 0;

  // HID report state for press/release transition detection
  uint8_t lastHidKeycode = 0;
  bool lastButtonState = false;
  unsigned long lastInjectionTime = 0;

  std::vector<BleDiscoveredDevice> discoveredDevices;
  std::mutex devicesMutex;

  // NimBLE 2.3.6 scan callbacks
  void onResult(const NimBLEAdvertisedDevice* device) override;
  void onScanEnd(const NimBLEScanResults& results, int reason) override;

  // HID report handling
  static void onHidNotifyStatic(NimBLERemoteCharacteristic* ch, uint8_t* data, size_t len, bool isNotify);
  void onHidReport(const uint8_t* data, size_t len);
  uint8_t mapKeycodeToButton(uint8_t keycode);
};
