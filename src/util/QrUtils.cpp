#include "QrUtils.h"

#include <qrcodegen.h>

#include <algorithm>
#include <memory>

#include "Logging.h"

void QrUtils::drawQrCode(const GfxRenderer& renderer, const Rect& bounds, const std::string& textPayload) {
  // Nayuki library needs two buffers of qrcodegen_BUFFER_LEN_FOR_VERSION(maxVersion) bytes.
  // We use version 40 (max) to guarantee the payload always fits.
  // Version 40 ECC_LOW holds up to 2953 bytes in byte mode (covers all realistic URLs/text).
  constexpr int kMaxVersion = qrcodegen_VERSION_MAX;  // 40
  constexpr size_t kBufLen = qrcodegen_BUFFER_LEN_FOR_VERSION(kMaxVersion);

  // Heap-allocate both buffers to avoid blowing the stack (~3763 bytes each).
  auto tempBuf = std::make_unique<uint8_t[]>(kBufLen);
  auto qrcode = std::make_unique<uint8_t[]>(kBufLen);

  bool ok = qrcodegen_encodeText(
    textPayload.c_str(),
    tempBuf.get(),
    qrcode.get(),
    qrcodegen_Ecc_LOW,
    qrcodegen_VERSION_MIN,  // minVersion = 1 (library picks smallest that fits)
    kMaxVersion,
    qrcodegen_Mask_AUTO,
    true  // boostEcl: allow upgrading ECC if space permits
  );

  if (!ok) {
    LOG_ERR("QR", "Failed to encode QR code (text length: %u)", (unsigned)textPayload.length());
    return;
  }

  const int size = qrcodegen_getSize(qrcode.get());
  const int maxDim = std::min(bounds.width, bounds.height);

  int px = maxDim / size;
  if (px < 1) px = 1;

  // Center within bounds
  const int qrDisplaySize = size * px;
  const int xOff = bounds.x + (bounds.width - qrDisplaySize) / 2;
  const int yOff = bounds.y + (bounds.height - qrDisplaySize) / 2;

  for (int cy = 0; cy < size; cy++) {
    for (int cx = 0; cx < size; cx++) {
      if (qrcodegen_getModule(qrcode.get(), cx, cy)) {
        renderer.fillRect(xOff + px * cx, yOff + px * cy, px, px, true);
      }
    }
  }
}
