#pragma once
#include "../Activity.h"
#include "util/ButtonNavigator.h"

#include <string>
#include <vector>

// Simple file picker that shows folders + .csv files.
// Returns FilePathResult with the selected CSV path, or isCancelled if Back pressed.
class FlashcardFilePickerActivity final : public Activity {
    ButtonNavigator buttonNavigator;
    std::string basepath;
    std::vector<std::string> files;
    int selectorIndex = 0;

    void loadFiles();

    static bool hasCsvExtension(const std::string& name) {
        if (name.size() < 5) return false;
        auto ext = name.substr(name.size() - 4);
        return ext == ".csv" || ext == ".CSV";
    }

public:
    explicit FlashcardFilePickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
        : Activity("FlashcardFilePicker", renderer, mappedInput), basepath("/") {}

    void onEnter() override;
    void loop() override;
    void render(RenderLock&&) override;
};
