#include "FontSelectActivity.h"

#include <FontManager.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kBuiltinReaderFontCount = 3;
constexpr CrossPointSettings::FONT_FAMILY kBuiltinReaderFonts[kBuiltinReaderFontCount] = {
    CrossPointSettings::BOOKERLY, CrossPointSettings::LEXEND, CrossPointSettings::BOKERLAM};
constexpr StrId kBuiltinReaderFontLabels[kBuiltinReaderFontCount] = {StrId::STR_BOOKERLY, StrId::STR_LEXEND, StrId::STR_BOKERLAM};
}  // namespace

void FontSelectActivity::onEnter() {
  Activity::onEnter();

  FontMgr.scanFonts();

  if (mode == SelectMode::Reader) {
    totalItems = kBuiltinReaderFontCount + FontMgr.getFontCount();
    const int currentExternal = FontMgr.getSelectedIndex();
    if (currentExternal >= 0) {
      selectedIndex = kBuiltinReaderFontCount + currentExternal;
    } else {
      const int familyIndex = static_cast<int>(SETTINGS.fontFamily);
      selectedIndex = (familyIndex < kBuiltinReaderFontCount) ? familyIndex : 0;
    }
  } else {
    totalItems = 1 + FontMgr.getFontCount();
    const int currentFont = FontMgr.getUiSelectedIndex();
    selectedIndex = (currentFont < 0) ? 0 : currentFont + 1;
  }

  requestUpdate();
}

void FontSelectActivity::onExit() { Activity::onExit(); }

void FontSelectActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, totalItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, totalItems);
    requestUpdate();
  });
}

void FontSelectActivity::handleSelection() {
  if (mode == SelectMode::Reader) {
    if (selectedIndex < kBuiltinReaderFontCount) {
      FontMgr.selectFont(-1);
      SETTINGS.fontFamily = static_cast<uint8_t>(kBuiltinReaderFonts[selectedIndex]);
      SETTINGS.saveToFile();
    } else {
      const int externalIndex = selectedIndex - kBuiltinReaderFontCount;
      const FontInfo* info = FontMgr.getFontInfo(externalIndex);
      if (info && !ExternalFont::canFitGlyph(info->width, info->height)) {
        return;  // Font glyph too large for cache
      }
      FontMgr.selectFont(externalIndex);
    }
  } else {
    if (selectedIndex == 0) {
      FontMgr.selectUiFont(-1);
    } else {
      const int externalIndex = selectedIndex - 1;
      const FontInfo* info = FontMgr.getFontInfo(externalIndex);
      if (info && !ExternalFont::canFitGlyph(info->width, info->height)) {
        return;
      }
      FontMgr.selectUiFont(externalIndex);
    }
  }

  finish();
}

void FontSelectActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const char* title = (mode == SelectMode::Reader) ? tr(STR_EXT_READER_FONT) : tr(STR_EXT_UI_FONT);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title);

  // Determine current selection marker position
  int currentIndex = 0;
  if (mode == SelectMode::Reader) {
    const int currentExternal = FontMgr.getSelectedIndex();
    if (currentExternal >= 0) {
      currentIndex = kBuiltinReaderFontCount + currentExternal;
    } else {
      const int familyIndex = static_cast<int>(SETTINGS.fontFamily);
      currentIndex = (familyIndex < kBuiltinReaderFontCount) ? familyIndex : 0;
    }
  } else {
    const int currentFont = FontMgr.getUiSelectedIndex();
    currentIndex = (currentFont < 0) ? 0 : currentFont + 1;
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, totalItems, selectedIndex,
      [this](int i) -> std::string {
        if (mode == SelectMode::Reader) {
          if (i < kBuiltinReaderFontCount) return std::string(I18N.get(kBuiltinReaderFontLabels[i]));
          const FontInfo* info = FontMgr.getFontInfo(i - kBuiltinReaderFontCount);
          if (info) {
            const bool loadable = ExternalFont::canFitGlyph(info->width, info->height);
            char label[80];
            snprintf(label, sizeof(label), "%s (%dpt)%s", info->name, info->size, loadable ? "" : " [!]");
            return std::string(label);
          }
        } else {
          if (i == 0) return std::string(tr(STR_BUILTIN_DISABLED));
          const FontInfo* info = FontMgr.getFontInfo(i - 1);
          if (info) {
            const bool loadable = ExternalFont::canFitGlyph(info->width, info->height);
            char label[80];
            snprintf(label, sizeof(label), "%s (%dpt)%s", info->name, info->size, loadable ? "" : " [!]");
            return std::string(label);
          }
        }
        return "";
      },
      nullptr, nullptr,
      [currentIndex](int i) -> std::string {
        return (i == currentIndex) ? std::string(tr(STR_ON_MARKER)) : std::string("");
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
