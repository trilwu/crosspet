#ifdef ENABLE_BLE
#include "BleScannerActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <NimBLEDevice.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include "fontIds.h"

// ---------------------------------------------------------------------------
// Tracker fingerprint table
// ---------------------------------------------------------------------------

struct TrackerSignature {
  const char* name;
  uint16_t companyId;   // BLE manufacturer data company ID (0 = don't check)
  uint8_t typeId;       // type byte at offset 2 in manufacturer data (0 = any)
  uint16_t serviceUuid; // GATT service UUID (0 = don't check)
};

static const TrackerSignature KNOWN_TRACKERS[] = {
    {"AirTag",   0x004C, 0x12, 0x0000}, // Apple Find My network
    {"Tile",     0x0000, 0x00, 0xFEED}, // Tile service UUID
    {"SmartTag", 0x0075, 0x00, 0xFD5A}, // Samsung SmartTag
    {"Flipper",  0x0000, 0x00, 0x0000}, // Detected by name prefix
};

static constexpr int TRACKER_COUNT = static_cast<int>(sizeof(KNOWN_TRACKERS) / sizeof(KNOWN_TRACKERS[0]));

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Returns a lowercase copy of s for case-insensitive matching.
static std::string toLower(const std::string& s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

// Checks whether haystack contains needle (case-insensitive).
static bool nameContains(const std::string& name, const char* needle) {
  return toLower(name).find(needle) != std::string::npos;
}

// Extract 16-bit company ID from manufacturer data (little-endian, first 2 bytes).
static bool extractCompanyId(const std::string& mfr, uint16_t& outId) {
  if (mfr.size() < 2) return false;
  outId = static_cast<uint16_t>(static_cast<uint8_t>(mfr[0])) |
          (static_cast<uint16_t>(static_cast<uint8_t>(mfr[1])) << 8);
  return true;
}

// ---------------------------------------------------------------------------
// Activity lifecycle
// ---------------------------------------------------------------------------

void BleScannerActivity::onEnter() {
  Activity::onEnter();
  startScan();
  requestUpdate();
}

void BleScannerActivity::onExit() {
  // Free ~70KB BLE stack RAM immediately on exit.
  NimBLEDevice::deinit(true);
  Activity::onExit();
}

// ---------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------

void BleScannerActivity::startScan() {
  devices.clear();
  scrollOffset = 0;
  scanning = true;
  requestUpdate();

  LOG_INF("BLE", "[SCAN] Starting passive scan for %ds, heap: %d",
          SCAN_DURATION_SEC, static_cast<int>(ESP.getFreeHeap()));

  NimBLEDevice::init("ghost");
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(false); // passive — lower power
  scan->setInterval(100);
  scan->setWindow(99);

  NimBLEScanResults results = scan->start(SCAN_DURATION_SEC, false);

  // Deduplicate by address and cap at MAX_BLE_DEVICES.
  for (int i = 0; i < results.getCount() && static_cast<int>(devices.size()) < MAX_BLE_DEVICES; ++i) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    const std::string addr = dev->getAddress().toString();

    // Skip duplicate addresses.
    bool dup = false;
    for (const auto& d : devices) {
      if (d.addr == addr) { dup = true; break; }
    }
    if (dup) continue;

    BleDeviceInfo info;
    info.name = dev->getName();
    info.addr = addr;
    info.rssi = dev->getRSSI();
    info.category = "???";
    info.trackerName = nullptr;

    classifyDevice(info, dev);
    devices.push_back(std::move(info));
  }

  // Sort: trackers first, then by RSSI descending (stronger signal = closer).
  std::sort(devices.begin(), devices.end(), [](const BleDeviceInfo& a, const BleDeviceInfo& b) {
    const bool aTracker = (a.trackerName != nullptr);
    const bool bTracker = (b.trackerName != nullptr);
    if (aTracker != bTracker) return aTracker > bTracker;
    return a.rssi > b.rssi;
  });

  scanning = false;

  LOG_INF("BLE", "[SCAN] Done — found %d devices", static_cast<int>(devices.size()));
}

// ---------------------------------------------------------------------------
// Classification
// ---------------------------------------------------------------------------

void BleScannerActivity::classifyDevice(BleDeviceInfo& info, const NimBLEAdvertisedDevice* dev) {
  const std::string mfr = dev->getManufacturerData();
  uint16_t companyId = 0;
  const bool hasMfr = extractCompanyId(mfr, companyId);
  const uint8_t typeId = (mfr.size() >= 3) ? static_cast<uint8_t>(mfr[2]) : 0;

  // --- Tracker matching ---
  for (int i = 0; i < TRACKER_COUNT; ++i) {
    const TrackerSignature& sig = KNOWN_TRACKERS[i];

    // Flipper: name-based detection.
    if (strcmp(sig.name, "Flipper") == 0) {
      if (nameContains(info.name, "flipper")) {
        info.category = "TRACKER";
        info.trackerName = sig.name;
        return;
      }
      continue;
    }

    // Company ID match (if required by signature).
    if (sig.companyId != 0) {
      if (!hasMfr || companyId != sig.companyId) continue;
      // Type byte match (if required).
      if (sig.typeId != 0 && typeId != sig.typeId) continue;
    }

    // Service UUID match (if required by signature).
    if (sig.serviceUuid != 0) {
      const NimBLEUUID svcUuid(sig.serviceUuid);
      if (!dev->haveServiceUUID() || !dev->isAdvertisingService(svcUuid)) continue;
    }

    // Passed all checks — it's a known tracker.
    info.category = "TRACKER";
    info.trackerName = sig.name;
    return;
  }

  // --- Category matching by name keywords ---
  const std::string nameLower = toLower(info.name);

  if (nameLower.find("pods") != std::string::npos ||
      nameLower.find("buds") != std::string::npos ||
      nameLower.find("headphone") != std::string::npos ||
      nameLower.find("speaker") != std::string::npos) {
    info.category = "AUDIO";
    return;
  }

  if (nameLower.find("band") != std::string::npos ||
      nameLower.find("watch") != std::string::npos ||
      nameLower.find("fit") != std::string::npos) {
    info.category = "WEAR";
    return;
  }

  if (nameLower.find("iphone") != std::string::npos ||
      nameLower.find("galaxy") != std::string::npos ||
      nameLower.find("pixel") != std::string::npos) {
    info.category = "PHONE";
    return;
  }

  // Default: unknown.
  info.category = "???";
}

// ---------------------------------------------------------------------------
// Filtered count
// ---------------------------------------------------------------------------

int BleScannerActivity::filteredCount() const {
  if (!trackersOnly) return static_cast<int>(devices.size());
  int n = 0;
  for (const auto& d : devices) {
    if (d.trackerName != nullptr) ++n;
  }
  return n;
}

// ---------------------------------------------------------------------------
// Input loop
// ---------------------------------------------------------------------------

void BleScannerActivity::loop() {
  const int total = filteredCount();
  const int maxScroll = std::max(0, total - VISIBLE_ROWS);

  // Scroll down.
  buttonNavigator.onNext([this, maxScroll] {
    if (scrollOffset < maxScroll) {
      ++scrollOffset;
      requestUpdate();
    }
  });

  // Scroll up.
  buttonNavigator.onPrevious([this] {
    if (scrollOffset > 0) {
      --scrollOffset;
      requestUpdate();
    }
  });

  // Select / confirm → re-scan.
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    startScan();
    requestUpdate();
    return;
  }

  // Back → exit activity.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // PageForward → toggle tracker-only filter.
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    trackersOnly = !trackersOnly;
    scrollOffset = 0;
    requestUpdate();
  }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void BleScannerActivity::render(RenderLock&& lock) {
  auto& gfx = ctos.getGfx();
  gfx.clearScreen();

  // Title bar.
  const char* rightText = trackersOnly ? "[TRACKER]" : "[5s scan]";
  ctos.drawCtosFrame("BLE SCANNER", rightText);

  const int screenW = gfx.getScreenWidth();
  const int screenH = gfx.getScreenHeight();

  if (scanning) {
    // Show scanning indicator centered on screen.
    ctos.drawMonoText(screenW / 2 - 40, screenH / 2 - 8, "SCANNING...");
    gfx.displayBuffer();
    return;
  }

  // Column layout constants (portrait 480x800).
  static constexpr int ROW_H = 18;
  static constexpr int COL_NAME = 10;
  static constexpr int COL_TYPE = 240;
  static constexpr int COL_RSSI = 340;
  static constexpr int COL_FLAG = 420;
  static constexpr int HEADER_Y = 40;
  static constexpr int ROWS_START_Y = 58;

  // Column headers.
  ctos.drawSeparator(HEADER_Y - 2);
  ctos.drawMonoText(COL_NAME, HEADER_Y, "DEVICE");
  ctos.drawMonoText(COL_TYPE, HEADER_Y, "TYPE");
  ctos.drawMonoText(COL_RSSI, HEADER_Y, "RSSI");
  ctos.drawMonoText(COL_FLAG, HEADER_Y, "SIG");
  ctos.drawSeparator(ROWS_START_Y - 2);

  // Render visible device rows.
  // `visibleIdx` counts only rows that pass the active filter; used for scroll offset.
  int row = 0;
  int visibleIdx = 0;
  for (const auto& dev : devices) {
    // Apply tracker-only filter.
    if (trackersOnly && dev.trackerName == nullptr) continue;
    // Skip rows before scroll offset.
    if (visibleIdx < scrollOffset) { ++visibleIdx; continue; }
    if (row >= VISIBLE_ROWS) break;

    const int y = ROWS_START_Y + row * ROW_H;

    // Build display name: prefix trackers with "!" warning.
    char nameBuf[32];
    if (dev.trackerName != nullptr) {
      snprintf(nameBuf, sizeof(nameBuf), "!%s", dev.trackerName);
    } else if (!dev.name.empty()) {
      snprintf(nameBuf, sizeof(nameBuf), "%.22s", dev.name.c_str());
    } else {
      snprintf(nameBuf, sizeof(nameBuf), "%.22s", dev.addr.c_str());
    }

    // Invert row for tracker entries to make them stand out.
    const bool highlight = (dev.trackerName != nullptr);
    ctos.drawMonoText(COL_NAME, y, nameBuf, highlight);
    ctos.drawMonoText(COL_TYPE, y, dev.category, false);

    // RSSI value.
    char rssiBuf[8];
    snprintf(rssiBuf, sizeof(rssiBuf), "%ddB", dev.rssi);
    ctos.drawMonoText(COL_RSSI, y, rssiBuf, false);

    // Signal bar graphic.
    ctos.drawSignalBar(COL_FLAG, y, dev.rssi);

    ++row;
    ++visibleIdx;
  }

  // Separator before status bar.
  const int statusY = screenH - 24;
  ctos.drawSeparator(statusY - 4);

  // Count trackers in full device list.
  int trackerCount = 0;
  for (const auto& d : devices) {
    if (d.trackerName != nullptr) ++trackerCount;
  }

  char statusBuf[80];
  if (trackersOnly) {
    snprintf(statusBuf, sizeof(statusBuf),
             "FILTER:ON | TRACKERS: %d | SCAN: %ds | [OK]=RESCAN",
             trackerCount, SCAN_DURATION_SEC);
  } else {
    snprintf(statusBuf, sizeof(statusBuf),
             "DEVICES: %d | TRACKERS: %d | SCAN: %ds | [OK]=RESCAN",
             static_cast<int>(devices.size()), trackerCount, SCAN_DURATION_SEC);
  }
  ctos.drawStatusBar(statusY, statusBuf);

  gfx.displayBuffer();
}

#endif  // ENABLE_BLE
