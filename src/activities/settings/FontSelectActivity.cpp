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

  phase = Phase::FontList;
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
  if (phase == Phase::RenderMode) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      phase = Phase::FontList;
      requestUpdate();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      handleRenderModeSelection();
      return;
    }
    buttonNavigator.onNextRelease([this] {
      renderModeIndex = (renderModeIndex + 1) % 2;
      requestUpdate();
    });
    buttonNavigator.onPreviousRelease([this] {
      renderModeIndex = (renderModeIndex + 1) % 2;
      requestUpdate();
    });
    return;
  }

  // Phase::FontList
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
      finish();
    } else {
      const int externalIndex = selectedIndex - kBuiltinReaderFontCount;
      const FontInfo* info = FontMgr.getFontInfo(externalIndex);
      if (info && !ExternalFont::canFitGlyph(info->width, info->height)) {
        return;  // Font glyph too large for cache
      }
      // Enter render mode sub-screen
      pendingExternalIndex = externalIndex;
      renderModeIndex = FontMgr.isExternalPrimary() ? 0 : 1;
      phase = Phase::RenderMode;
      requestUpdate();
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
    finish();
  }
}

void FontSelectActivity::handleRenderModeSelection() {
  if (pendingExternalIndex < 0) return;
  FontMgr.selectFont(pendingExternalIndex);
  FontMgr.setExternalPrimary(renderModeIndex == 0);
  finish();
}

void FontSelectActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  if (phase == Phase::RenderMode) {
    renderModeScreen(pageWidth, pageHeight);
  } else {
    renderFontList(pageWidth, pageHeight);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void FontSelectActivity::renderFontList(int pageWidth, int pageHeight) {
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
}

void FontSelectActivity::renderModeScreen(int pageWidth, int pageHeight) {
  const auto& metrics = UITheme::getInstance().getMetrics();

  // Header: font name
  const FontInfo* info = FontMgr.getFontInfo(pendingExternalIndex);
  char headerBuf[64];
  if (info) {
    snprintf(headerBuf, sizeof(headerBuf), "%s (%dpt)", info->name, info->size);
  } else {
    snprintf(headerBuf, sizeof(headerBuf), "External Font");
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, headerBuf);

  // Current mode marker
  const int currentMode = FontMgr.isExternalPrimary() ? 0 : 1;

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, 2, renderModeIndex,
      [](int i) -> std::string {
        if (i == 0) return std::string(tr(STR_EXT_FONT_PRIMARY));
        return std::string(tr(STR_EXT_FONT_SUPPLEMENT));
      },
      [](int i) -> std::string {
        if (i == 0) return std::string(tr(STR_EXT_FONT_PRIMARY_DESC));
        return std::string(tr(STR_EXT_FONT_SUPPLEMENT_DESC));
      },
      nullptr,
      [currentMode](int i) -> std::string {
        return (i == currentMode) ? std::string(tr(STR_ON_MARKER)) : std::string("");
      });
}
