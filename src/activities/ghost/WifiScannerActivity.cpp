#include "WifiScannerActivity.h"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "fontIds.h"

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void WifiScannerActivity::onEnter() {
  Activity::onEnter();
  startScan();
  requestUpdate();
}

// ─── Scanning ────────────────────────────────────────────────────────────────

void WifiScannerActivity::startScan() {
  scanning = true;
  requestUpdate();  // show "SCANNING..." state immediately

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  unsigned long t0 = millis();

  wifi_scan_config_t config = {};
  config.show_hidden = true;
  config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  config.scan_time.active.min = 100;
  config.scan_time.active.max = 300;

  esp_wifi_scan_start(&config, true);  // blocking until scan completes

  scanTimeMs = millis() - t0;

  uint16_t count = 0;
  esp_wifi_scan_get_ap_num(&count);
  if (count > MAX_APS) count = MAX_APS;

  apRecords.resize(count);
  esp_wifi_scan_get_ap_records(&count, apRecords.data());

  // Sort strongest signal first
  std::sort(apRecords.begin(), apRecords.end(),
            [](const wifi_ap_record_t& a, const wifi_ap_record_t& b) { return a.rssi > b.rssi; });

  scanning = false;
  scrollOffset = 0;
}

// ─── Input ───────────────────────────────────────────────────────────────────

void WifiScannerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Power)) {
    startScan();
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
      mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    int maxOffset = static_cast<int>(apRecords.size()) - VISIBLE_ROWS;
    if (maxOffset < 0) maxOffset = 0;
    if (scrollOffset < maxOffset) {
      scrollOffset++;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
      mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (scrollOffset > 0) {
      scrollOffset--;
      requestUpdate();
    }
    return;
  }
}

// ─── Auth mode label ─────────────────────────────────────────────────────────

const char* WifiScannerActivity::authModeStr(wifi_auth_mode_t mode) const {
  switch (mode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "ENT";
    default:
      return "UNK";
  }
}

// ─── Rendering ───────────────────────────────────────────────────────────────

void WifiScannerActivity::render(RenderLock&&) {
  auto& gfx = ctos.getGfx();
  gfx.clearScreen();

  ctos.drawCtosFrame("WIFI SCANNER", "[2.4GHz]");

  const int screenW = gfx.getScreenWidth();
  const int screenH = gfx.getScreenHeight();

  // Frame leaves ~40px top bar + 20px bottom bar; content starts at y=48
  constexpr int CONTENT_TOP = 48;
  constexpr int CONTENT_BOTTOM_MARGIN = 28;
  const int contentBottom = screenH - CONTENT_BOTTOM_MARGIN;

  if (scanning) {
    // Centre "SCANNING..." text vertically in content area
    const int midY = CONTENT_TOP + (contentBottom - CONTENT_TOP) / 2;
    const char* msg = "SCANNING...";
    const int msgW = gfx.getTextWidth(UI_12_FONT_ID, msg);
    gfx.drawText(UI_12_FONT_ID, (screenW - msgW) / 2, midY, msg);
    gfx.displayBuffer();
    return;
  }

  // ── Column layout ──────────────────────────────────────────────────────────
  // SSID occupies most of the width; CH, RSSI, SEC, and signal bars on right
  constexpr int COL_CH_W = 28;    // channel
  constexpr int COL_RSSI_W = 40;  // e.g. "-87"
  constexpr int COL_SEC_W = 38;   // auth mode string
  constexpr int COL_BAR_W = 30;   // signal bars
  constexpr int MARGIN = 10;

  const int colBar = screenW - MARGIN - COL_BAR_W;
  const int colSec = colBar - COL_SEC_W;
  const int colRssi = colSec - COL_RSSI_W;
  const int colCh = colRssi - COL_CH_W;
  const int colSsid = MARGIN;

  // ── Column headers ─────────────────────────────────────────────────────────
  const int headerY = CONTENT_TOP;
  gfx.drawText(SMALL_FONT_ID, colSsid, headerY, "SSID");
  gfx.drawText(SMALL_FONT_ID, colCh, headerY, "CH");
  gfx.drawText(SMALL_FONT_ID, colRssi, headerY, "RSSI");
  gfx.drawText(SMALL_FONT_ID, colSec, headerY, "SEC");
  gfx.drawText(SMALL_FONT_ID, colBar, headerY, "SIG");

  const int sepY = headerY + gfx.getLineHeight(SMALL_FONT_ID) + 4;
  ctos.drawSeparator(sepY);

  // ── AP rows ────────────────────────────────────────────────────────────────
  const int rowH = gfx.getLineHeight(UI_12_FONT_ID) + 4;
  int y = sepY + 6;

  const int visibleCount = std::min(VISIBLE_ROWS, static_cast<int>(apRecords.size()) - scrollOffset);
  for (int i = 0; i < visibleCount && y + rowH <= contentBottom; i++) {
    const wifi_ap_record_t& ap = apRecords[static_cast<size_t>(scrollOffset + i)];

    // SSID — show "[HIDDEN]" when empty
    const char* ssid = (ap.ssid[0] == '\0') ? "[HIDDEN]" : reinterpret_cast<const char*>(ap.ssid);

    // Truncate SSID to fit column width (colCh - colSsid - 4)
    char ssidBuf[33];  // max 32 chars + null
    int maxSsidW = colCh - colSsid - 4;
    int ssidW = gfx.getTextWidth(UI_12_FONT_ID, ssid);
    if (ssidW > maxSsidW) {
      // Truncate with ellipsis
      int len = static_cast<int>(strlen(ssid));
      snprintf(ssidBuf, sizeof(ssidBuf), "%s", ssid);
      while (len > 1 && gfx.getTextWidth(UI_12_FONT_ID, ssidBuf) > maxSsidW) {
        len--;
        ssidBuf[len - 1] = '.';
        ssidBuf[len] = '\0';
      }
      ssid = ssidBuf;
    }

    gfx.drawText(UI_12_FONT_ID, colSsid, y, ssid);

    // Channel
    char chBuf[4];
    snprintf(chBuf, sizeof(chBuf), "%d", ap.primary);
    gfx.drawText(UI_12_FONT_ID, colCh, y, chBuf);

    // RSSI
    char rssiBuf[6];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d", ap.rssi);
    gfx.drawText(UI_12_FONT_ID, colRssi, y, rssiBuf);

    // Auth mode
    gfx.drawText(UI_12_FONT_ID, colSec, y, authModeStr(ap.authmode));

    // Signal bar (centred in column)
    ctos.drawSignalBar(colBar, y, ap.rssi);

    y += rowH;
  }

  // Show scroll indicator if list is longer than visible window
  const int totalAps = static_cast<int>(apRecords.size());
  if (totalAps > VISIBLE_ROWS) {
    // Draw a simple scroll position hint on the right edge
    const int trackH = contentBottom - (sepY + 6);
    const int thumbH = std::max(8, trackH * VISIBLE_ROWS / totalAps);
    const int maxOffset = totalAps - VISIBLE_ROWS;
    const int thumbY = (sepY + 6) + (trackH - thumbH) * scrollOffset / maxOffset;
    gfx.fillRect(screenW - 4, thumbY, 3, thumbH, true);
  }

  // ── Status bar ─────────────────────────────────────────────────────────────
  char statusBuf[64];
  unsigned long secs = scanTimeMs / 1000;
  unsigned long tenths = (scanTimeMs % 1000) / 100;
  snprintf(statusBuf, sizeof(statusBuf), "APs: %d | SCAN: %lu.%lus | CH: 1-13", totalAps, secs, tenths);
  ctos.drawStatusBar(contentBottom + 4, statusBuf);

  gfx.displayBuffer();
}
