#include "FlashcardDeck.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <cstdio>

static constexpr const char* TAG = "FlashcardDeck";

// ---------------------------------------------------------------------------
// FlashcardCard helpers
// ---------------------------------------------------------------------------

// Replace all occurrences of "/n" (the CSV-encoded newline) with '\n'.
std::string FlashcardDeck::decodeNewlines(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '/' && i + 1 < raw.size() && raw[i + 1] == 'n') {
            result += '\n';
            ++i;
        } else {
            result += raw[i];
        }
    }
    return result;
}

std::string FlashcardCard::frontMain() const {
    // Locate first '\' character
    size_t pos = frontContent.find('\\');
    std::string raw = (pos == std::string::npos)
                          ? frontContent
                          : frontContent.substr(0, pos);
    // Trim trailing whitespace
    while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t')) {
        raw.pop_back();
    }
    return FlashcardDeck::decodeNewlines(raw);
}

std::string FlashcardCard::frontHint() const {
    // Find "\ " marker (backslash + space) to locate hint start
    size_t start = frontContent.find("\\ ");
    if (start == std::string::npos) return {};
    start += 2;  // skip past "\ "

    // Find closing " \" (space + backslash)
    size_t end = frontContent.find(" \\", start);
    if (end == std::string::npos) return {};

    std::string hint = frontContent.substr(start, end - start);
    // Trim
    while (!hint.empty() && (hint.front() == ' ' || hint.front() == '\t')) {
        hint.erase(hint.begin());
    }
    while (!hint.empty() && (hint.back() == ' ' || hint.back() == '\t')) {
        hint.pop_back();
    }
    return FlashcardDeck::decodeNewlines(hint);
}

// ---------------------------------------------------------------------------
// CSV parsing
// ---------------------------------------------------------------------------

// Parse one CSV field. Handles quoted fields with "" escaping.
// Advances ptr past the trailing comma (or to end of string).
std::string FlashcardDeck::parseCsvField(const char*& ptr) {
    std::string result;
    if (*ptr == '\0') return result;

    if (*ptr == '"') {
        // Quoted field
        ++ptr;
        while (*ptr != '\0') {
            if (*ptr == '"') {
                ++ptr;
                if (*ptr == '"') {
                    // Escaped double quote
                    result += '"';
                    ++ptr;
                } else {
                    // End of quoted field
                    break;
                }
            } else {
                result += *ptr++;
            }
        }
        // Skip trailing comma
        if (*ptr == ',') ++ptr;
    } else {
        // Unquoted field — read until comma or end of line
        while (*ptr != '\0' && *ptr != ',' && *ptr != '\r' && *ptr != '\n') {
            result += *ptr++;
        }
        if (*ptr == ',') ++ptr;
    }

    return result;
}

// Write a field, quoting it if it contains commas, newlines, or double quotes.
void FlashcardDeck::writeCsvField(std::string& out, const std::string& field) {
    bool needsQuote = (field.find(',') != std::string::npos ||
                       field.find('"') != std::string::npos ||
                       field.find('\n') != std::string::npos ||
                       field.find('\r') != std::string::npos);
    if (!needsQuote) {
        out += field;
        return;
    }
    out += '"';
    for (char c : field) {
        if (c == '"') out += '"';  // escape by doubling
        out += c;
    }
    out += '"';
}

// ---------------------------------------------------------------------------
// FlashcardDeck — load / save
// ---------------------------------------------------------------------------

bool FlashcardDeck::loadFromCsv(const char* path) {
    cards.clear();
    filePath = path;

    String content = Storage.readFile(path);
    if (content.isEmpty()) {
        LOG_ERR(TAG, "Failed to read CSV: %s", path);
        return false;
    }

    const char* ptr = content.c_str();

    // Skip header line
    while (*ptr != '\0' && *ptr != '\n') ++ptr;
    if (*ptr == '\n') ++ptr;

    uint16_t nextId = 1;
    while (*ptr != '\0') {
        // Skip blank lines
        if (*ptr == '\r' || *ptr == '\n') {
            ++ptr;
            continue;
        }

        if (cards.size() >= MAX_CARDS_PER_DECK) {
            LOG_ERR(TAG, "Deck %s exceeds MAX_CARDS_PER_DECK (%u), truncating", path,
                    (unsigned)MAX_CARDS_PER_DECK);
            break;
        }

        FlashcardCard card;

        // card_id
        std::string idStr = parseCsvField(ptr);
        card.id = idStr.empty() ? nextId : (uint16_t)atoi(idStr.c_str());
        nextId = card.id + 1;

        // front_content, back_content
        card.frontContent = parseCsvField(ptr);
        card.backContent = parseCsvField(ptr);

        // sr_due, sr_interval, sr_ease
        std::string srDue = parseCsvField(ptr);
        std::string srInterval = parseCsvField(ptr);
        std::string srEase = parseCsvField(ptr);

        if (!srDue.empty() && !srInterval.empty() && !srEase.empty()) {
            card.srs.dueDate = FlashcardSrs::parseDateStr(srDue.c_str());
            card.srs.interval = (uint16_t)atoi(srInterval.c_str());
            card.srs.ease = (uint16_t)atoi(srEase.c_str());
            // Clamp ease to valid range [130, 400]
            if (card.srs.ease < 130) card.srs.ease = 130;
            if (card.srs.ease > 400) card.srs.ease = 400;
        }
        // else: new card — srs stays at default values

        // Skip to end of line
        while (*ptr != '\0' && *ptr != '\n') ++ptr;
        if (*ptr == '\n') ++ptr;

        cards.push_back(std::move(card));
    }

    // Derive display name from filename
    size_t sep = filePath.rfind('/');
    std::string filename = (sep == std::string::npos) ? filePath : filePath.substr(sep + 1);
    name = fileNameToDisplayName(filename);

    LOG_DBG(TAG, "Loaded %u cards from %s", (unsigned)cards.size(), path);
    return true;
}

bool FlashcardDeck::saveToCsv(const char* path) const {
    // Build CSV in memory
    std::string csv;
    csv.reserve(cards.size() * 60);
    csv += "card_id,front_content,back_content,sr_due,sr_interval,sr_ease\n";

    char dateBuf[12];
    for (const auto& card : cards) {
        csv += std::to_string(card.id);
        csv += ',';
        writeCsvField(csv, card.frontContent);
        csv += ',';
        writeCsvField(csv, card.backContent);
        csv += ',';

        if (card.srs.dueDate != 0) {
            FlashcardSrs::formatDateStr(card.srs.dueDate, dateBuf, sizeof(dateBuf));
            csv += dateBuf;
            csv += ',';
            csv += std::to_string(card.srs.interval);
            csv += ',';
            csv += std::to_string(card.srs.ease);
        } else {
            csv += ",,";  // new card — empty SRS fields
        }
        csv += '\n';
    }

    // Atomic write: .tmp → rename
    std::string tmpPath = std::string(path) + ".tmp";
    if (!Storage.writeFile(tmpPath.c_str(), String(csv.c_str()))) {
        LOG_ERR(TAG, "Failed to write tmp file: %s", tmpPath.c_str());
        return false;
    }

    // Try rename directly (FAT rename replaces destination atomically on most impls).
    // If that fails, fall back to remove-then-rename.
    if (!Storage.rename(tmpPath.c_str(), path)) {
        Storage.remove(path);
        if (!Storage.rename(tmpPath.c_str(), path)) {
            LOG_ERR(TAG, "Failed to rename %s -> %s", tmpPath.c_str(), path);
            return false;
        }
    }

    LOG_DBG(TAG, "Saved %u cards to %s", (unsigned)cards.size(), path);
    return true;
}

bool FlashcardDeck::importCsv(const char* srcPath) {
    if (!Storage.ensureDirectoryExists(FLASHCARD_DIR)) {
        LOG_ERR(TAG, "Cannot create directory: %s", FLASHCARD_DIR);
        return false;
    }

    // Extract filename from source path
    const char* sep = strrchr(srcPath, '/');
    const char* filename = sep ? sep + 1 : srcPath;

    std::string destPath = std::string(FLASHCARD_DIR) + "/" + filename;

    // Read source file content and write to destination
    // Use a chunked read to avoid large heap allocations where possible,
    // but for CSV files within deck size limits String is acceptable.
    String content = Storage.readFile(srcPath);
    if (content.isEmpty()) {
        LOG_ERR(TAG, "importCsv: cannot read source: %s", srcPath);
        return false;
    }

    if (!Storage.writeFile(destPath.c_str(), content)) {
        LOG_ERR(TAG, "importCsv: failed to write: %s", destPath.c_str());
        return false;
    }

    return loadFromCsv(destPath.c_str());
}

// ---------------------------------------------------------------------------
// FlashcardDeck — queries
// ---------------------------------------------------------------------------

DeckStats FlashcardDeck::getStats(uint32_t today) const {
    DeckStats stats;
    stats.totalCards = (uint16_t)cards.size();
    for (const auto& card : cards) {
        if (card.isNew()) {
            ++stats.newCards;
        } else if (card.isDue(today)) {
            ++stats.dueCards;
        }
    }
    return stats;
}

std::vector<size_t> FlashcardDeck::buildReviewQueue(uint32_t today, uint8_t newPerDay,
                                                      uint16_t maxReview) const {
    std::vector<size_t> due;
    std::vector<size_t> newCards;

    for (size_t i = 0; i < cards.size(); ++i) {
        const FlashcardCard& c = cards[i];
        if (c.isNew()) {
            newCards.push_back(i);
        } else if (c.isDue(today)) {
            due.push_back(i);
        }
    }

    std::vector<size_t> queue;
    queue.reserve(std::min((size_t)maxReview, due.size() + newCards.size()));

    // Due cards first
    for (size_t idx : due) {
        if (queue.size() >= maxReview) break;
        queue.push_back(idx);
    }

    // Then new cards, capped at newPerDay
    uint8_t newAdded = 0;
    for (size_t idx : newCards) {
        if (queue.size() >= maxReview) break;
        if (newAdded >= newPerDay) break;
        queue.push_back(idx);
        ++newAdded;
    }

    return queue;
}

bool FlashcardDeck::updateCard(size_t index, const SrsState& newState) {
    if (index >= cards.size()) {
        LOG_ERR(TAG, "updateCard: index %u out of range (%u)", (unsigned)index,
                (unsigned)cards.size());
        return false;
    }
    cards[index].srs = newState;
    return true;
}

// ---------------------------------------------------------------------------
// FlashcardDeck — static helpers
// ---------------------------------------------------------------------------

std::vector<std::string> FlashcardDeck::listDecks() {
    std::vector<std::string> result;

    HalFile dir = Storage.open(FLASHCARD_DIR);
    if (!dir) {
        LOG_DBG(TAG, "listDecks: directory not found: %s", FLASHCARD_DIR);
        return result;
    }

    dir.rewindDirectory();
    HalFile entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            char nameBuf[128];
            entry.getName(nameBuf, sizeof(nameBuf));
            std::string fname(nameBuf);
            if (fname.size() > 4 &&
                fname.substr(fname.size() - 4) == ".csv") {
                result.push_back(std::string(FLASHCARD_DIR) + "/" + fname);
            }
        }
        entry = dir.openNextFile();
    }

    // Sort alphabetically for consistent ordering
    std::sort(result.begin(), result.end());
    return result;
}

std::string FlashcardDeck::fileNameToDisplayName(const std::string& filename) {
    // Strip .csv extension
    std::string name = filename;
    if (name.size() > 4 && name.substr(name.size() - 4) == ".csv") {
        name = name.substr(0, name.size() - 4);
    }
    // Replace underscores with spaces
    for (char& c : name) {
        if (c == '_') c = ' ';
    }
    return name;
}
