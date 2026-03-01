#include "VirtualPetActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstring>

#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "pet/PetEvolution.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"
#include "pet/PetState.h"

// ---- Lifecycle ----------------------------------------------------------

void VirtualPetActivity::onEnter() {
  Activity::onEnter();
  PET_MANAGER.load();
  PET_MANAGER.tick();
  requestUpdate();
}

void VirtualPetActivity::loop() {
  // Type selection mode: handle Up/Down/Confirm/Back inline
  if (screenMode == ScreenMode::TYPE_SELECT) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      screenMode = ScreenMode::NORMAL;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      typeSelectIndex = (typeSelectIndex > 0) ? typeSelectIndex - 1 : PetTypeNames::COUNT - 1;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      typeSelectIndex = (typeSelectIndex + 1) % PetTypeNames::COUNT;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      confirmTypeSelect();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!PET_MANAGER.exists() || !PET_MANAGER.isAlive()) {
    // Confirm starts the hatch flow (name → type → hatch)
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      startHatchFlow();
    }
    return;
  }

  bool changed = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    actionMenu.moveUp();
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    actionMenu.moveDown();
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    executeSelectedAction();
    changed = true;
  }
  if (changed) requestUpdate();
}

void VirtualPetActivity::executeSelectedAction() {
  const PetAction action = actionMenu.getSelected();
  switch (action) {
    case PetAction::FEED_MEAL:     PET_MANAGER.feedMeal();      break;
    case PetAction::FEED_SNACK:    PET_MANAGER.feedSnack();     break;
    case PetAction::MEDICINE:      PET_MANAGER.giveMedicine();  break;
    case PetAction::EXERCISE:      PET_MANAGER.exercise();      break;
    case PetAction::CLEAN:         PET_MANAGER.cleanBathroom(); break;
    case PetAction::SCOLD:         PET_MANAGER.disciplinePet(); break;
    case PetAction::IGNORE_CRY:    PET_MANAGER.ignoreCry();     break;
    case PetAction::TOGGLE_LIGHTS: PET_MANAGER.toggleLights();  break;
    case PetAction::PET_PET:       PET_MANAGER.pet();           break;
    case PetAction::RENAME:        startRenameFlow();           return;
    case PetAction::CHANGE_TYPE:   startTypeSelectForChange();  return;
    default: break;
  }
}

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

  constexpr int PET_SCALE = 2;
  const int petSize = PetSpriteRenderer::displaySize(PET_SCALE);
  const int spriteX = (renderer.getScreenWidth() - petSize) / 2;
  PetSpriteRenderer::drawPet(renderer, spriteX, centerY - petSize - lh,
                              PetStage::DEAD, PetMood::DEAD, PET_SCALE);
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

  // --- Pet sprite (96x96) centered at top ---
  constexpr int PET_SCALE = 2;
  const int petSize = PetSpriteRenderer::displaySize(PET_SCALE);
  const int spriteX = (pageWidth - petSize) / 2;
  PetSpriteRenderer::drawPet(renderer, spriteX, contentTop, state.stage, mood, PET_SCALE,
                              state.evolutionVariant);

  // --- Status icons row below sprite ---
  const int iconsY = contentTop + petSize + 4;
  const int sideMargin = 30;
  statsPanel.renderStatusIcons(renderer, state, sideMargin, iconsY, pageWidth - sideMargin * 2);

  // --- Name | Stage | Day | Streak info line ---
  const int infoY = iconsY + renderer.getLineHeight(SMALL_FONT_ID) + 4;
  const char* stageName = PetEvolution::variantStageName(state.stage, state.evolutionVariant);
  char infoLine[80];
  if (state.petName[0]) {
    // Show custom name and species type (if not Default), plus day/streak
    if (state.petType > 0) {
      snprintf(infoLine, sizeof(infoLine), "%s (%s)  |  Day %lu  |  Streak %u",
               state.petName, PetTypeNames::get(state.petType),
               (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);
    } else {
      snprintf(infoLine, sizeof(infoLine), "%s (%s)  |  Day %lu  |  Streak %u",
               state.petName, stageName,
               (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);
    }
  } else {
    // No custom name: show stage (+ type if non-default)
    if (state.petType > 0) {
      snprintf(infoLine, sizeof(infoLine), "%s %s  |  Day %lu  |  Streak %u",
               PetTypeNames::get(state.petType), stageName,
               (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);
    } else {
      snprintf(infoLine, sizeof(infoLine), "%s  |  Day %lu  |  Streak %u",
               stageName, (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);
    }
  }
  renderer.drawCenteredText(SMALL_FONT_ID, infoY, infoLine);

  // --- Stat bars ---
  const int statsY = infoY + renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;
  const int barW = pageWidth - sideMargin * 2;
  statsPanel.renderStats(renderer, state, sideMargin, statsY, barW);

  // --- Action menu fills remaining space ---
  const int barSpacing = renderer.getLineHeight(SMALL_FONT_ID) + 10;
  const int menuY = statsY + barSpacing * 6 + metrics.verticalSpacing;
  const int menuH = contentBottom - menuY;
  actionMenu.render(renderer, state, sideMargin, menuY, barW, menuH);

  // --- Feedback text above button hints ---
  if (PET_MANAGER.getLastFeedback() != nullptr) {
    const int fbY = contentBottom - renderer.getLineHeight(SMALL_FONT_ID) - 2;
    renderer.drawCenteredText(SMALL_FONT_ID, fbY, PET_MANAGER.getLastFeedback());
  }

  // --- Button hints ---
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Select", "Up", "Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

// ---- Type selection screen -----------------------------------------------

void VirtualPetActivity::renderTypeSelect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  // Prompt
  renderer.drawCenteredText(SMALL_FONT_ID, contentTop, tr(STR_PET_SELECT_TYPE));

  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int rowH = lh + 8;
  const int listTop = contentTop + renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;
  const int sideMargin = metrics.contentSidePadding;
  const int listW = pageWidth - sideMargin * 2;

  for (int i = 0; i < static_cast<int>(PetTypeNames::COUNT); i++) {
    const int rowY = listTop + i * rowH;
    if (rowY + rowH > pageHeight - metrics.buttonHintsHeight) break;

    const bool selected = (i == typeSelectIndex);
    if (selected) {
      renderer.fillRect(sideMargin - 4, rowY - 2, listW + 8, rowH, false);
      renderer.drawText(UI_10_FONT_ID, sideMargin, rowY, PetTypeNames::NAMES[i], /*invert=*/false);
    } else {
      renderer.drawText(UI_10_FONT_ID, sideMargin, rowY, PetTypeNames::NAMES[i]);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

// ---- Hatch / rename / type-change flow ------------------------------------

void VirtualPetActivity::startHatchFlow() {
  // Step 1: ask for a name via keyboard
  const char* currentName = PET_MANAGER.exists() ? PET_MANAGER.getState().petName : "";
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput,
                                              tr(STR_PET_ENTER_NAME), currentName, 19, false),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          auto text = std::get<KeyboardResult>(result.data).text;
          strncpy(pendingName, text.c_str(), sizeof(pendingName) - 1);
          pendingName[sizeof(pendingName) - 1] = '\0';
        } else {
          pendingName[0] = '\0';
        }
        // Step 2: show type selection
        startTypeSelectForHatch();
      });
}

void VirtualPetActivity::startRenameFlow() {
  const char* currentName = PET_MANAGER.getState().petName;
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput,
                                              tr(STR_PET_ENTER_NAME), currentName, 19, false),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          auto text = std::get<KeyboardResult>(result.data).text;
          PET_MANAGER.renamePet(text.c_str());
        }
        screenMode = ScreenMode::NORMAL;
        requestUpdate();
      });
}

void VirtualPetActivity::startTypeSelectForHatch() {
  hatchAfterTypeSelect = true;
  typeSelectIndex = 0;
  screenMode = ScreenMode::TYPE_SELECT;
  requestUpdate();
}

void VirtualPetActivity::startTypeSelectForChange() {
  hatchAfterTypeSelect = false;
  typeSelectIndex = static_cast<int>(PET_MANAGER.getState().petType);
  screenMode = ScreenMode::TYPE_SELECT;
  requestUpdate();
}

void VirtualPetActivity::confirmTypeSelect() {
  const uint8_t selectedType = static_cast<uint8_t>(typeSelectIndex);
  if (hatchAfterTypeSelect) {
    PET_MANAGER.hatchNew(pendingName, selectedType);
    pendingName[0] = '\0';
  } else {
    PET_MANAGER.changeType(selectedType);
  }
  screenMode = ScreenMode::NORMAL;
  requestUpdate();
}
