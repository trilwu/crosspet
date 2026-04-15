#pragma once
#include "../Activity.h"
#include "util/ButtonNavigator.h"

class FlashcardSettingsActivity final : public Activity {
    ButtonNavigator buttonNavigator;
    int selectedIndex = 0;
    bool editing = false;

    uint8_t newPerDay = 10;
    uint8_t maxReview = 250;

    void saveSettings();

public:
    explicit FlashcardSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
        : Activity("FlashcardSettings", renderer, mappedInput) {}

    void onEnter() override;
    void loop() override;
    void render(RenderLock&&) override;
};
