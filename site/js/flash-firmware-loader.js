/* CrossPet firmware flasher using esptool-js directly.
   Writes firmware to app partition (0x10000) with compress + no full erase.
   Same approach as xteink.dve.al flasher. */

import { ESPLoader, Transport } from 'https://unpkg.com/esptool-js@0.5.7/bundle.js';

var FIRMWARE_URL = 'firmware/firmware.bin';
var MANIFEST_URL = 'firmware/manifest.json';
var APP_OFFSET = 0x10000;
// ESP32-C3 built-in USB JTAG/serial
var USB_VENDOR_ID = 12346;
var USB_PRODUCT_ID = 4097;

var flashBtn = document.getElementById('flash-btn');
var progressEl = document.getElementById('flash-progress');
var progressFill = document.getElementById('flash-progress-fill');
var statusEl = document.getElementById('flash-status');
var versionEl = document.getElementById('firmware-version');
var downloadEl = document.getElementById('firmware-download');
var errorEl = document.getElementById('flash-error');
var errorMsg = document.getElementById('flash-error-msg');
var unsupportedEl = document.getElementById('flash-unsupported');

// Check Web Serial support
if (!('serial' in navigator && navigator.serial)) {
  if (flashBtn) flashBtn.style.display = 'none';
  if (unsupportedEl) unsupportedEl.style.display = '';
}

// Load manifest to show version and download link
fetch(MANIFEST_URL)
  .then(function (res) { return res.ok ? res.json() : null; })
  .then(function (m) {
    if (!m) return;
    if (versionEl) {
      versionEl.textContent = 'v' + m.version;
      versionEl.closest('.firmware-version-info').style.display = '';
    }
    if (downloadEl) downloadEl.style.display = '';
  })
  .catch(function () {});

// Flash button handler
if (flashBtn) {
  flashBtn.addEventListener('click', startFlash);
}

async function startFlash() {
  flashBtn.disabled = true;
  flashBtn.textContent = 'Flashing...';
  showProgress('Requesting serial port...');
  clearError();

  var transport = null;

  try {
    // Request serial port with ESP32-C3 USB filter
    var port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: USB_VENDOR_ID, usbProductId: USB_PRODUCT_ID }]
    });

    // Connect via esptool
    showProgress('Connecting to ESP32-C3...');
    transport = new Transport(port, false);
    var loader = new ESPLoader({
      transport: transport,
      baudrate: 115200,
      romBaudrate: 115200,
      enableTracing: false
    });

    await loader.main();
    showProgress('Connected. Downloading firmware...');

    // Download firmware binary
    var res = await fetch(FIRMWARE_URL);
    if (!res.ok) throw new Error('Failed to download firmware: ' + res.status);
    var firmwareData = await res.arrayBuffer();
    var firmwareStr = loader.ui8ToBstr(new Uint8Array(firmwareData));

    showProgress('Writing firmware (' + (firmwareData.byteLength / 1024 / 1024).toFixed(1) + ' MB)...');

    // Write firmware to app partition — no full erase, with compression
    await loader.writeFlash({
      fileArray: [{ data: firmwareStr, address: APP_OFFSET }],
      flashSize: 'keep',
      flashMode: 'keep',
      flashFreq: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: function (_fileIndex, written, total) {
        var pct = Math.round((written / total) * 100);
        setProgress(pct);
        statusEl.textContent = 'Writing: ' + pct + '% (' +
          Math.round(written / 1024) + ' / ' + Math.round(total / 1024) + ' KB)';
      }
    });

    // ESP32-C3 USB JTAG doesn't support DTR/RTS hard_reset properly —
    // it locks up the device. Use no_reset and tell user to power cycle.
    await loader.after('no_reset');
    await transport.disconnect();
    transport = null;

    // Success
    setProgress(100);
    statusEl.innerHTML = 'Flash complete! <strong>Press the power button</strong> to reboot your device.';
    statusEl.className = 'flash-status success';
    flashBtn.textContent = 'Flash CrossPet Firmware';
    flashBtn.disabled = false;

  } catch (err) {
    console.error('[flash]', err);
    showError(err.message || 'Unknown error');
    hideProgress();
    flashBtn.textContent = 'Flash CrossPet Firmware';
    flashBtn.disabled = false;

    // Try to clean up transport
    if (transport) {
      try { await transport.disconnect(); } catch (_) {}
    }
  }
}

function showProgress(msg) {
  progressEl.classList.add('active');
  statusEl.textContent = msg;
  statusEl.className = 'flash-status';
}

function setProgress(pct) {
  progressFill.style.width = pct + '%';
}

function hideProgress() {
  progressEl.classList.remove('active');
  progressFill.style.width = '0%';
}

function showError(msg) {
  if (errorEl && errorMsg) {
    errorMsg.textContent = msg;
    errorEl.style.display = '';
  }
}

function clearError() {
  if (errorEl) errorEl.style.display = 'none';
}
