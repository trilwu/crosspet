#pragma once

#include <NimBLEDevice.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class HalGPIO;

// Info about a discovered BLE HID device during scanning
struct BleDiscoveredDevice {
  std::string name;
  NimBLEAddress address;
  int rssi;
};

// Manages BLE Central connection to HID page-turn remotes
class BleRemoteManager : public NimBLEScanCallbacks, public NimBLEClientCallbacks {
 public:
  explicit BleRemoteManager(HalGPIO& gpio);

  // Lifecycle
  bool init();
  void deinit();
  bool isEnabled() const { return enabled; }
  bool isConnected() const { return connected; }
  bool isSuspended() const { return suspended; }

  // Scanning
  void startScan(uint32_t durationSec = 10);
  void stopScan();
  bool isScanning() const { return scanning; }

  // Connection
  bool connectToDevice(const NimBLEAddress& addr);
  void disconnect();
  const std::string& getLastError() const { return lastError; }

  // Discovered devices (thread-safe access)
  std::vector<BleDiscoveredDevice> getDiscoveredDevices();
  void clearDiscoveredDevices();

  // Auto-reconnect to bonded device (non-blocking)
  void autoReconnect(const char* bondedAddr, uint8_t addrType = 0);

  // Pairing lock — prevents main loop from calling deinit() during pairing
  void setPairingActive(bool active) { pairingActive = active; }
  bool isPairingActive() const { return pairingActive; }

  // WiFi mutual exclusion
  void suspend();
  void resume();

  // Periodic tick for background reconnect (call from main loop)
  void tick();

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

  std::vector<BleDiscoveredDevice> discoveredDevices;
  std::mutex devicesMutex;

  // NimBLEScanCallbacks
  void onResult(const NimBLEAdvertisedDevice* device) override;
  void onScanEnd(const NimBLEScanResults& results, int reason) override;

  // NimBLEClientCallbacks
  void onConnect(NimBLEClient* client) override;
  void onDisconnect(NimBLEClient* client, int reason) override;

  // HID report handling
  void onHidReport(const uint8_t* data, size_t len);
  static uint8_t mapKeycodeToButton(uint8_t keycode);
  static uint8_t mapConsumerUsageToButton(uint16_t usageId);
};
