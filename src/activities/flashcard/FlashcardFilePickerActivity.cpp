#include "FlashcardFilePickerActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"
#include "../ActivityResult.h"

void FlashcardFilePickerActivity::loadFiles() {
    files.clear();

    auto root = Storage.open(basepath.c_str());
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return;
    }

    root.rewindDirectory();
    char name[256];
    for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
        file.getName(name, sizeof(name));
        if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
            file.close();
            continue;
        }
        if (file.isDirectory()) {
            files.emplace_back(std::string(name) + "/");
        } else if (hasCsvExtension(name)) {
            files.emplace_back(name);
        }
        file.close();
    }
    root.close();
    std::sort(files.begin(), files.end());
}

void FlashcardFilePickerActivity::onEnter() {
    Activity::onEnter();
    loadFiles();
    requestUpdate();
}

void FlashcardFilePickerActivity::loop() {
    const int total = static_cast<int>(files.size());

    buttonNavigator.onNext([this, total] {
        selectorIndex = ButtonNavigator::nextIndex(selectorIndex, total);
        requestUpdate();
    });
    buttonNavigator.onPrevious([this, total] {
        selectorIndex = ButtonNavigator::previousIndex(selectorIndex, total);
        requestUpdate();
    });

    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        if (basepath != "/") {
            // Go up one directory
            auto pos = basepath.find_last_of('/');
            basepath = (pos == 0) ? "/" : basepath.substr(0, pos);
            selectorIndex = 0;
            loadFiles();
            requestUpdate();
        } else {
            // At root — cancel
            ActivityResult cancelled;
            cancelled.isCancelled = true;
            setResult(std::move(cancelled));
            finish();
        }
        return;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (files.empty()) return;

        const std::string& entry = files[selectorIndex];
        bool isDir = entry.back() == '/';

        std::string fullBase = basepath;
        if (fullBase.back() != '/') fullBase += "/";

        if (isDir) {
            basepath = fullBase + entry.substr(0, entry.size() - 1);
            selectorIndex = 0;
            loadFiles();
            requestUpdate();
        } else {
            // CSV selected — return the full path
            ActivityResult result;
            result.data = FilePathResult{fullBase + entry};
            setResult(std::move(result));
            finish();
        }
    }
}

void FlashcardFilePickerActivity::render(RenderLock&&) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();

    renderer.clearScreen();

    // Header: "Files" with current path as subtitle
    GUI.drawHeader(renderer,
                   Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                   tr(STR_FLASHCARD_IMPORT));

    // Show current path below header
    int y = metrics.topPadding + metrics.headerHeight + 2;
    renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding, y,
                      basepath == "/" ? "SD Card" : basepath.c_str());
    y += renderer.getLineHeight(SMALL_FONT_ID) + 2;

    // Draw separator
    renderer.drawLine(0, y, pageWidth, y, true);
    y += 2;

    const int menuTop = y;
    const int menuHeight = pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

    if (files.empty()) {
        renderer.drawCenteredText(UI_10_FONT_ID, menuTop + menuHeight / 2, "No .csv files");
    } else {
        GUI.drawList(
            renderer,
            Rect{0, menuTop, pageWidth, menuHeight},
            static_cast<int>(files.size()),
            selectorIndex,
            [this](int i) -> std::string { return files[i]; },
            nullptr,  // no subtitle
            nullptr,  // no icon
            [this](int i) -> std::string {
                const auto& f = files[i];
                if (f.back() == '/') return tr(STR_FOLDER);
                return ".csv";
            });
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT),
                                              tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    renderer.displayBuffer();
}
