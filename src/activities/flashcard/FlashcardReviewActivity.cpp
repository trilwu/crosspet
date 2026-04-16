#include "FlashcardReviewActivity.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <I18n.h>

#include <cstring>

#include "components/UITheme.h"
#include "CrossPetSettings.h"
#include "fontIds.h"
#include "FlashcardDoneActivity.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

FlashcardReviewActivity::FlashcardReviewActivity(GfxRenderer& renderer,
                                                 MappedInputManager& mappedInput,
                                                 const std::string& deckPath)
    : Activity("FlashcardReview", renderer, mappedInput), deckPath(deckPath) {}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void FlashcardReviewActivity::onEnter() {
    Activity::onEnter();
    today = FlashcardSrs::getToday();
    deck.loadFromCsv(deckPath.c_str());
    reviewQueue = deck.buildReviewQueue(today,
                                        PET_SETTINGS.flashcardNewPerDay,
                                        PET_SETTINGS.flashcardMaxReviewPerDay);
    currentQueueIndex = 0;
    cardState = CardState::Front;
    saveCounter = 0;

    if (reviewQueue.empty()) {
        activityManager.pushActivity(
            std::make_unique<FlashcardDoneActivity>(renderer, mappedInput));
        return;
    }
    maxQueueSize = reviewQueue.size() * 2;
    requestUpdate();
}

// ─── Input loop ───────────────────────────────────────────────────────────────

void FlashcardReviewActivity::loop() {
    if (reviewQueue.empty() || currentQueueIndex >= reviewQueue.size()) return;

    if (cardState == CardState::Front) {
        if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
            deck.saveToCsv(deckPath.c_str());
            finish();
            return;
        }
        if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
            flipCard();
        }
    } else {
        // Back state — 4 buttons map to 4 SRS ratings
        // Back→Again, Left/Up→Hard, Right/Down→Good, Confirm→Easy
        if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
            rateCard(SrsRating::Again);
        } else if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
                   mappedInput.wasReleased(MappedInputManager::Button::Up)) {
            rateCard(SrsRating::Hard);
        } else if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
                   mappedInput.wasReleased(MappedInputManager::Button::Down)) {
            rateCard(SrsRating::Good);
        } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
            rateCard(SrsRating::Easy);
        }
    }
}

// ─── Card state transitions ───────────────────────────────────────────────────

void FlashcardReviewActivity::flipCard() {
    cardState = CardState::Back;
    requestUpdate();
}

void FlashcardReviewActivity::rateCard(SrsRating rating) {
    size_t cardIdx = reviewQueue[currentQueueIndex];
    auto& card = deck.getCards()[cardIdx];
    SrsState newState = FlashcardSrs::review(card.srs, rating, today);
    deck.updateCard(cardIdx, newState);

    if (rating == SrsRating::Again && reviewQueue.size() < maxQueueSize) {
        reviewQueue.push_back(cardIdx);  // re-queue failed card at end
    }

    currentQueueIndex++;
    saveCounter++;

    if (saveCounter >= 5) {
        deck.saveToCsv(deckPath.c_str());
        saveCounter = 0;
    }

    if (currentQueueIndex >= reviewQueue.size()) {
        deck.saveToCsv(deckPath.c_str());
        activityManager.pushActivity(
            std::make_unique<FlashcardDoneActivity>(renderer, mappedInput));
    } else {
        cardState = CardState::Front;
        requestUpdate();
    }
}

// ─── Render ───────────────────────────────────────────────────────────────────

void FlashcardReviewActivity::render(RenderLock&&) {
    if (reviewQueue.empty() || currentQueueIndex >= reviewQueue.size()) return;

    const auto& card = deck.getCards()[reviewQueue[currentQueueIndex]];
    renderer.clearScreen();

    const auto& metrics = UITheme::getInstance().getMetrics();
    auto pageWidth = renderer.getScreenWidth();

    GUI.drawHeader(renderer,
                   Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                   deck.getName().c_str());

    if (cardState == CardState::Front) {
        renderFront(card);
    } else {
        renderBack(card);
    }

    renderer.displayBuffer();
}

// ─── renderFront ──────────────────────────────────────────────────────────────

void FlashcardReviewActivity::renderFront(const FlashcardCard& card) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    auto pageHeight = renderer.getScreenHeight();
    int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
    int contentHeight = contentBottom - contentTop;

    std::string mainText = card.frontMain();
    int mainLineHeight = renderer.getLineHeight(BOOKERLY_18_FONT_ID);

    // Position main text in the upper-third of the content area
    int textY = contentTop + contentHeight / 3;

    // Render main text — split on '\n' and draw each line centered
    size_t pos = 0;
    while (pos < mainText.size()) {
        size_t nl = mainText.find('\n', pos);
        std::string line = (nl == std::string::npos)
                               ? mainText.substr(pos)
                               : mainText.substr(pos, nl - pos);
        renderer.drawCenteredText(BOOKERLY_18_FONT_ID, textY, line.c_str());
        textY += mainLineHeight;
        pos = (nl == std::string::npos) ? mainText.size() : nl + 1;
    }

    // Optional hint text below main text
    std::string hint = card.frontHint();
    if (!hint.empty()) {
        textY += metrics.verticalSpacing;
        renderer.drawCenteredText(UI_10_FONT_ID, textY, hint.c_str());
    }

    // Button hints: Back + Show
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_FLASHCARD_SHOW), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

// ─── renderBack ───────────────────────────────────────────────────────────────

void FlashcardReviewActivity::renderBack(const FlashcardCard& card) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    auto pageWidth = renderer.getScreenWidth();
    auto pageHeight = renderer.getScreenHeight();
    int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

    // --- Two-line SRS buttons at bottom (draw in Portrait like drawButtonHints) ---
    const auto origOrientation = renderer.getOrientation();
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);

    constexpr int btnWidth = 106;
    constexpr int btnHeight = 50;  // taller than normal 40px to fit 2 lines
    const int portHeight = renderer.getScreenHeight();
    const int btnY = portHeight - btnHeight;
    const int x4Pos[] = {25, 130, 245, 350};
    const int x3Pos[] = {38, 154, 268, 384};
    const int* btnPos = gpio.deviceIsX3() ? x3Pos : x4Pos;

    // Copy previewInterval results — static buffer aliasing
    const auto& srs = card.srs;
    char iv1[16], iv2[16], iv3[16], iv4[16];
    strncpy(iv1, FlashcardSrs::previewInterval(srs, SrsRating::Again), sizeof(iv1));
    strncpy(iv2, FlashcardSrs::previewInterval(srs, SrsRating::Hard), sizeof(iv2));
    strncpy(iv3, FlashcardSrs::previewInterval(srs, SrsRating::Good), sizeof(iv3));
    strncpy(iv4, FlashcardSrs::previewInterval(srs, SrsRating::Easy), sizeof(iv4));
    iv1[15] = iv2[15] = iv3[15] = iv4[15] = '\0';

    // Physical mapping: Back→Again, Confirm→Easy, Left/Up→Hard, Right/Down→Good
    // In PortraitInverted, swap label order like drawButtonHints does
    const char* intervals_raw[] = {iv1, iv4, iv2, iv3};
    const char* ratings_raw[] = {
        tr(STR_FLASHCARD_AGAIN), tr(STR_FLASHCARD_EASY),
        tr(STR_FLASHCARD_HARD), tr(STR_FLASHCARD_GOOD)};

    const char* intervals[4];
    const char* ratings[4];
    if (origOrientation == GfxRenderer::Orientation::PortraitInverted) {
        intervals[0] = intervals_raw[3]; intervals[1] = intervals_raw[2];
        intervals[2] = intervals_raw[1]; intervals[3] = intervals_raw[0];
        ratings[0] = ratings_raw[3]; ratings[1] = ratings_raw[2];
        ratings[2] = ratings_raw[1]; ratings[3] = ratings_raw[0];
    } else {
        for (int i = 0; i < 4; i++) { intervals[i] = intervals_raw[i]; ratings[i] = ratings_raw[i]; }
    }

    for (int i = 0; i < 4; i++) {
        int x = btnPos[i];
        renderer.fillRect(x, btnY, btnWidth, btnHeight, false);
        renderer.drawRect(x, btnY, btnWidth, btnHeight);
        // Line 1: interval (centered)
        int tw1 = renderer.getTextWidth(SMALL_FONT_ID, intervals[i]);
        renderer.drawText(SMALL_FONT_ID, x + (btnWidth - tw1) / 2, btnY + 7, intervals[i]);
        // Line 2: rating label (centered)
        int tw2 = renderer.getTextWidth(UI_10_FONT_ID, ratings[i]);
        renderer.drawText(UI_10_FONT_ID, x + (btnWidth - tw2) / 2, btnY + 27, ratings[i]);
    }

    renderer.setOrientation(origOrientation);

    // --- Content area: front text + hint + separator + answer ---
    int contentBottom = pageHeight - btnHeight - metrics.verticalSpacing;
    int contentHeight = contentBottom - contentTop;

    // Front main text — large, centered in upper portion
    std::string mainText = card.frontMain();
    int mainLineHeight = renderer.getLineHeight(BOOKERLY_18_FONT_ID);
    int textY = contentTop + contentHeight / 3;

    size_t pos = 0;
    while (pos < mainText.size()) {
        size_t nl = mainText.find('\n', pos);
        std::string line = (nl == std::string::npos)
                               ? mainText.substr(pos)
                               : mainText.substr(pos, nl - pos);
        renderer.drawCenteredText(BOOKERLY_18_FONT_ID, textY, line.c_str());
        textY += mainLineHeight;
        pos = (nl == std::string::npos) ? mainText.size() : nl + 1;
    }

    // Hint below front text
    std::string hint = card.frontHint();
    if (!hint.empty()) {
        textY += 4;
        renderer.drawCenteredText(UI_10_FONT_ID, textY, hint.c_str());
        textY += renderer.getLineHeight(UI_10_FONT_ID);
    }

    // Separator line
    textY += metrics.verticalSpacing;
    renderer.drawLine(metrics.contentSidePadding, textY,
                      pageWidth - metrics.contentSidePadding, textY, true);
    textY += metrics.verticalSpacing;

    // Back content (answer)
    renderer.drawCenteredText(UI_10_FONT_ID, textY, card.backContent.c_str());
}
