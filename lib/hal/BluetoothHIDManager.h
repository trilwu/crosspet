#pragma once
#ifdef ENABLE_BLE

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include "DeviceProfiles.h"

// Forward declarations
class NimBLEClient;
class NimBLERemoteCharacteristic;
class NimBLEAdvertisedDevice;

struct BluetoothDevice {
  std::string address;
  std::string name;
  int rssi;
  bool isHID = false;
  uint8_t addressType = 0;  // BLE address type (0=public, 1=random, etc.)
};

struct ConnectedDevice {
  std::string address;
  std::string name;
  NimBLEClient* client = nullptr;
  std::vector<NimBLERemoteCharacteristic*> reportChars;
  unsigned long connectedTime = 0;    // Timestamp when BLE link was established
  bool subscribed = false;
  unsigned long lastActivityTime = 0;  // Timestamp of last HID report received
  uint8_t lastHIDKeycode = 0x00;       // Track last keycode to detect press/release transitions
  unsigned long lastInjectionTime = 0; // Cooldown for button injection to prevent flooding
  uint8_t lastInjectedKeycode = 0x00;  // Track last injected key for smarter cooldown
  bool wasConnected = false;           // Track if this device was previously connected for auto-reconnect
  bool hasSeenRelease = false;         // Ignore startup noise until a release frame is seen
  bool lastButtonState = false;        // Track button pressed state (from byte[0])
  const DeviceProfiles::DeviceProfile* profile = nullptr;  // Device-specific HID profile

  // Normalized event tracking
  unsigned long lastNormalizedEventMs = 0;
  uint8_t lastNormalizedKeycode = 0x00;
  bool lastNormalizedPressed = false;
  uint8_t lastNormalizedDirection = 0xFF;  // 0x00=back, 0x01=forward, 0xFF=unknown

  // Game Brick V2 state
  uint16_t lastGameBrickCounter = 0xFFFF;
  uint8_t lastGameBrickActiveKey = 0x00;
  uint8_t gameBrickCenterPressFrames = 0;
  bool pendingGameBrickRelease = false;
  unsigned long pendingGameBrickReleaseMs = 0;
  uint8_t pendingGameBrickKeycode = 0x00;
  uint8_t pendingGameBrickButton = 0xFF;

  // Report descriptor hints (populated during connect)
  bool descriptorHasConsumerPage = false;
  bool descriptorHasKeyboardPage = false;
  uint8_t descriptorSuggestedIndex = 0xFF;

  // Simple fallback for unknown devices
  bool simpleFallbackEnabled = false;
  uint8_t simpleForwardKeycode = 0x00;
  uint8_t simpleBackKeycode = 0x00;

  // Active injected virtual button (0xFF = none)
  uint8_t activeInjectedButton = 0xFF;

  // By-value storage for per-device custom profile (avoids dangling pointer from static slot)
  DeviceProfiles::DeviceProfile perDevProfileStorage = {};
  bool hasPerDevProfile = false;
};

class BluetoothHIDManager {
public:
  // Singleton access
  static BluetoothHIDManager& getInstance();

  // Lifecycle
  bool enable();
  bool disable();
  bool isEnabled() const { return _enabled; }

  // Scanning
  void startScan(uint32_t durationMs = 10000);
  void stopScan();
  bool isScanning() const { return _scanning; }
  const std::vector<BluetoothDevice>& getDiscoveredDevices() const { return _discoveredDevices; }

  // Connection
  bool connectToDevice(std::string address);  // By value — cleared internally
  bool disconnectFromDevice(const std::string& address);
  bool isConnected(const std::string& address) const;
  std::vector<std::string> getConnectedDevices() const;

  // Input handling
  void processInputEvents();
  void setInputCallback(std::function<void(uint16_t keycode)> callback);
  void setButtonInjector(std::function<void(uint8_t buttonIndex)> injector);
  void setButtonActivityNotifier(std::function<void(uint8_t)> notifier);
  void setReaderContextCallback(std::function<bool()> callback);
  bool hadRecentFree2Input(unsigned long windowMs = 1500) const;
  void setLearnInputCallback(std::function<void(uint8_t keycode, uint8_t reportIndex)> callback);
  void setDebugCaptureEnabled(bool enabled) { _debugCaptureEnabled = enabled; }
  bool isDebugCaptureEnabled() const { return _debugCaptureEnabled; }
  void setBondedDevice(const std::string& address, const std::string& name = "", uint8_t addrType = 0xFF);
  void updateActivity();  // Call periodically to check inactivity timeout
  void checkAutoReconnect(bool userInputDetected = false);  // Reconnect bonded device when disconnected
  
  // Check if BLE has had activity recently (within last 4 minutes)
  // Used by power manager to prevent sleep during BLE use
  bool hasRecentActivity() const;

  // State persistence
  void saveState();
  void loadState();

  std::string lastError;

  // BLE callbacks (public for NimBLE callbacks)
  void onScanResult(NimBLEAdvertisedDevice* advertisedDevice);
  static void onHIDNotify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);

private:
  BluetoothHIDManager();
  ~BluetoothHIDManager();
  BluetoothHIDManager(const BluetoothHIDManager&) = delete;
  BluetoothHIDManager& operator=(const BluetoothHIDManager&) = delete;

  void cleanup();
  uint16_t parseHIDReport(uint8_t* data, size_t length);
  ConnectedDevice* findConnectedDevice(const std::string& address);
  uint8_t mapKeycodeToButton(uint8_t keycode, const ConnectedDevice* device);

  // Input dispatch helpers (defined in BluetoothHIDInput.cpp)
  static void injectWithCooldown(ConnectedDevice* device, uint8_t keycode, uint8_t btn);
  static void handleGameBrickV1(ConnectedDevice* device, uint8_t* pData, size_t length);
  static void handleGameBrickV2(ConnectedDevice* device, uint8_t* pData, size_t length);
  static void handleFree2(ConnectedDevice* device, uint8_t* pData, size_t length);
  static void handleGeneric(ConnectedDevice* device, uint8_t* pData, size_t length);

  bool _enabled = false;
  bool _scanning = false;
  bool _nimbleOwnedByUs = false;  // true if we called NimBLEDevice::init()
  std::vector<BluetoothDevice> _discoveredDevices;
  std::vector<ConnectedDevice> _connectedDevices;
  std::function<void(uint16_t)> _inputCallback;
  std::function<void(uint8_t)> _buttonInjector;
  std::function<void(uint8_t)> _buttonActivityNotifier;
  std::function<bool()> _readerContextCallback;
  std::function<void(uint8_t, uint8_t)> _learnInputCallback;
  bool _debugCaptureEnabled = false;
  std::string _bondedDeviceAddress;
  std::string _bondedDeviceName;
  uint8_t _bondedDeviceAddrType = 0xFF;  // BLE address type for reconnect
  
  // Inactivity timeout (milliseconds)
  static constexpr unsigned long INACTIVITY_TIMEOUT_MS = 300000;  // 5 minutes
  unsigned long lastMaintenanceCheck = 0;

  // Spinlock protecting _connectedDevices — NimBLE callbacks run on host task
  portMUX_TYPE _devicesMux = portMUX_INITIALIZER_UNLOCKED;
};
#endif // ENABLE_BLE
