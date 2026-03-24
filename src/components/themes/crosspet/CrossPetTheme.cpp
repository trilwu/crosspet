#include "CrossPetTheme.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "components/icons/book24.h"
#include "components/icons/file24.h"
#include "components/icons/folder24.h"
#include "components/icons/image24.h"
#include "components/icons/text24.h"
#include "components/icons/book.h"
#include "components/icons/folder.h"
#include "components/icons/hotspot.h"
#include "components/icons/library.h"
#include "components/icons/recent.h"
#include "components/icons/settings2.h"
#include "components/icons/tools.h"
#include "components/icons/transfer.h"
#include "components/icons/wifi.h"
#include "fontIds.h"

// Internal constants for CrossPet design language
namespace {

const uint8_t* cpIconForName(UIIcon icon, int size) {
  if (size == 24) {
    switch (icon) {
      case UIIcon::Folder: return Folder24Icon;
      case UIIcon::Text:   return Text24Icon;
      case UIIcon::Image:  return Image24Icon;
      case UIIcon::Book:   return Book24Icon;
      case UIIcon::File:   return File24Icon;
      default: return nullptr;
    }
  } else if (size == 32) {
    switch (icon) {
      case UIIcon::Folder:   return FolderIcon;
      case UIIcon::Book:     return BookIcon;
      case UIIcon::Recent:   return RecentIcon;
      case UIIcon::Settings: return Settings2Icon;
      case UIIcon::Transfer: return TransferIcon;
      case UIIcon::Library:  return LibraryIcon;
      case UIIcon::Wifi:     return WifiIcon;
      case UIIcon::Hotspot:  return HotspotIcon;
      case UIIcon::Tools:    return ToolsIcon;
      default: return nullptr;
    }
  }
  return nullptr;
}
constexpr int kCardRadius = 12;
constexpr int kSelectionRadius = 8;
constexpr int kHPad = CrossPetMetrics::rowHPad;
constexpr int kMaxValueWidth = 200;
constexpr int kPopupMarginX = 16;
constexpr int kPopupMarginY = 12;
// Button hints geometry: 4 positions in portrait (58, 146, 254, 342)
constexpr int kBtnPositions[] = {58, 146, 254, 342};
constexpr int kBtnWidth = 80;
constexpr int kBtnHeight = 40;     // same as buttonHintsHeight
constexpr int kShapeSize = 10;     // triangle/circle half-size
}  // namespace

// ── Header ────────────────────────────────────────────────────────────────────

void CrossPetTheme::drawHeader(const GfxRenderer& renderer, Rect rect, const char* title,
                               const char* /*subtitle*/) const {
  // Clear area
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);

  // Battery icon on the right
  const bool showPct =
      SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
  const int battX = rect.x + rect.width - 12 - CrossPetMetrics::values.batteryWidth;
  drawBatteryRight(renderer,
                   Rect{battX, rect.y + 5, CrossPetMetrics::values.batteryWidth,
                        CrossPetMetrics::values.batteryHeight},
                   showPct);

  // Centered title in UI_12 bold
  if (title) {
    const int maxTitleW = rect.width - CrossPetMetrics::values.contentSidePadding * 2;
    auto truncated = renderer.truncatedText(UI_12_FONT_ID, title, maxTitleW, EpdFontFamily::BOLD);
    const int titleW = renderer.getTextWidth(UI_12_FONT_ID, truncated.c_str(), EpdFontFamily::BOLD);
    const int titleX = rect.x + (rect.width - titleW) / 2;
    const int titleY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    renderer.drawText(UI_12_FONT_ID, titleX, titleY, truncated.c_str(), true, EpdFontFamily::BOLD);
  }

  // Thin 1px divider at bottom
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

// ── Sub-header ────────────────────────────────────────────────────────────────

void CrossPetTheme::drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                                  const char* rightLabel) const {
  int currentX = rect.x + CrossPetMetrics::values.contentSidePadding;
  int rightSpace = CrossPetMetrics::values.contentSidePadding;

  if (rightLabel) {
    auto truncRight = renderer.truncatedText(SMALL_FONT_ID, rightLabel, kMaxValueWidth, EpdFontFamily::REGULAR);
    int rightW = renderer.getTextWidth(SMALL_FONT_ID, truncRight.c_str());
    renderer.drawText(SMALL_FONT_ID,
                      rect.x + rect.width - CrossPetMetrics::values.contentSidePadding - rightW,
                      rect.y + 7, truncRight.c_str());
    rightSpace += rightW + kHPad;
  }

  // Small caps style: SMALL_FONT, no bottom underline (mockup design)
  auto truncLabel = renderer.truncatedText(SMALL_FONT_ID, label,
                                           rect.width - CrossPetMetrics::values.contentSidePadding - rightSpace,
                                           EpdFontFamily::REGULAR);
  renderer.drawText(SMALL_FONT_ID, currentX, rect.y + 8, truncLabel.c_str(), true, EpdFontFamily::REGULAR);
  // No bottom line — just the label (mockup style)
}

// ── Tab bar ───────────────────────────────────────────────────────────────────

void CrossPetTheme::drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                               bool selected) const {
  const int availW = rect.width - 2 * CrossPetMetrics::values.contentSidePadding;
  const int tabPad = 2 * kHPad;

  int totalW = 0;
  for (const auto& tab : tabs) {
    totalW += renderer.getTextWidth(UI_10_FONT_ID, tab.label, EpdFontFamily::REGULAR) + tabPad;
  }
  totalW += (int)(tabs.size() - 1) * CrossPetMetrics::values.tabSpacing;
  const int fontId = (totalW > availW) ? SMALL_FONT_ID : UI_10_FONT_ID;

  int curX = rect.x + CrossPetMetrics::values.contentSidePadding;

  for (const auto& tab : tabs) {
    const int textW = renderer.getTextWidth(fontId, tab.label, EpdFontFamily::REGULAR);
    const int tabW = textW + tabPad;

    if (tab.selected) {
      // Bold + underline for selected tab (focused = thicker underline, no fill)
      const int lineThickness = selected ? 3 : 2;
      renderer.drawLine(curX, rect.y + rect.height - 3, curX + tabW, rect.y + rect.height - 3, lineThickness, true);
    }

    // Bold style for selected tab, regular otherwise
    const EpdFontFamily::Style style = tab.selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    renderer.drawText(fontId, curX + kHPad, rect.y + 6, tab.label, true, style);

    curX += tabW + CrossPetMetrics::values.tabSpacing;
  }

  // Thin 1px bottom line
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

// ── List ──────────────────────────────────────────────────────────────────────

void CrossPetTheme::drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                             const std::function<std::string(int index)>& rowTitle,
                             const std::function<std::string(int index)>& rowSubtitle,
                             const std::function<UIIcon(int index)>& rowIcon,
                             const std::function<std::string(int index)>& rowValue,
                             bool highlightValue) const {
  const int rowH = (rowSubtitle != nullptr) ? CrossPetMetrics::values.listWithSubtitleRowHeight
                                            : CrossPetMetrics::values.listRowHeight;
  const int pageItems = rect.height / rowH;
  const int totalPages = (itemCount + pageItems - 1) / pageItems;

  // Scroll bar
  if (totalPages > 1) {
    const int sbH = (rect.height * pageItems) / itemCount;
    const int curPage = selectedIndex / pageItems;
    const int sbY = rect.y + ((rect.height - sbH) * curPage) / (totalPages - 1);
    const int sbX = rect.x + rect.width - CrossPetMetrics::values.scrollBarRightOffset;
    renderer.drawLine(sbX, rect.y, sbX, rect.y + rect.height, true);
    renderer.fillRect(sbX - CrossPetMetrics::values.scrollBarWidth, sbY,
                      CrossPetMetrics::values.scrollBarWidth, sbH, true);
  }

  // Content width accounting for scroll bar
  const int contentW = rect.width -
      (totalPages > 1 ? (CrossPetMetrics::values.scrollBarWidth + CrossPetMetrics::values.scrollBarRightOffset) : 1);

  // Selection highlight — LightGray rounded fill (keeps icons visible)
  if (selectedIndex >= 0) {
    const int selY = rect.y + (selectedIndex % pageItems) * rowH;
    renderer.fillRoundedRect(rect.x + CrossPetMetrics::values.contentSidePadding,
                             selY,
                             contentW - CrossPetMetrics::values.contentSidePadding * 2,
                             rowH, kSelectionRadius, Color::LightGray);
  }

  const int pageStart = selectedIndex / pageItems * pageItems;
  constexpr int listIconSize = 24;
  constexpr int menuIconSize = 32;

  int textX = rect.x + CrossPetMetrics::values.contentSidePadding + kHPad;
  int textW = contentW - CrossPetMetrics::values.contentSidePadding * 2 - kHPad * 2;
  int iconSize = 0;
  if (rowIcon != nullptr) {
    iconSize = (rowSubtitle != nullptr) ? menuIconSize : listIconSize;
    textX += iconSize + kHPad;
    textW -= iconSize + kHPad;
  }

  for (int i = pageStart; i < itemCount && i < pageStart + pageItems; i++) {
    const int itemY = rect.y + (i % pageItems) * rowH;
    const bool isSelected = (i == selectedIndex);
    int rowTextW = textW;

    // Check for section header marker (\x01 prefix)
    auto itemName = rowTitle(i);
    if (!itemName.empty() && itemName[0] == '\x01') {
      // Section header: small uppercase text with top padding, no highlight
      renderer.drawText(SMALL_FONT_ID, textX, itemY + 14, itemName.c_str() + 1, true);
      continue;
    }

    // Value column — measure width first for title truncation
    int valueW = 0;
    std::string valueText;
    if (rowValue != nullptr) {
      valueText = rowValue(i);
      valueText = renderer.truncatedText(UI_10_FONT_ID, valueText.c_str(), kMaxValueWidth);
      valueW = renderer.getTextWidth(UI_10_FONT_ID, valueText.c_str()) + kHPad;
      rowTextW -= valueW;
    }

    // Title — text stays black on LightGray selection
    auto item = renderer.truncatedText(UI_10_FONT_ID, itemName.c_str(), rowTextW);
    renderer.drawText(UI_10_FONT_ID, textX, itemY + 4, item.c_str(), true);

    // Icon — always visible (LightGray selection keeps icons readable)
    if (rowIcon != nullptr) {
      const int iconY = (rowSubtitle != nullptr) ? 16 : 10;
      const uint8_t* iconBmp = cpIconForName(rowIcon(i), iconSize);
      if (iconBmp) {
        renderer.drawIcon(iconBmp,
                          rect.x + CrossPetMetrics::values.contentSidePadding + kHPad,
                          itemY + iconY, iconSize, iconSize);
      }
    }

    // Subtitle
    if (rowSubtitle != nullptr) {
      std::string subtitleText = rowSubtitle(i);
      auto subtitle = renderer.truncatedText(SMALL_FONT_ID, subtitleText.c_str(), rowTextW);
      renderer.drawText(SMALL_FONT_ID, textX, itemY + 30, subtitle.c_str(), true);
    }

    // Value — toggle pill badges for ON/OFF, DarkGray for enum values
    if (!valueText.empty()) {
      const bool isOn = (valueText == I18N.get(StrId::STR_STATE_ON));
      const bool isOff = (valueText == I18N.get(StrId::STR_STATE_OFF));
      const bool isToggle = isOn || isOff;

      if (isToggle) {
        // Pill badge: rounded rect with text inside
        constexpr int pillH = 22;
        constexpr int pillR = 6;
        constexpr int pillPad = 10;
        const int pillTextW = renderer.getTextWidth(SMALL_FONT_ID, valueText.c_str());
        const int pillW = pillTextW + 2 * pillPad;
        const int pillX = rect.x + contentW - CrossPetMetrics::values.contentSidePadding - pillW;
        const int pillY = itemY + (rowH - pillH) / 2;

        if (isOn) {
          // Black filled pill, white text
          renderer.fillRoundedRect(pillX, pillY, pillW, pillH, pillR, Color::Black);
          renderer.drawText(SMALL_FONT_ID, pillX + pillPad, pillY + 2, valueText.c_str(), false);
        } else {
          // Gray outline pill, gray text
          renderer.drawRoundedRect(pillX, pillY, pillW, pillH, 1, pillR, true);
          renderer.drawText(SMALL_FONT_ID, pillX + pillPad, pillY + 2, valueText.c_str(), true);
        }
      } else {
        // Enum/value text in DarkGray for visual hierarchy (label=black, value=gray)
        const int valX = rect.x + contentW - CrossPetMetrics::values.contentSidePadding - valueW;
        renderer.fillRectDither(valX - 2, itemY + 4, valueW + 2, rowH - 8, Color::LightGray);
        renderer.drawText(UI_10_FONT_ID, valX, itemY + 6, valueText.c_str(), true);
      }
    }

    // Thin separator between items (not on selected row, not after last visible item)
    if (!isSelected && i < pageStart + pageItems - 1 && i < itemCount - 1) {
      const int sepY = itemY + rowH - 1;
      renderer.fillRectDither(rect.x + CrossPetMetrics::values.contentSidePadding + kHPad,
                              sepY, contentW - CrossPetMetrics::values.contentSidePadding * 2 - kHPad * 2,
                              1, Color::LightGray);
    }
  }
}

// ── Button menu ───────────────────────────────────────────────────────────────

void CrossPetTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                   const std::function<std::string(int index)>& buttonLabel,
                                   const std::function<UIIcon(int index)>& rowIcon) const {
  for (int i = 0; i < buttonCount; ++i) {
    const int tileW = rect.width - CrossPetMetrics::values.contentSidePadding * 2;
    Rect tileRect{rect.x + CrossPetMetrics::values.contentSidePadding,
                  rect.y + i * (CrossPetMetrics::values.menuRowHeight + CrossPetMetrics::values.menuSpacing),
                  tileW, CrossPetMetrics::values.menuRowHeight};

    if (selectedIndex == i) {
      renderer.fillRoundedRect(tileRect.x, tileRect.y, tileRect.width, tileRect.height,
                               kSelectionRadius, Color::LightGray);
    }

    std::string labelStr = buttonLabel(i);
    const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
    int textX = tileRect.x + 16;
    const int textY = tileRect.y + (CrossPetMetrics::values.menuRowHeight - lineH) / 2;

    if (rowIcon != nullptr) {
      constexpr int menuIconSize = 32;
      const uint8_t* iconBmp = cpIconForName(rowIcon(i), menuIconSize);
      if (iconBmp) {
        renderer.drawIcon(iconBmp, textX, textY + 3, menuIconSize, menuIconSize);
      }
      textX += 32 + kHPad + 2;
    }

    renderer.drawText(UI_12_FONT_ID, textX, textY, labelStr.c_str(), true);
  }
}

// ── Button hints — geometric shapes ──────────────────────────────────────────

void CrossPetTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2,
                                    const char* btn3, const char* btn4) const {
  const GfxRenderer::Orientation orig = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  constexpr int hH = CrossPetMetrics::values.buttonHintsHeight;
  const int hY = sh - hH;

  // Clear area + thin top divider
  renderer.fillRect(0, hY, sw, hH, false);
  renderer.drawLine(0, hY, sw - 1, hY);

  // Text mode: draw labels when provided (most activities)
  const char* labels[] = {btn1, btn2, btn3, btn4};
  constexpr int pos[] = {25, 130, 245, 350};
  constexpr int bw = 106;
  const int cy = hY + hH / 2;
  const int lineH = renderer.getLineHeight(SMALL_FONT_ID);
  const int textY = cy - lineH / 2;

  for (int i = 0; i < 4; i++) {
    if (labels[i] && labels[i][0]) {
      const int tw = renderer.getTextWidth(SMALL_FONT_ID, labels[i]);
      renderer.drawText(SMALL_FONT_ID, pos[i] + (bw - tw) / 2, textY, labels[i], true);
    }
  }

  renderer.setOrientation(orig);
}

// ── Popup — rounded card style ────────────────────────────────────────────────

Rect CrossPetTheme::drawPopup(const GfxRenderer& renderer, const char* message) const {
  constexpr int y = 132;
  const int textW = renderer.getTextWidth(UI_12_FONT_ID, message, EpdFontFamily::REGULAR);
  const int textH = renderer.getLineHeight(UI_12_FONT_ID);
  const int w = textW + kPopupMarginX * 2;
  const int h = textH + kPopupMarginY * 2;
  const int x = (renderer.getScreenWidth() - w) / 2;

  // White fill + 1px black border, radius 12
  renderer.fillRoundedRect(x, y, w, h, kCardRadius, Color::White);
  renderer.drawRoundedRect(x, y, w, h, 1, kCardRadius, true);

  const int textX = x + (w - textW) / 2;
  const int textY = y + kPopupMarginY - 2;
  renderer.drawText(UI_12_FONT_ID, textX, textY, message, true, EpdFontFamily::REGULAR);
  renderer.displayBuffer();

  return Rect{x, y, w, h};
}

// ── Confirm dialog — two-button card ─────────────────────────────────────────

void CrossPetTheme::drawConfirmDialog(const GfxRenderer& renderer, const char* title,
                                      const char* body, const char* cancelLabel,
                                      const char* confirmLabel, bool confirmSelected) const {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  constexpr int margin = 30;
  constexpr int pad = 16;
  const int w = sw - 2 * margin;
  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallH = renderer.getLineHeight(UI_10_FONT_ID);

  // Measure body text (wrapped, max 6 lines)
  auto bodyLines = renderer.wrappedText(UI_10_FONT_ID, body, w - 2 * pad, 6);
  const int bodyH = (int)bodyLines.size() * smallH;

  // Total height: pad + title + sep + body + sep + button row + pad
  constexpr int sepH = 1;
  constexpr int btnH = 40;
  const int h = pad + lineH + pad / 2 + sepH + pad / 2 + bodyH + pad / 2 + sepH + btnH + pad / 2;
  const int x = margin;
  const int y = (sh - h) / 2;

  // White fill + 1px border, radius 12
  renderer.fillRoundedRect(x, y, w, h, kCardRadius, Color::White);
  renderer.drawRoundedRect(x, y, w, h, 1, kCardRadius, true);

  int cy = y + pad;

  // Title bold
  renderer.drawText(UI_12_FONT_ID, x + pad, cy, title, true, EpdFontFamily::BOLD);
  cy += lineH + pad / 2;

  // Separator
  renderer.drawLine(x + pad, cy, x + w - pad, cy);
  cy += sepH + pad / 2;

  // Body text
  for (const auto& line : bodyLines) {
    renderer.drawText(UI_10_FONT_ID, x + pad, cy, line.c_str(), true);
    cy += smallH;
  }
  cy += pad / 2;

  // Separator
  renderer.drawLine(x + pad, cy, x + w - pad, cy);
  cy += sepH;

  // Two buttons: Cancel (left) | Confirm (right)
  const int btnW = (w - 2 * pad) / 2;
  const int btnX1 = x + pad;
  const int btnX2 = x + pad + btnW;
  const int btnY = cy;

  // Vertical divider between buttons
  renderer.drawLine(btnX2, btnY, btnX2, btnY + btnH);

  // Cancel button (filled when selected)
  if (!confirmSelected) {
    renderer.fillRect(btnX1, btnY, btnW, btnH, true);
  }
  const int cancelW = renderer.getTextWidth(UI_10_FONT_ID, cancelLabel);
  renderer.drawText(UI_10_FONT_ID, btnX1 + (btnW - cancelW) / 2,
                    btnY + (btnH - smallH) / 2, cancelLabel, confirmSelected);

  // Confirm button (filled when selected)
  if (confirmSelected) {
    renderer.fillRect(btnX2, btnY, btnW, btnH, true);
  }
  const int confirmW = renderer.getTextWidth(UI_10_FONT_ID, confirmLabel);
  renderer.drawText(UI_10_FONT_ID, btnX2 + (btnW - confirmW) / 2,
                    btnY + (btnH - smallH) / 2, confirmLabel, !confirmSelected);

  renderer.displayBuffer();
}

// ── Keyboard key — LightGray selection ───────────────────────────────────────

void CrossPetTheme::drawKeyboardKey(const GfxRenderer& renderer, Rect rect, const char* label,
                                    const bool isSelected) const {
  if (isSelected) {
    // LightGray fill + 1px border; text stays black
    renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, kSelectionRadius, Color::LightGray);
    renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 1, kSelectionRadius, true);
  }

  const int textW = renderer.getTextWidth(UI_12_FONT_ID, label);
  const int textX = rect.x + (rect.width - textW) / 2;
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  // Text always black (not inverted like Lyra)
  renderer.drawText(UI_12_FONT_ID, textX, textY, label, true);
}
