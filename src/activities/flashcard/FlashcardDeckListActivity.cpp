#include "FlashcardDeckListActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include "FlashcardFilePickerActivity.h"
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
            const std::string deckPath = decks[selectedIndex].path;
            startActivityForResult(
                std::make_unique<FlashcardReviewActivity>(renderer, mappedInput, deckPath),
                [this](const ActivityResult&) {
                    today = FlashcardSrs::getToday();
                    loadDeckList();
                    requestUpdate();
                });
        } else if (selectedIndex == deckCount) {
            // Import: launch file picker to select a CSV
            startActivityForResult(
                std::make_unique<FlashcardFilePickerActivity>(renderer, mappedInput),
                [this](const ActivityResult& result) {
                    if (!result.isCancelled) {
                        auto* fp = std::get_if<FilePathResult>(&result.data);
                        if (fp) {
                            FlashcardDeck deck;
                            deck.importCsv(fp->path.c_str());
                        }
                    }
                    today = FlashcardSrs::getToday();
                    loadDeckList();
                    requestUpdate();
                });
        } else {
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
    const int sidePad = metrics.contentSidePadding;

    renderer.clearScreen();

    // Header
    GUI.drawHeader(renderer,
                   Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                   tr(STR_FLASHCARD_DECK_SELECT));

    int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

    // Subtitle: "Số lượng thẻ review hôm nay: 25/250"
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "%s %d/%d",
             tr(STR_FLASHCARD_REVIEW_TODAY),
             static_cast<int>(totalDueToday),
             static_cast<int>(PET_SETTINGS.flashcardMaxReviewPerDay));
    renderer.drawText(SMALL_FONT_ID, sidePad, y, subtitle);
    y += renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;

    const int deckCount = static_cast<int>(decks.size());
    const int total = deckCount + EXTRA_ITEMS;
    const int cardWidth = pageWidth - sidePad * 2;

    if (deckCount == 0) {
        renderer.drawCenteredText(UI_10_FONT_ID, y + 30, tr(STR_FLASHCARD_NO_DECKS));
        y += 60;
    }

    // Deck cards
    for (int i = 0; i < deckCount; i++) {
        const auto& d = decks[i];
        constexpr int cardHeight = 65;
        const int cardX = sidePad;

        // Selected highlight: filled background
        if (i == selectedIndex) {
            renderer.fillRect(cardX, y, cardWidth, cardHeight, true);
        }
        renderer.drawRect(cardX, y, cardWidth, cardHeight);

        bool inv = (i == selectedIndex);
        // Deck name (bold)
        renderer.drawText(UI_12_FONT_ID, cardX + 8, y + 5, d.name.c_str(), !inv, EpdFontFamily::BOLD);

        // Stats row: "Mới   Đến hạn   Tổng cộng" with values
        int statsY = y + 30;
        int colX = cardX + 8;
        int colSpacing = (cardWidth - 16) / 3;

        // Column headers
        renderer.drawText(SMALL_FONT_ID, colX, statsY, tr(STR_FLASHCARD_NEW), !inv);
        renderer.drawText(SMALL_FONT_ID, colX + colSpacing, statsY, tr(STR_FLASHCARD_DUE), !inv);
        renderer.drawText(SMALL_FONT_ID, colX + colSpacing * 2, statsY, tr(STR_FLASHCARD_TOTAL), !inv);

        // Column values (below headers)
        char val[8];
        int valY = statsY + renderer.getLineHeight(SMALL_FONT_ID) + 2;
        snprintf(val, sizeof(val), "%d", static_cast<int>(d.stats.newCards));
        renderer.drawText(UI_10_FONT_ID, colX, valY, val, !inv);
        snprintf(val, sizeof(val), "%d", static_cast<int>(d.stats.dueCards));
        renderer.drawText(UI_10_FONT_ID, colX + colSpacing, valY, val, !inv);
        snprintf(val, sizeof(val), "%d", static_cast<int>(d.stats.totalCards));
        renderer.drawText(UI_10_FONT_ID, colX + colSpacing * 2, valY, val, !inv);

        y += cardHeight + metrics.verticalSpacing;
    }

    // Import item
    int importIdx = deckCount;
    if (selectedIndex == importIdx) {
        renderer.fillRect(sidePad, y, cardWidth, 30, true);
    }
    renderer.drawText(UI_10_FONT_ID, sidePad + 8, y + 5, tr(STR_FLASHCARD_IMPORT),
                      selectedIndex != importIdx);
    y += 30 + 4;

    // Settings item
    int settingsIdx = deckCount + 1;
    if (selectedIndex == settingsIdx) {
        renderer.fillRect(sidePad, y, cardWidth, 30, true);
    }
    renderer.drawText(UI_10_FONT_ID, sidePad + 8, y + 5, tr(STR_FLASHCARD_SETTINGS),
                      selectedIndex != settingsIdx);

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT),
                                              tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    renderer.displayBuffer();
}
