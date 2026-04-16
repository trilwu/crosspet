#pragma once
#include "../Activity.h"
#include "FlashcardDeck.h"
#include "util/ButtonNavigator.h"

#include <string>
#include <vector>

class FlashcardDeckListActivity final : public Activity {
    ButtonNavigator buttonNavigator;

    struct DeckEntry {
        std::string name;
        std::string path;
        DeckStats stats;
    };

    std::vector<DeckEntry> decks;
    int selectedIndex = 0;
    uint32_t today = 0;
    uint16_t totalDueToday = 0;

    void loadDeckList();

    // Two extra items appended after deck list: Import + Settings
    static constexpr int EXTRA_ITEMS = 2;

public:
    explicit FlashcardDeckListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
        : Activity("FlashcardDeckList", renderer, mappedInput) {}

    void onEnter() override;
    void loop() override;
    void render(RenderLock&&) override;
};
