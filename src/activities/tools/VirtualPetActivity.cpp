#include "VirtualPetActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"
#include "pet/PetEvolution.h"
#include "pet/PetManager.h"
#include "pet/PetSpriteRenderer.h"

// ---- Lifecycle ----------------------------------------------------------

void VirtualPetActivity::onEnter() {
  Activity::onEnter();
  PET_MANAGER.load();
  PET_MANAGER.tick();
  requestUpdate();
}

void VirtualPetActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!PET_MANAGER.exists() || !PET_MANAGER.isAlive()) {
    // Confirm hatches a new egg from no-pet or dead state
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      PET_MANAGER.hatchNew();
      requestUpdate();
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

  if (!PET_MANAGER.exists()) {
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

  // --- Stage | Day | Streak info line ---
  const int infoY = iconsY + renderer.getLineHeight(SMALL_FONT_ID) + 4;
  const char* stageName = PetEvolution::variantStageName(state.stage, state.evolutionVariant);
  char infoLine[64];
  snprintf(infoLine, sizeof(infoLine), "%s  |  Day %lu  |  Streak %u",
           stageName, (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);
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
