# CrossPet 1.8.4

Recovery release: restores the in-app SD Card Firmware Update and fixes the OTA
memory-pressure abort. Built on the 1.8.3 base. Addresses #28.

## New

### SD Card Firmware Update
Settings → System → **SD Card Firmware Update**. Place any firmware `.bin` on the
SD card, select it, and confirm. The image is validated (magic header, segment
table, XOR checksum, SHA-256), written directly to the OTA app partition, and the
boot slot is switched — **no USB, no WiFi, no hotspot required**.

Because it writes the partition raw and updates `otadata` directly (bypassing the
runtime `esp_image_verify` that rejects patched images on X4 silicon), it can
flash **back to CrossPoint / CrossInk / stock**, not just CrossPet. This is the
escape hatch for USB-locked (eFuse-burned) X3/X4 units. Verified on X4 hardware.

### OTA memory-pressure fix
The updater now frees the active reader/UI font caches before the update-check
and firmware-download TLS handshakes, restoring them afterwards. On the ESP32-C3
(~46 KB free after WiFi) a loaded external font — especially a CJK font — left too
little heap for the mbedTLS handshake, so OTA aborted before the ClientHello (the
"connects then immediately disconnects" behavior reported in #28).

## Getting out from an older build

1. **On 1.8.3 or older:** Settings → System → **Check for updates** → update to
   1.8.4, then use **SD Card Firmware Update** to flash wherever you want.
2. **If that OTA won't start** (memory pressure on your *current* firmware): remove
   or disable the external font, delete everything off the SD card except your
   target `update.bin`, press Reset then hold Power, and go straight to Check for
   updates.
3. **If OTA is completely dead:** use the Xteink Unlocker + Escape Hatch to get
   1.8.4 on; from then on the built-in SD path is always available.

## Notes

- New strings added; other languages fall back to English until translated.
- No partition-table or bootloader changes — installs over 1.8.3 via normal OTA.
