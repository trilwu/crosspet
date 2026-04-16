#ifdef ENABLE_BLE
// BluetoothSettingsLearnMode.cpp
// Learn mode key capture: handleLearnInput() + renderLearnKeys()
#include "BluetoothSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "DeviceProfiles.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BluetoothSettingsActivity::handleLearnInput() {
  if (pendingLearnKey != 0) {
    const uint8_t capturedKey = pendingLearnKey;
    pendingLearnKey = 0;

    if (learnStep == LearnStep::WAIT_PREV) {
      learnedPrevKey = capturedKey;
      learnStep = LearnStep::WAIT_NEXT;
      char buf[64];
      snprintf(buf, sizeof(buf), "Prev=0x%02X captured, press NEXT", learnedPrevKey);
      lastError = buf;
      requestUpdate();
      return;
    }

    if (learnStep == LearnStep::WAIT_NEXT) {
      if (capturedKey == learnedPrevKey) {
        lastError = "Next key must be different";
        requestUpdate();
        return;
      }
      learnedNextKey = capturedKey;
      // Transition to WAIT_TEST instead of saving immediately
      learnStep = LearnStep::WAIT_TEST;
      learnTestDeadlineMs = millis() + 10000;  // 10s deadline
      learnTestPrevPressed = false;
      learnTestNextPressed = false;
      learnedReportIndex = 2;  // Default byte index (limitation: callback doesn't report this)
      lastError = "Press both buttons to confirm (10s)";
      requestUpdate();
      return;
    }

    if (learnStep == LearnStep::WAIT_TEST) {
      if (capturedKey == learnedPrevKey) learnTestPrevPressed = true;
      else if (capturedKey == learnedNextKey) learnTestNextPressed = true;

      if (learnTestPrevPressed && learnTestNextPressed) {
        // Both confirmed — save per-device profile
        auto connDevs = btMgr->getConnectedDevices();
        if (!connDevs.empty()) {
          DeviceProfiles::setCustomProfileForDevice(
            connDevs[0], learnedPrevKey, learnedNextKey, learnedReportIndex);
        } else {
          DeviceProfiles::setCustomProfile(learnedPrevKey, learnedNextKey, learnedReportIndex);
        }
        learnStep = LearnStep::DONE;
        char buf[96];
        snprintf(buf, sizeof(buf), "Saved Prev=0x%02X Next=0x%02X", learnedPrevKey, learnedNextKey);
        lastError = buf;
      } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Prev:%s Next:%s",
                 learnTestPrevPressed ? "OK" : "?", learnTestNextPressed ? "OK" : "?");
        lastError = buf;
      }
      requestUpdate();
      return;
    }
  }

  // WAIT_TEST timeout check (no key needed)
  if (learnStep == LearnStep::WAIT_TEST && millis() >= learnTestDeadlineMs) {
    viewMode = ViewMode::MAIN_MENU;
    selectedIndex = 0;
    lastError = "Timed out — keys not saved";
    requestUpdate();
    return;
  }

  // Confirm exits learn mode once done
  if (learnStep == LearnStep::DONE && mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    viewMode = ViewMode::MAIN_MENU;
    selectedIndex = 0;
    requestUpdate();
  }
}

void BluetoothSettingsActivity::renderLearnKeys() {
  auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Learn Page-Turn Keys");

  // Step prompt
  const char* stepText = "Press PREVIOUS PAGE button";
  if (learnStep == LearnStep::WAIT_NEXT) {
    stepText = "Press NEXT PAGE button";
  } else if (learnStep == LearnStep::WAIT_TEST) {
    stepText = "Press both buttons to confirm";
  } else if (learnStep == LearnStep::DONE) {
    stepText = "Learning complete";
  }
  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    stepText);

  // Capture status lines
  char line1[64];
  char line2[64];
  snprintf(line1, sizeof(line1), "Prev key: %s", learnedPrevKey ? "captured" : "waiting");
  snprintf(line2, sizeof(line2), "Next key: %s", learnedNextKey ? "captured" : "waiting");

  const int bodyY = metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight;
  renderer.drawCenteredText(UI_12_FONT_ID, bodyY + 32, line1);
  renderer.drawCenteredText(UI_12_FONT_ID, bodyY + 56, line2);

  if (learnStep == LearnStep::WAIT_TEST) {
    char testLine[64];
    unsigned long remaining = (learnTestDeadlineMs > millis()) ? (learnTestDeadlineMs - millis()) / 1000 : 0;
    snprintf(testLine, sizeof(testLine), "Prev:%s  Next:%s  (%lus left)",
             learnTestPrevPressed ? "OK" : "?", learnTestNextPressed ? "OK" : "?", remaining);
    renderer.drawCenteredText(UI_10_FONT_ID, bodyY + 80, testLine);
  }

  if (!lastError.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - metrics.buttonHintsHeight - 16, lastError.c_str());
  }

  const auto confirmLabel = (learnStep == LearnStep::DONE) ? tr(STR_SELECT) : "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
#endif // ENABLE_BLE
