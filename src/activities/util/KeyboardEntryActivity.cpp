#include "KeyboardEntryActivity.h"

#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

// Keyboard layouts - lowercase
const char* const KeyboardEntryActivity::keyboard[NUM_ROWS] = {
    "`1234567890-=", "qwertyuiop[]\\", "asdfghjkl;'", "zxcvbnm,./",
    "^  _____<OK"  // ^ = shift, _ = space, < = backspace, OK = done
};

// Keyboard layouts - uppercase/symbols
const char* const KeyboardEntryActivity::keyboardShift[NUM_ROWS] = {"~!@#$%^&*()_+", "QWERTYUIOP{}|", "ASDFGHJKL:\"",
                                                                    "ZXCVBNM<>?", "SPECIAL ROW"};

// Shift state strings
const char* const KeyboardEntryActivity::shiftString[3] = {"shift", "SHIFT", "LOCK"};

void KeyboardEntryActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void KeyboardEntryActivity::onExit() { Activity::onExit(); }

int KeyboardEntryActivity::getRowLength(const int row) const {
  if (row < 0 || row >= NUM_ROWS) return 0;

  switch (row) {
    case 0: return 13;  // `1234567890-=
    case 1: return 13;  // qwertyuiop[]backslash
    case 2: return 11;  // asdfghjkl;'
    case 3: return 10;  // zxcvbnm,./
    case 4: return 11;  // shift(2) space(5) backspace(2) OK(2)
    default: return 0;
  }
}

char KeyboardEntryActivity::getSelectedChar() const {
  const char* const* layout = shiftState ? keyboardShift : keyboard;
  if (selectedRow < 0 || selectedRow >= NUM_ROWS) return '\0';
  if (selectedCol < 0 || selectedCol >= getRowLength(selectedRow)) return '\0';
  return layout[selectedRow][selectedCol];
}

// Returns the secondary (diacritic) character for the currently-selected key,
// or '\0' if none is defined. Uses explicit lookup for correctness.
std::string KeyboardEntryActivity::getSelectedSecondaryChar() const {
  if (selectedRow >= SPECIAL_ROW) return {};
  return getSecondaryCharFor(keyboard[selectedRow][selectedCol]);
}

std::string KeyboardEntryActivity::getSecondaryCharFor(char primary) {
  // Vietnamese diacritics (UTF-8 multi-byte)
  switch (primary) {
    case 'a': return "\xC4\x83";  // ă
    case 'A': return "\xC4\x82";  // Ă
    case 'e': return "\xC3\xAA";  // ê
    case 'E': return "\xC3\x8A";  // Ê
    case 'o': return "\xC3\xB4";  // ô
    case 'O': return "\xC3\x94";  // Ô
    case 'u': return "\xC6\xB0";  // ư
    case 'U': return "\xC6\xAF";  // Ư
    case 'd': return "\xC4\x91";  // đ
    case 'D': return "\xC4\x90";  // Đ
    // Common Latin secondary chars
    case 'c': return "\xC3\xA7";  // ç
    case 'n': return "\xC3\xB1";  // ñ
    case 's': return "\xC3\x9F";  // ß
    default:  return {};
  }
}

// Insert secondary char for current selection. Returns true if inserted.
bool KeyboardEntryActivity::insertSecondaryChar() {
  const std::string sec = getSelectedSecondaryChar();
  if (sec.empty()) return false;
  if (maxLength == 0 || text.length() < maxLength) {
    text += sec;
    if (shiftState == 1) shiftState = 0;
  }
  return true;
}

bool KeyboardEntryActivity::handleKeyPress() {
  // Handle special row (bottom row with shift, space, backspace, done)
  if (selectedRow == SPECIAL_ROW) {
    if (selectedCol >= SHIFT_COL && selectedCol < SPACE_COL) {
      // Shift cycle: lower → upper → caps lock → lower
      shiftState = (shiftState + 1) % 3;
      return true;
    }

    if (selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL) {
      if (maxLength == 0 || text.length() < maxLength) {
        text += ' ';
      }
      return true;
    }

    if (selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL) {
      if (!text.empty()) {
        text.pop_back();
      }
      return true;
    }

    if (selectedCol >= DONE_COL) {
      onComplete(text);
      return false;
    }
  }

  // Regular character
  const char c = getSelectedChar();
  if (c == '\0') return true;

  if (maxLength == 0 || text.length() < maxLength) {
    text += c;
    // Auto-disable shift after typing a character (not in caps lock mode)
    if (shiftState == 1) shiftState = 0;
  }

  return true;
}

KeyboardEntryActivity::KeyboardLayout KeyboardEntryActivity::computeLayout() const {
  const auto screenWidth = renderer.getScreenWidth();
  const auto screenHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  // Compute key size dynamically from screen width.
  // KEYS_PER_ROW = 13 is the widest row. Keep 1px spacing between keys.
  // Leave 2px total side margin (1px each side) — keys are tight on e-ink.
  const int totalSpacing = (KEYS_PER_ROW - 1);   // 12 gaps of 1px
  const int sidePad = 2;
  const int keyWidth = (screenWidth - sidePad * 2 - totalSpacing) / KEYS_PER_ROW;
  const int keySpacing = 1;
  const int actualRowWidth = KEYS_PER_ROW * keyWidth + (KEYS_PER_ROW - 1) * keySpacing;
  const int leftMargin = (screenWidth - actualRowWidth) / 2;

  // Key height: fit NUM_ROWS rows above button hints, with 1px row spacing.
  const int availableHeight = screenHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  // Reserve top area for header + input field (estimated 120px)
  const int keyboardAreaHeight = availableHeight - 120;
  const int rowSpacing = 1;
  const int keyHeight = (keyboardAreaHeight - (NUM_ROWS - 1) * rowSpacing) / NUM_ROWS;

  // Bottom-align keyboard above button hints
  const int totalKbHeight = NUM_ROWS * keyHeight + (NUM_ROWS - 1) * rowSpacing;
  const int startY = availableHeight - totalKbHeight;

  return {keyWidth, keyHeight, keySpacing, leftMargin, startY};
}

void KeyboardEntryActivity::loop() {
  // --- Long-press detection for Confirm button ---
  if (mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
    if (!confirmHeld_) {
      confirmHeld_ = true;
      longPressHandled_ = false;
    } else if (!longPressHandled_ && mappedInput.getHeldTime() >= LONG_PRESS_MS) {
      // Long press fired: insert secondary char
      if (selectedRow != SPECIAL_ROW) {
        if (insertSecondaryChar()) {
          requestUpdate();
        }
      }
      longPressHandled_ = true;
    }
  } else {
    if (confirmHeld_) {
      // Button just released — if we didn't handle as long-press, do normal press
      if (!longPressHandled_) {
        if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
          // Normal short press — handled below via wasPressed path, skip here
        }
      }
      confirmHeld_ = false;
      longPressHandled_ = false;
    }
  }

  // Navigation
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    selectedRow = ButtonNavigator::previousIndex(selectedRow, NUM_ROWS);
    const int maxCol = getRowLength(selectedRow) - 1;
    if (selectedCol > maxCol) selectedCol = maxCol;
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] {
    selectedRow = ButtonNavigator::nextIndex(selectedRow, NUM_ROWS);
    const int maxCol = getRowLength(selectedRow) - 1;
    if (selectedCol > maxCol) selectedCol = maxCol;
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] {
    const int maxCol = getRowLength(selectedRow) - 1;

    if (selectedRow == SPECIAL_ROW) {
      if (selectedCol >= SHIFT_COL && selectedCol < SPACE_COL) {
        selectedCol = maxCol;
      } else if (selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL) {
        selectedCol = SHIFT_COL;
      } else if (selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL) {
        selectedCol = SPACE_COL;
      } else if (selectedCol >= DONE_COL) {
        selectedCol = BACKSPACE_COL;
      }
    } else {
      selectedCol = ButtonNavigator::previousIndex(selectedCol, maxCol + 1);
    }
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] {
    const int maxCol = getRowLength(selectedRow) - 1;

    if (selectedRow == SPECIAL_ROW) {
      if (selectedCol >= SHIFT_COL && selectedCol < SPACE_COL) {
        selectedCol = SPACE_COL;
      } else if (selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL) {
        selectedCol = BACKSPACE_COL;
      } else if (selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL) {
        selectedCol = DONE_COL;
      } else if (selectedCol >= DONE_COL) {
        // On OK key + Right: toggle password visibility (when in password mode)
        if (isPassword) {
          showPassword_ = !showPassword_;
          requestUpdate();
        }
        selectedCol = SHIFT_COL;
      }
    } else {
      selectedCol = ButtonNavigator::nextIndex(selectedCol, maxCol + 1);
    }
    requestUpdate();
  });

  // Short press: normal key selection (only if not a long-press)
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    // wasPressed fires on the first frame the button goes down.
    // Long-press: we wait — actual insert happens in the held-time check above.
    // Short-press: handled on release to avoid double-fire with long-press path.
    // (wasPressed just starts the hold timer; handleKeyPress runs on wasReleased)
  }

  // Execute key on release (avoids firing both short and long press)
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!longPressHandled_) {
      // Short press: normal action
      if (handleKeyPress()) {
        requestUpdate();
      }
    }
    confirmHeld_ = false;
    longPressHandled_ = false;
  }

  // Cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onCancel();
  }
}

void KeyboardEntryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title.c_str());

  // --- Input field ---
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int inputStartY =
      metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + metrics.verticalSpacing * 4;
  int inputHeight = 0;

  // Build display text: password masking or plain
  std::string displayText;
  if (isPassword && !showPassword_) {
    displayText = std::string(text.length(), '\xB7');  // middle dot bullet (CP1252 · U+00B7)
  } else {
    displayText = text;
  }
  displayText += "_";  // cursor

  // Word-wrap input text across multiple lines
  int lineStartIdx = 0;
  int lineEndIdx = static_cast<int>(displayText.length());
  int textWidth = 0;
  while (true) {
    std::string lineText = displayText.substr(lineStartIdx, lineEndIdx - lineStartIdx);
    textWidth = renderer.getTextWidth(UI_12_FONT_ID, lineText.c_str());
    if (textWidth <= pageWidth - 2 * metrics.contentSidePadding) {
      if (metrics.keyboardCenteredText) {
        renderer.drawCenteredText(UI_12_FONT_ID, inputStartY + inputHeight, lineText.c_str());
      } else {
        renderer.drawText(UI_12_FONT_ID, metrics.contentSidePadding, inputStartY + inputHeight, lineText.c_str());
      }
      if (lineEndIdx == static_cast<int>(displayText.length())) break;
      inputHeight += lineHeight;
      lineStartIdx = lineEndIdx;
      lineEndIdx = static_cast<int>(displayText.length());
    } else {
      lineEndIdx -= 1;
    }
  }

  GUI.drawTextField(renderer, Rect{0, inputStartY, pageWidth, inputHeight}, textWidth);

  // --- Keyboard ---
  const auto layout = computeLayout();
  const int keyWidth = layout.keyWidth;
  const int keyHeight = layout.keyHeight;
  const int keySpacing = layout.keySpacing;
  const int leftMargin = layout.leftMargin;
  const int keyboardStartY = layout.startY;

  const char* const* charLayout = shiftState ? keyboardShift : keyboard;

  for (int row = 0; row < NUM_ROWS; row++) {
    const int rowY = keyboardStartY + row * (keyHeight + keySpacing);
    const int startX = leftMargin;

    if (row == SPECIAL_ROW) {
      int currentX = startX;

      // SHIFT key (spans 2 logical columns)
      const bool shiftSelected = (selectedRow == SPECIAL_ROW && selectedCol >= SHIFT_COL && selectedCol < SPACE_COL);
      const int shiftXWidth = (SPACE_COL - SHIFT_COL) * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, shiftXWidth, keyHeight}, shiftString[shiftState],
                          shiftSelected);
      currentX += shiftXWidth;

      // SPACE bar (spans 5 logical columns)
      const bool spaceSelected =
          (selectedRow == SPECIAL_ROW && selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL);
      const int spaceXWidth = (BACKSPACE_COL - SPACE_COL) * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, spaceXWidth, keyHeight}, "_____", spaceSelected);
      currentX += spaceXWidth;

      // BACKSPACE key (spans 2 logical columns)
      const bool bsSelected = (selectedRow == SPECIAL_ROW && selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL);
      const int backspaceXWidth = (DONE_COL - BACKSPACE_COL) * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, backspaceXWidth, keyHeight}, "<-", bsSelected);
      currentX += backspaceXWidth;

      // OK / DONE button (spans 2 logical columns)
      // Show "Show" hint when in password mode
      const bool okSelected = (selectedRow == SPECIAL_ROW && selectedCol >= DONE_COL);
      const int okXWidth = (getRowLength(row) - DONE_COL) * (keyWidth + keySpacing);
      const char* okLabel = (isPassword && okSelected) ? tr(STR_SHOW_PASSWORD) : tr(STR_OK_BUTTON);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, okXWidth, keyHeight}, okLabel, okSelected);
    } else {
      for (int col = 0; col < getRowLength(row); col++) {
        const char c = charLayout[row][col];
        std::string keyLabel(1, c);

        const int keyX = startX + col * (keyWidth + keySpacing);
        const bool isSelected = (row == selectedRow && col == selectedCol);

        // Draw primary key label
        GUI.drawKeyboardKey(renderer, Rect{keyX, rowY, keyWidth, keyHeight}, keyLabel.c_str(), isSelected);

        // Draw secondary char as small superscript in top-right of key
        if (row >= 1 && row <= 3) {
          const std::string sec = getSecondaryCharFor(keyboard[row][col]);
          if (!sec.empty()) {
            const int secX = keyX + keyWidth - renderer.getTextWidth(SMALL_FONT_ID, sec.c_str()) - 1;
            const int secY = rowY;
            renderer.drawText(SMALL_FONT_ID, secX, secY, sec.c_str());
          }
        }
      }
    }
  }

  // Password visibility indicator below keyboard
  if (isPassword) {
    const char* hint = showPassword_ ? tr(STR_SHOW_PASSWORD) : "*****";
    const int hintY = keyboardStartY + NUM_ROWS * (keyHeight + keySpacing) + 2;
    renderer.drawCenteredText(SMALL_FONT_ID, hintY, hint);
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, ">", "<");

  renderer.displayBuffer();
}

void KeyboardEntryActivity::onComplete(std::string text) {
  setResult(KeyboardResult{std::move(text)});
  finish();
}

void KeyboardEntryActivity::onCancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
