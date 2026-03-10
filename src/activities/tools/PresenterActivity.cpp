#include "PresenterActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "components/UITheme.h"
#include "components/icons/presenter.h"
#include "fontIds.h"

void PresenterActivity::onEnter() {
  Activity::onEnter();
  state = State::ADVERTISING;
  if (!presenter.init()) {
    LOG_ERR("PRESENTER", "BLE init failed, exiting");
    finish();
    return;
  }
  requestUpdate();
}

void PresenterActivity::cleanup() {
  presenter.deinit();
}

void PresenterActivity::onExit() {
  cleanup();
  Activity::onExit();
}

void PresenterActivity::loop() {
  switch (state) {
    case State::ADVERTISING: {
      if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        finish();
        return;
      }
      if (presenter.isConnected()) {
        state = State::CONNECTED;
        requestUpdate();
      }
      break;
    }

    case State::CONNECTED: {
      // Read all button events into locals first — wasReleased() is one-shot per frame
      const bool right   = mappedInput.wasReleased(MappedInputManager::Button::Right);
      const bool left    = mappedInput.wasReleased(MappedInputManager::Button::Left);
      const bool confirm = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
      const bool back    = mappedInput.wasReleased(MappedInputManager::Button::Back);
      const bool backLongPress = back && mappedInput.getHeldTime() > 800;

      if (right)   presenter.sendKey(0x4F);  // Right Arrow → next slide
      if (left)    presenter.sendKey(0x50);  // Left Arrow  → prev slide
      if (confirm) presenter.sendKey(0x3E);  // F5          → start presentation
      if (backLongPress) { finish(); return; }  // Long-press Back → exit presenter
      if (back)    presenter.sendKey(0x29);  // Short-press Back → Escape

      // Host disconnected → return to advertising
      if (!presenter.isConnected()) {
        state = State::ADVERTISING;
        requestUpdate();
      }
      break;
    }
  }
}

void PresenterActivity::render(RenderLock&&) {
  const auto& metrics   = UITheme::getInstance().getMetrics();
  const auto pageWidth  = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_PRESENTER));

  const int contentTop    = metrics.topPadding + metrics.headerHeight;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight;
  const int centerY       = (contentTop + contentBottom) / 2;

  // Draw pen icon centered above content
  const int iconX = (pageWidth - PRESENTER_ICON_W) / 2;
  const int iconY = contentTop + 20;
  renderer.drawIcon(PresenterPenIcon, iconX, iconY, PRESENTER_ICON_W, PRESENTER_ICON_H);
  const int textAreaTop = iconY + PRESENTER_ICON_H + 16;

  if (state == State::ADVERTISING) {
    const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
    const int advCenterY = (textAreaTop + contentBottom) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, advCenterY - lineH, tr(STR_PRESENTER_ADVERTISING));
    renderer.drawCenteredText(SMALL_FONT_ID, advCenterY + 8, tr(STR_PRESENTER_PAIR_HINT));

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  } else {
    // Show key legend: button → action rows
    const int lineH   = renderer.getLineHeight(SMALL_FONT_ID);
    const int spacing = lineH + 6;
    const int blockH  = spacing * 4;
    const int legendCenterY = (textAreaTop + contentBottom) / 2;
    int y = legendCenterY - blockH / 2;

    struct { const char* label; const char* action; } rows[] = {
        {tr(STR_PRESENTER_BTN_NEXT),  tr(STR_PRESENTER_ACT_NEXT)},
        {tr(STR_PRESENTER_BTN_PREV),  tr(STR_PRESENTER_ACT_PREV)},
        {tr(STR_PRESENTER_BTN_START), tr(STR_PRESENTER_ACT_START)},
        {tr(STR_PRESENTER_BTN_ESC),   tr(STR_PRESENTER_ACT_ESC)},
    };
    for (const auto& row : rows) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s  %s", row.label, row.action);
      renderer.drawCenteredText(SMALL_FONT_ID, y, buf);
      y += spacing;
    }

    const auto labels = mappedInput.mapLabels(
        tr(STR_PRESENTER_ESC), tr(STR_PRESENTER_START),
        tr(STR_PRESENTER_PREV), tr(STR_PRESENTER_NEXT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
