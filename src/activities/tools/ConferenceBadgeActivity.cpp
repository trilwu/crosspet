#include "ConferenceBadgeActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstring>

#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/QrUtils.h"

namespace {
constexpr const char* BADGE_FILE = "/badge.txt";
constexpr size_t MAX_BADGE_SIZE = 512;
}  // namespace

// ---- File I/O -----------------------------------------------------------

void ConferenceBadgeActivity::loadFromFile() {
  badgeName.clear();
  badgeTitle.clear();
  qrData.clear();
  fileLoaded = false;

  if (!Storage.exists(BADGE_FILE)) return;

  char buffer[MAX_BADGE_SIZE];
  size_t bytesRead = Storage.readFileToBuffer(BADGE_FILE, buffer, MAX_BADGE_SIZE);
  if (bytesRead == 0) return;

  fileLoaded = true;
  const char* ptr = buffer;
  const char* end = buffer + bytesRead;
  int lineNum = 0;

  while (ptr < end && lineNum < 3) {
    const char* lineEnd = ptr;
    while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') lineEnd++;
    std::string line(ptr, lineEnd - ptr);
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();

    switch (lineNum) {
      case 0: badgeName = std::move(line); break;
      case 1: badgeTitle = std::move(line); break;
      case 2: qrData = std::move(line); break;
    }

    ptr = lineEnd;
    if (ptr < end && *ptr == '\r') ptr++;
    if (ptr < end && *ptr == '\n') ptr++;
    lineNum++;
  }
}

void ConferenceBadgeActivity::saveToFile() {
  // Write "name\ntitle\nqr\n" — fields may be empty but order is fixed
  std::string content = badgeName + "\n" + badgeTitle + "\n" + qrData + "\n";
  Storage.writeFile(BADGE_FILE, String(content.c_str()));
  fileLoaded = true;
}

// ---- Lifecycle ----------------------------------------------------------

void ConferenceBadgeActivity::onEnter() {
  Activity::onEnter();
  mode = ViewMode::VIEWING;
  editSelected = 0;
  loadFromFile();
  requestUpdate();
}

void ConferenceBadgeActivity::loop() {
  if (mode == ViewMode::VIEWING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    // Confirm enters edit mode
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      mode = ViewMode::EDITING;
      editSelected = 0;
      requestUpdate();
    }
  } else {
    // EDITING mode
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      mode = ViewMode::VIEWING;
      requestUpdate();
      return;
    }

    buttonNavigator.onNext([this] {
      editSelected = (editSelected + 1) % EDIT_FIELD_COUNT;
      requestUpdate();
    });
    buttonNavigator.onPrevious([this] {
      editSelected = (editSelected + EDIT_FIELD_COUNT - 1) % EDIT_FIELD_COUNT;
      requestUpdate();
    });

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      openFieldEditor(editSelected);
    }
  }
}

// ---- Field editor -------------------------------------------------------

void ConferenceBadgeActivity::openFieldEditor(const int field) {
  const char* title = nullptr;
  std::string* target = nullptr;
  size_t maxLen = 60;

  switch (field) {
    case FIELD_NAME:  title = tr(STR_BADGE_NAME);    target = &badgeName;  maxLen = 60;  break;
    case FIELD_TITLE: title = tr(STR_BADGE_ROLE);    target = &badgeTitle; maxLen = 80;  break;
    case FIELD_QR:    title = tr(STR_BADGE_QR_DATA); target = &qrData;     maxLen = 256; break;
    default: return;
  }

  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, title, *target, maxLen, InputType::Text),
      [this, target](const ActivityResult& result) {
        if (!result.isCancelled) {
          *target = std::get<KeyboardResult>(result.data).text;
          saveToFile();
        }
        mode = ViewMode::EDITING;
        requestUpdate();
      });
}

// ---- Render -------------------------------------------------------------

void ConferenceBadgeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (mode == ViewMode::VIEWING) {
    renderViewing();
  } else {
    renderEditing();
  }
  renderer.displayBuffer();
}

void ConferenceBadgeActivity::renderViewing() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  if (!fileLoaded || badgeName.empty()) {
    // No badge configured — prompt to set one up
    const int lh = renderer.getLineHeight(UI_10_FONT_ID);
    const int centerY = (pageHeight - lh * 2) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_PLACE_BADGE_TXT));
    renderer.drawCenteredText(SMALL_FONT_ID, centerY + lh + 6, tr(STR_BADGE_SETUP));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_BADGE_EDIT), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    return;
  }

  const int nameHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int titleHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int spacing = 20;

  const int qrAvailW = pageWidth - 40;
  const int qrAvailH = pageHeight / 3;

  int totalHeight = nameHeight;
  if (!badgeTitle.empty()) totalHeight += spacing + titleHeight;
  if (!qrData.empty()) totalHeight += spacing + qrAvailH;

  int y = (pageHeight - totalHeight) / 2;

  renderer.drawCenteredText(UI_12_FONT_ID, y, badgeName.c_str(), true, EpdFontFamily::BOLD);
  y += nameHeight + spacing;

  if (!badgeTitle.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, badgeTitle.c_str());
    y += titleHeight + spacing;
  }

  if (!qrData.empty()) {
    const Rect qrBounds(20, y, qrAvailW, qrAvailH);
    QrUtils::drawQrCode(renderer, qrBounds, qrData);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_BADGE_EDIT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ConferenceBadgeActivity::renderEditing() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_BADGE_SETUP));

  // Show 3 editable fields as a simple list
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int rowH = lh + 12;
  const int sideMargin = metrics.contentSidePadding;

  const char* fieldLabels[EDIT_FIELD_COUNT] = {tr(STR_BADGE_NAME), tr(STR_BADGE_ROLE), tr(STR_BADGE_QR_DATA)};
  const std::string* fieldValues[EDIT_FIELD_COUNT] = {&badgeName, &badgeTitle, &qrData};

  for (int i = 0; i < EDIT_FIELD_COUNT; i++) {
    const int y = contentTop + i * (rowH + metrics.verticalSpacing);
    const bool selected = (i == editSelected);

    if (selected) {
      renderer.fillRect(sideMargin - 4, y - 2, pageWidth - sideMargin * 2 + 8, rowH, false);
    }

    // Label
    renderer.drawText(SMALL_FONT_ID, sideMargin, y, fieldLabels[i]);
    // Value (truncated preview)
    const std::string& val = *fieldValues[i];
    char preview[48];
    if (val.empty()) {
      snprintf(preview, sizeof(preview), "—");
    } else {
      snprintf(preview, sizeof(preview), "%.*s", 40, val.c_str());
      if (val.size() > 40) strncat(preview, "...", sizeof(preview) - strlen(preview) - 1);
    }
    renderer.drawText(UI_10_FONT_ID, sideMargin, y + renderer.getLineHeight(SMALL_FONT_ID) + 2, preview);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
