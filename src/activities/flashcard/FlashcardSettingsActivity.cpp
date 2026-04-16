#include "FlashcardSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPetSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

static constexpr int SETTINGS_COUNT = 2;
static constexpr uint8_t NEW_PER_DAY_MIN  = 5;
static constexpr uint8_t NEW_PER_DAY_MAX  = 50;
static constexpr uint8_t NEW_PER_DAY_STEP = 5;
static constexpr uint8_t MAX_REVIEW_MIN   = 25;
// uint8_t max is 255; step is 25 (255 = 10*25 + 5, so we cap at 250 to keep steps clean)
static constexpr uint8_t MAX_REVIEW_MAX   = 250;
static constexpr uint8_t MAX_REVIEW_STEP  = 25;

void FlashcardSettingsActivity::onEnter() {
    Activity::onEnter();
    newPerDay = PET_SETTINGS.flashcardNewPerDay;
    maxReview = PET_SETTINGS.flashcardMaxReviewPerDay;

    // Clamp to valid range in case stored value is out of range
    if (newPerDay < NEW_PER_DAY_MIN || newPerDay > NEW_PER_DAY_MAX)
        newPerDay = (newPerDay < NEW_PER_DAY_MIN) ? NEW_PER_DAY_MIN : NEW_PER_DAY_MAX;
    if (maxReview < MAX_REVIEW_MIN || maxReview > MAX_REVIEW_MAX)
        maxReview = (maxReview < MAX_REVIEW_MIN) ? MAX_REVIEW_MIN : MAX_REVIEW_MAX;

    requestUpdate();
}

void FlashcardSettingsActivity::saveSettings() {
    PET_SETTINGS.flashcardNewPerDay = newPerDay;
    PET_SETTINGS.flashcardMaxReviewPerDay = maxReview;
    PET_SETTINGS.saveToFile();
}

void FlashcardSettingsActivity::loop() {
    if (!editing) {
        buttonNavigator.onNext([this] {
            selectedIndex = ButtonNavigator::nextIndex(selectedIndex, SETTINGS_COUNT);
            requestUpdate();
        });

        buttonNavigator.onPrevious([this] {
            selectedIndex = ButtonNavigator::previousIndex(selectedIndex, SETTINGS_COUNT);
            requestUpdate();
        });

        if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
            editing = true;
            requestUpdate();
        }

        if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
            saveSettings();
            finish();
        }
    } else {
        // Editing mode: Up increases value, Down decreases value
        buttonNavigator.onNext([this] {
            if (selectedIndex == 0) {
                // newPerDay: increase by step, cap at max (cast to int to avoid uint8_t overflow)
                if (static_cast<int>(newPerDay) + NEW_PER_DAY_STEP <= NEW_PER_DAY_MAX)
                    newPerDay += NEW_PER_DAY_STEP;
                else
                    newPerDay = NEW_PER_DAY_MAX;
            } else {
                // maxReview: increase by step, cap at max
                if (static_cast<int>(maxReview) + MAX_REVIEW_STEP <= MAX_REVIEW_MAX)
                    maxReview += MAX_REVIEW_STEP;
                else
                    maxReview = MAX_REVIEW_MAX;
            }
            requestUpdate();
        });

        buttonNavigator.onPrevious([this] {
            if (selectedIndex == 0) {
                // newPerDay: decrease by step, floor at min (cast to int to avoid uint8_t underflow)
                if (static_cast<int>(newPerDay) - NEW_PER_DAY_STEP >= NEW_PER_DAY_MIN)
                    newPerDay -= NEW_PER_DAY_STEP;
                else
                    newPerDay = NEW_PER_DAY_MIN;
            } else {
                // maxReview: decrease by step, floor at min
                if (static_cast<int>(maxReview) - MAX_REVIEW_STEP >= MAX_REVIEW_MIN)
                    maxReview -= MAX_REVIEW_STEP;
                else
                    maxReview = MAX_REVIEW_MIN;
            }
            requestUpdate();
        });

        if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
            mappedInput.wasReleased(MappedInputManager::Button::Back)) {
            editing = false;
            requestUpdate();
        }
    }
}

void FlashcardSettingsActivity::render(RenderLock&&) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();

    renderer.clearScreen();

    GUI.drawHeader(renderer,
                   Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                   tr(STR_FLASHCARD_SETTINGS));

    const int menuTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int menuHeight = pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

    // Build value strings, marking the editing row with an indicator
    char val0[16], val1[16];
    if (editing && selectedIndex == 0) {
        snprintf(val0, sizeof(val0), "[ %d ]", static_cast<int>(newPerDay));
    } else {
        snprintf(val0, sizeof(val0), "%d", static_cast<int>(newPerDay));
    }
    if (editing && selectedIndex == 1) {
        snprintf(val1, sizeof(val1), "[ %d ]", static_cast<int>(maxReview));
    } else {
        snprintf(val1, sizeof(val1), "%d", static_cast<int>(maxReview));
    }

    const char* values[SETTINGS_COUNT] = {val0, val1};

    GUI.drawList(
        renderer,
        Rect{0, menuTop, pageWidth, menuHeight},
        SETTINGS_COUNT,
        selectedIndex,
        [](int i) -> std::string {
            if (i == 0) return tr(STR_FLASHCARD_NEW_PER_DAY);
            return tr(STR_FLASHCARD_MAX_REVIEW);
        },
        nullptr,
        nullptr,
        [&values](int i) -> std::string {
            return values[i];
        },
        true);  // highlightValue = true to visually distinguish the value column

    // Button hints differ by mode: Back=save&exit (non-editing), or confirm (editing)
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT),
                                             tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    renderer.displayBuffer();
}
