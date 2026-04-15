#pragma once
#include "../Activity.h"
#include "FlashcardDeck.h"
#include "FlashcardSrs.h"
#include <vector>
#include <string>

class FlashcardReviewActivity final : public Activity {
public:
    FlashcardReviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                            const std::string& deckPath);
    void onEnter() override;
    void loop() override;
    void render(RenderLock&&) override;

private:
    enum class CardState { Front, Back };

    void flipCard();
    void rateCard(SrsRating rating);
    void renderFront(const FlashcardCard& card);
    void renderBack(const FlashcardCard& card);

    FlashcardDeck deck;
    std::string deckPath;
    std::vector<size_t> reviewQueue;
    size_t currentQueueIndex = 0;
    CardState cardState = CardState::Front;
    uint32_t today = 0;
    uint8_t saveCounter = 0;  // batch save every 5 cards
    size_t maxQueueSize = 0;  // cap for re-queue growth (2x initial)
};
