/* Fetch latest CrossPet firmware from GitHub Releases
   and set up esp-web-tools manifest for OTA-only flash. */

(function () {
  'use strict';

  const REPO = 'trilwu/crosspet';
  const API_URL = 'https://api.github.com/repos/' + REPO + '/releases/latest';
  const FIRMWARE_ASSET_PREFIX = 'firmware-x4-crosspet-';

  // OTA-only: flash firmware at app partition offset, skip bootloader/partitions
  var OTA_OFFSET = 65536; // 0x10000

  var installBtn = document.querySelector('esp-web-install-button');
  var versionEl = document.getElementById('firmware-version');

  if (!installBtn) return;

  fetch(API_URL)
    .then(function (res) {
      if (!res.ok) throw new Error('GitHub API ' + res.status);
      return res.json();
    })
    .then(function (release) {
      var version = release.tag_name || '0.0.0';
      var asset = null;

      // Find firmware-x4-crosspet-*.bin in release assets
      for (var i = 0; i < release.assets.length; i++) {
        var name = release.assets[i].name;
        if (name.startsWith(FIRMWARE_ASSET_PREFIX) && name.endsWith('.bin')) {
          asset = release.assets[i];
          break;
        }
      }

      if (!asset) {
        showError('firmware.bin not found in release ' + version);
        return;
      }

      // browser_download_url goes through GitHub redirect → objects.githubusercontent.com (CORS OK)
      var downloadUrl = asset.browser_download_url;

      // Build OTA-only manifest
      var manifest = {
        name: 'CrossPet Reader',
        version: version.replace(/^v/, ''),
        new_install_prompt_erase: false,
        builds: [
          {
            chipFamily: 'ESP32-C3',
            parts: [{ path: downloadUrl, offset: OTA_OFFSET }]
          }
        ]
      };

      // Create blob URL for manifest and assign to esp-web-install-button
      var blob = new Blob([JSON.stringify(manifest)], { type: 'application/json' });
      installBtn.setAttribute('manifest', URL.createObjectURL(blob));

      // Show version
      if (versionEl) {
        versionEl.textContent = version;
        versionEl.closest('.firmware-version-info').style.display = '';
      }
    })
    .catch(function (err) {
      console.warn('[flash] Failed to fetch release:', err);
      showError(err.message);
    });

  function showError(msg) {
    if (versionEl) {
      versionEl.textContent = 'error';
      versionEl.closest('.firmware-version-info').style.display = '';
    }
    console.error('[flash]', msg);
  }
})();
