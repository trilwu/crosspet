/* Flash firmware loader for CrossPet Reader.
   Uses static manifest at /firmware/manifest.json (same-origin, no CORS).
   Firmware binary is deployed alongside the manifest by the release workflow.
   Falls back to GitHub Releases download link if manifest fetch fails. */

(function () {
  'use strict';

  var REPO = 'trilwu/crosspet';
  var MANIFEST_PATH = 'firmware/manifest.json';
  var FALLBACK_VERSION = 'v1.6.10';

  var installBtn = document.querySelector('esp-web-install-button');
  var versionEl = document.getElementById('firmware-version');
  var downloadEl = document.getElementById('firmware-download');
  var downloadLink = document.getElementById('firmware-download-link');
  var errorEl = document.getElementById('flash-error');
  var errorMsg = document.getElementById('flash-error-msg');

  if (!installBtn) return;

  // Point esp-web-tools to static manifest (same origin — no CORS)
  installBtn.setAttribute('manifest', MANIFEST_PATH);

  // Fetch manifest to display version and set up download link
  fetch(MANIFEST_PATH)
    .then(function (res) {
      if (!res.ok) throw new Error('Manifest not found');
      return res.json();
    })
    .then(function (manifest) {
      var version = manifest.version || '0.0.0';

      // Show version
      if (versionEl) {
        versionEl.textContent = 'v' + version;
        versionEl.closest('.firmware-version-info').style.display = '';
      }

      // Show direct download link (same-origin firmware binary)
      if (downloadLink && downloadEl) {
        downloadLink.href = 'firmware/firmware.bin';
        downloadLink.setAttribute('download', 'firmware-x4-crosspet-v' + version + '.bin');
        downloadEl.style.display = '';
      }
    })
    .catch(function (err) {
      console.warn('[flash] Manifest fetch failed, using GitHub fallback:', err.message);

      // Fallback: link to GitHub Releases for manual download
      var ghUrl = 'https://github.com/' + REPO + '/releases/download/' +
        FALLBACK_VERSION + '/firmware-x4-crosspet-' + FALLBACK_VERSION + '.bin';

      if (downloadLink && downloadEl) {
        downloadLink.href = ghUrl;
        downloadLink.setAttribute('download', 'firmware-x4-crosspet-' + FALLBACK_VERSION + '.bin');
        downloadEl.style.display = '';
      }

      if (versionEl) {
        versionEl.textContent = FALLBACK_VERSION;
        versionEl.closest('.firmware-version-info').style.display = '';
      }

      if (errorEl && errorMsg) {
        errorMsg.textContent = 'Firmware not yet deployed to this site. Use the download link or community flasher.';
        errorEl.style.display = '';
      }
    });
})();
