#pragma once
#include "../Activity.h"

class FlashcardDoneActivity final : public Activity {
public:
    explicit FlashcardDoneActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
        : Activity("FlashcardDone", renderer, mappedInput) {}

    void onEnter() override;
    void loop() override;
    void render(RenderLock&&) override;
};
