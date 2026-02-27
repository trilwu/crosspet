#include "ConferenceBadgeActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstring>

#include "components/UITheme.h"
#include "fontIds.h"
#include "util/QrUtils.h"

namespace {
constexpr const char* BADGE_FILE = "/badge.txt";
constexpr size_t MAX_BADGE_SIZE = 512;
}  // namespace

void ConferenceBadgeActivity::onEnter() {
  Activity::onEnter();

  // Reset state on re-entry
  badgeName.clear();
  badgeTitle.clear();
  qrData.clear();
  fileLoaded = false;

  if (Storage.exists(BADGE_FILE)) {
    char buffer[MAX_BADGE_SIZE];
    size_t bytesRead = Storage.readFileToBuffer(BADGE_FILE, buffer, MAX_BADGE_SIZE);
    if (bytesRead > 0) {
      fileLoaded = true;
      // Parse lines: name, title, qr data
      const char* ptr = buffer;
      const char* end = buffer + bytesRead;
      int lineNum = 0;

      while (ptr < end && lineNum < 3) {
        const char* lineEnd = ptr;
        while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') lineEnd++;

        std::string line(ptr, lineEnd - ptr);

        // Trim trailing whitespace
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();

        switch (lineNum) {
          case 0:
            badgeName = std::move(line);
            break;
          case 1:
            badgeTitle = std::move(line);
            break;
          case 2:
            qrData = std::move(line);
            break;
        }

        ptr = lineEnd;
        if (ptr < end && *ptr == '\r') ptr++;
        if (ptr < end && *ptr == '\n') ptr++;
        lineNum++;
      }
    }
  }

  requestUpdate();
}

void ConferenceBadgeActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
}

void ConferenceBadgeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  if (!fileLoaded || badgeName.empty()) {
    const int y = (pageHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_PLACE_BADGE_TXT));
  } else {
    const int nameHeight = renderer.getLineHeight(UI_12_FONT_ID);
    const int titleHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int spacing = 20;

    // Calculate available QR area
    const int qrAvailW = pageWidth - 40;
    const int qrAvailH = pageHeight / 3;

    // Calculate total content height to center vertically
    int totalHeight = nameHeight;
    if (!badgeTitle.empty()) totalHeight += spacing + titleHeight;
    if (!qrData.empty()) totalHeight += spacing + qrAvailH;

    int y = (pageHeight - totalHeight) / 2;

    // Name large and centered
    renderer.drawCenteredText(UI_12_FONT_ID, y, badgeName.c_str(), true, EpdFontFamily::BOLD);
    y += nameHeight + spacing;

    // Title centered below name
    if (!badgeTitle.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, y, badgeTitle.c_str());
      y += titleHeight + spacing;
    }

    // QR code centered using existing QrUtils
    if (!qrData.empty()) {
      const Rect qrBounds(20, y, qrAvailW, qrAvailH);
      QrUtils::drawQrCode(renderer, qrBounds, qrData);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
