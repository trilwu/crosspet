/* Fetch latest CrossPet firmware from GitHub Releases
   and set up esp-web-tools manifest for OTA-only flash.
   Also provides a direct download link as fallback. */

(function () {
  'use strict';

  var REPO = 'trilwu/crosspet';
  var API_URL = 'https://api.github.com/repos/' + REPO + '/releases/latest';
  var FIRMWARE_ASSET_PREFIX = 'firmware-x4-crosspet-';

  // OTA-only: flash firmware at app partition offset, skip bootloader/partitions
  var OTA_OFFSET = 65536; // 0x10000

  var installBtn = document.querySelector('esp-web-install-button');
  var versionEl = document.getElementById('firmware-version');
  var downloadEl = document.getElementById('firmware-download');
  var downloadLink = document.getElementById('firmware-download-link');
  var errorEl = document.getElementById('flash-error');
  var errorMsg = document.getElementById('flash-error-msg');

  if (!installBtn) return;

  fetch(API_URL)
    .then(function (res) {
      if (!res.ok) {
        if (res.status === 403) throw new Error('GitHub API rate limit exceeded. Try again later.');
        throw new Error('GitHub API error: ' + res.status);
      }
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
        showError('Firmware .bin not found in release ' + version + '. Check GitHub releases.');
        return;
      }

      // browser_download_url redirects to objects.githubusercontent.com (CORS OK)
      var downloadUrl = asset.browser_download_url;

      // Show direct download link
      if (downloadLink && downloadEl) {
        downloadLink.href = downloadUrl;
        downloadLink.setAttribute('download', asset.name);
        downloadEl.style.display = '';
      }

      // Build OTA-only manifest — esp-web-tools fetches firmware from this URL
      // Use new_install_prompt_erase: false since we only write app partition
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
      // Show fallback: link to GitHub releases page
      if (downloadLink && downloadEl) {
        downloadLink.href = 'https://github.com/' + REPO + '/releases/latest';
        downloadLink.removeAttribute('download');
        downloadLink.textContent = 'Go to GitHub Releases';
        downloadEl.style.display = '';
      }
    });

  function showError(msg) {
    if (errorEl && errorMsg) {
      errorMsg.textContent = msg;
      errorEl.style.display = '';
    }
    if (versionEl) {
      versionEl.textContent = 'error';
      versionEl.closest('.firmware-version-info').style.display = '';
    }
    console.error('[flash]', msg);
  }
})();
