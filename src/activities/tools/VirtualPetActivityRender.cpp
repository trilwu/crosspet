#include "VirtualPetActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"
#include "ReadingStats.h"
#include "pet/PetEvolution.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"
#include "pet/PetState.h"

// ---- Render entry point -------------------------------------------------

void VirtualPetActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_VIRTUAL_PET));

  if (screenMode == ScreenMode::TYPE_SELECT) {
    renderTypeSelect();
  } else if (!PET_MANAGER.exists()) {
    renderNoPet();
  } else if (!PET_MANAGER.isAlive()) {
    renderDead();
  } else {
    renderAlive();
  }

  renderer.displayBuffer();
}

// ---- Sub-renderers ------------------------------------------------------

void VirtualPetActivity::renderNoPet() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageHeight = renderer.getScreenHeight();
  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int centerY = contentTop + (pageHeight - contentTop - metrics.buttonHintsHeight) / 2;

  renderer.drawCenteredText(UI_10_FONT_ID, centerY - lh, tr(STR_PET_NO_PET));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_PET_HATCH_EGG), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void VirtualPetActivity::renderDead() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageHeight = renderer.getScreenHeight();
  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int centerY = contentTop + (pageHeight - contentTop - metrics.buttonHintsHeight) / 2;

  constexpr int PET_SCALE = 3;
  const int petSize = PetSpriteRenderer::displaySize(PET_SCALE);
  const int spriteX = (renderer.getScreenWidth() - petSize) / 2;
  PetSpriteRenderer::drawPet(renderer, spriteX, centerY - petSize - lh,
                              PetStage::DEAD, PetMood::DEAD, PET_SCALE, 0,
                              PET_MANAGER.getState().petType);
  renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_PET_DEAD_MESSAGE));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_PET_HATCH_NEW), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void VirtualPetActivity::renderAlive() const {
  const auto& state = PET_MANAGER.getState();
  const auto mood = PET_MANAGER.getMood();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // --- Pet sprite (144x144) centered at top, with idle animation offset ---
  constexpr int PET_SCALE = 3;
  const int petSize = PetSpriteRenderer::displaySize(PET_SCALE);
  int spriteX = (pageWidth - petSize) / 2;
  int spriteY = contentTop;

  // Idle animation: 3-frame cycle (0→1→2→0)
  // Egg wobbles left/center/right; living pets bob up/down/up
  static const int EGG_WOBBLE[3] = {-2, 0, 2};
  static const int BOB[3] = {0, 3, 0};
  if (state.stage == PetStage::EGG) {
    spriteX += EGG_WOBBLE[animFrame];
  } else {
    spriteY += BOB[animFrame];
  }

  PetSpriteRenderer::drawPet(renderer, spriteX, spriteY, state.stage, mood, PET_SCALE,
                              state.evolutionVariant, state.petType, animFrame);

  // Draw action feedback icon next to sprite (top-right)
  if (actionIcon != PetAnimIcon::NONE) {
    drawPetActionIcon(renderer, actionIcon, spriteX + petSize + 8, contentTop + 8);
  }

  // --- Status icons row below sprite ---
  const int iconsY = contentTop + petSize + 4;
  const int sideMargin = 30;
  statsPanel.renderStatusIcons(renderer, state, sideMargin, iconsY, pageWidth - sideMargin * 2);

  // --- Name | Stage | Day | Streak info line ---
  const int infoY = iconsY + renderer.getLineHeight(SMALL_FONT_ID) + 4;
  const char* stageName = PetEvolution::variantStageName(state.stage, state.evolutionVariant);
  char infoLine[80];
  if (state.petName[0]) {
    if (state.petType > 0) {
      snprintf(infoLine, sizeof(infoLine), tr(STR_PET_INFO_FMT_NAME_TYPE),
               state.petName, PetEvolution::typeName(state.petType),
               (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);
    } else {
      snprintf(infoLine, sizeof(infoLine), tr(STR_PET_INFO_FMT_NAME_TYPE),
               state.petName, stageName,
               (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);
    }
  } else {
    if (state.petType > 0) {
      snprintf(infoLine, sizeof(infoLine), tr(STR_PET_INFO_FMT_TYPE_STAGE),
               PetEvolution::typeName(state.petType), stageName,
               (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);
    } else {
      snprintf(infoLine, sizeof(infoLine), tr(STR_PET_INFO_FMT_STAGE),
               stageName, (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);
    }
  }
  renderer.drawCenteredText(SMALL_FONT_ID, infoY, infoLine);

  // --- Stat bars ---
  const int statsY = infoY + renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;
  const int barW = pageWidth - sideMargin * 2;
  statsPanel.renderStats(renderer, state, sideMargin, statsY, barW);

  // --- Reading stats compact row ---
  const int barSpacing = renderer.getLineHeight(SMALL_FONT_ID) + 10;
  int readY = statsY + barSpacing * 7 + 4;
  {
    renderer.fillRect(sideMargin + 20, readY, barW - 40, 1);
    readY += 6;
    uint32_t mins = READ_STATS.todayReadSeconds / 60;
    char readLine[80];
    snprintf(readLine, sizeof(readLine), "%s %lum  |  %s %ud  |  %s %u",
             tr(STR_STATS_TODAY), (unsigned long)mins,
             tr(STR_STATS_STREAK), (unsigned)READ_STATS.currentStreak,
             tr(STR_STATS_BOOKS_DONE), (unsigned)READ_STATS.booksFinished);
    renderer.drawCenteredText(SMALL_FONT_ID, readY, readLine);
    readY += renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;
  }

  // --- Action menu fills remaining space ---
  const int menuY = readY;
  const int menuH = contentBottom - menuY;
  actionMenu.render(renderer, state, sideMargin, menuY, barW, menuH);

  // --- Feedback text above button hints ---
  if (PET_MANAGER.getLastFeedback() != nullptr) {
    const int fbY = contentBottom - renderer.getLineHeight(SMALL_FONT_ID) - 2;
    renderer.drawCenteredText(SMALL_FONT_ID, fbY, PET_MANAGER.getLastFeedback());
  }

  // --- Button hints ---
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

// ---- Type selection screen -----------------------------------------------

void VirtualPetActivity::renderTypeSelect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  renderer.drawCenteredText(SMALL_FONT_ID, contentTop, tr(STR_PET_SELECT_TYPE));

  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int rowH = lh + 8;
  const int listTop = contentTop + renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;

  // Layout: left = sprite preview, right = type list
  constexpr int PREVIEW_SCALE = 3;
  const int previewSize = PetSpriteRenderer::displaySize(PREVIEW_SCALE);  // 144px
  const int previewX = metrics.contentSidePadding;
  const int previewY = listTop + 8;

  // Draw preview sprite of selected type at COMPANION stage (most recognizable)
  PetSpriteRenderer::drawPet(renderer, previewX, previewY, PetStage::COMPANION, PetMood::HAPPY,
                              PREVIEW_SCALE, 0, static_cast<uint8_t>(typeSelectIndex));

  // Draw selected type name below preview
  const char* selectedName = PetEvolution::typeName(typeSelectIndex);
  const int nameY = previewY + previewSize + 8;
  const int nameCenterX = previewX + previewSize / 2;
  int tw = renderer.getTextWidth(UI_10_FONT_ID, selectedName);
  renderer.drawText(UI_10_FONT_ID, nameCenterX - tw / 2, nameY, selectedName, false, EpdFontFamily::BOLD);

  // Type list on the right side
  const int listX = previewX + previewSize + 20;
  const int listW = pageWidth - listX - metrics.contentSidePadding;

  for (int i = 0; i < static_cast<int>(PetTypeNames::COUNT); i++) {
    const int rowY = listTop + i * rowH;
    if (rowY + rowH > pageHeight - metrics.buttonHintsHeight) break;

    const bool selected = (i == typeSelectIndex);
    if (selected) {
      renderer.fillRect(listX - 4, rowY - 2, listW + 8, rowH, false);
      renderer.drawText(UI_10_FONT_ID, listX, rowY, PetEvolution::typeName(i), /*invert=*/true);
    } else {
      renderer.drawText(UI_10_FONT_ID, listX, rowY, PetEvolution::typeName(i));
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
