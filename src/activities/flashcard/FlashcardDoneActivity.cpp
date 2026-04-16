#include "FlashcardDoneActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"

void FlashcardDoneActivity::onEnter() {
    Activity::onEnter();
    requestUpdate();
}

void FlashcardDoneActivity::loop() {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        finish();
    }
}

void FlashcardDoneActivity::render(RenderLock&&) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();

    renderer.clearScreen();

    GUI.drawHeader(renderer,
                   Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                   tr(STR_FLASHCARD));

    // Vertically center the done message in the content area
    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
    const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
    const int textY = contentTop + (contentHeight - lineH) / 2;

    renderer.drawCenteredText(UI_10_FONT_ID, textY, tr(STR_FLASHCARD_DONE_MSG), true);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    renderer.displayBuffer();
}
