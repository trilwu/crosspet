#include "FlashcardDeckListActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include "FlashcardReviewActivity.h"
#include "FlashcardSettingsActivity.h"
#include "FlashcardSrs.h"
#include "CrossPetSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

void FlashcardDeckListActivity::onEnter() {
    Activity::onEnter();
    today = FlashcardSrs::getToday();
    loadDeckList();
    requestUpdate();
}

void FlashcardDeckListActivity::loadDeckList() {
    decks.clear();
    totalDueToday = 0;

    const auto paths = FlashcardDeck::listDecks();
    for (const auto& path : paths) {
        FlashcardDeck deck;
        if (!deck.loadFromCsv(path.c_str())) continue;

        DeckEntry entry;
        entry.name = deck.getName();
        entry.path = path;
        entry.stats = deck.getStats(today);
        totalDueToday += entry.stats.newCards + entry.stats.dueCards;
        decks.push_back(std::move(entry));
    }

    // Clamp selectedIndex in case deck list shrank
    const int total = static_cast<int>(decks.size()) + EXTRA_ITEMS;
    if (selectedIndex >= total) selectedIndex = total > 0 ? total - 1 : 0;
}

void FlashcardDeckListActivity::loop() {
    // Dismiss import message on any button press
    if (showingImportMessage) {
        if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
            mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
            showingImportMessage = false;
            loadDeckList();
            requestUpdate();
        }
        return;
    }

    const int total = static_cast<int>(decks.size()) + EXTRA_ITEMS;

    buttonNavigator.onNext([this, total] {
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, total);
        requestUpdate();
    });

    buttonNavigator.onPrevious([this, total] {
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, total);
        requestUpdate();
    });

    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        finish();
        return;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        const int deckCount = static_cast<int>(decks.size());

        if (selectedIndex < deckCount) {
            // Launch review for the selected deck
            const std::string deckPath = decks[selectedIndex].path;
            startActivityForResult(
                std::make_unique<FlashcardReviewActivity>(renderer, mappedInput, deckPath),
                [this](const ActivityResult&) {
                    today = FlashcardSrs::getToday();
                    loadDeckList();
                    requestUpdate();
                });
        } else if (selectedIndex == deckCount) {
            // Import: show message about placing CSV files
            showingImportMessage = true;
            requestUpdate();
        } else {
            // Settings
            startActivityForResult(
                std::make_unique<FlashcardSettingsActivity>(renderer, mappedInput),
                [this](const ActivityResult&) {
                    today = FlashcardSrs::getToday();
                    loadDeckList();
                    requestUpdate();
                });
        }
    }
}

void FlashcardDeckListActivity::render(RenderLock&&) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();

    renderer.clearScreen();

    // Import message overlay
    if (showingImportMessage) {
        GUI.drawHeader(renderer,
                       Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                       tr(STR_FLASHCARD_IMPORT));
        const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
        const int y = (pageHeight - lineH * 2) / 2;
        renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_FLASHCARD_IMPORT_HINT));
        renderer.drawCenteredText(SMALL_FONT_ID, y + lineH + 4, FLASHCARD_DIR);
        const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
        GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
        renderer.displayBuffer();
        return;
    }

    // Build subtitle: "Reviews: X/Y"
    char subtitle[32];
    snprintf(subtitle, sizeof(subtitle), "Reviews: %d/%d",
             static_cast<int>(totalDueToday),
             static_cast<int>(PET_SETTINGS.flashcardMaxReviewPerDay));

    GUI.drawHeader(renderer,
                   Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                   tr(STR_FLASHCARD_DECK_SELECT), subtitle);

    const int menuTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int menuHeight = pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

    const int deckCount = static_cast<int>(decks.size());
    const int total = deckCount + EXTRA_ITEMS;

    if (deckCount == 0) {
        // No decks yet — draw centered hint text
        const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
        const int y = menuTop + (menuHeight - lineH) / 2;
        renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_FLASHCARD_NO_DECKS), true);
    }

    // Always draw the list (includes Import + Settings even when no decks)
    GUI.drawList(
        renderer,
        Rect{0, menuTop, pageWidth, menuHeight},
        total,
        selectedIndex,
        [this, deckCount](int i) -> std::string {
            if (i < deckCount) return decks[i].name;
            if (i == deckCount) return tr(STR_FLASHCARD_IMPORT);
            return tr(STR_FLASHCARD_SETTINGS);
        },
        [this, deckCount](int i) -> std::string {
            if (i >= deckCount) return "";
            char buf[48];
            snprintf(buf, sizeof(buf), "New: %d  Due: %d  Total: %d",
                     static_cast<int>(decks[i].stats.newCards),
                     static_cast<int>(decks[i].stats.dueCards),
                     static_cast<int>(decks[i].stats.totalCards));
            return buf;
        },
        nullptr,
        nullptr);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT),
                                              tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    renderer.displayBuffer();
}
