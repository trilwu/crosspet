#include "VirtualPetActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"
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

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!PET_MANAGER.exists() || !PET_MANAGER.isAlive()) {
      PET_MANAGER.hatchNew();
    } else {
      PET_MANAGER.pet();
      PET_MANAGER.save();
    }
    requestUpdate();
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
  const auto pageHeight = renderer.getScreenHeight();
  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int centerY = contentTop + (pageHeight - contentTop - metrics.buttonHintsHeight) / 2;

  renderer.drawCenteredText(UI_10_FONT_ID, centerY - lh, tr(STR_PET_NO_PET));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_PET_HATCH_EGG), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  (void)pageWidth;
}

void VirtualPetActivity::renderDead() const {
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int centerY = contentTop + (pageHeight - contentTop - metrics.buttonHintsHeight) / 2;

  // Draw dead sprite
  const int spriteX = (renderer.getScreenWidth() - PetSpriteRenderer::SPRITE_W) / 2;
  PetSpriteRenderer::drawPet(renderer, spriteX, centerY - PetSpriteRenderer::SPRITE_H - lh,
                              PetStage::DEAD, PetMood::DEAD);

  renderer.drawCenteredText(UI_10_FONT_ID, centerY, tr(STR_PET_DEAD_MESSAGE));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_PET_HATCH_NEW), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void VirtualPetActivity::renderAlive() const {
  const auto& state = PET_MANAGER.getState();
  const auto mood = PET_MANAGER.getMood();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int contentHeight = contentBottom - contentTop;

  // --- Pet sprite (48x48 drawn in a 144x144 scaled visual area via centered placement) ---
  // We draw the actual 48x48 sprite but position it in the upper third of content area
  const int spriteAreaH = contentHeight * 2 / 5;
  const int spriteX = (pageWidth - PetSpriteRenderer::SPRITE_W) / 2;
  const int spriteY = contentTop + (spriteAreaH - PetSpriteRenderer::SPRITE_H) / 2;
  PetSpriteRenderer::drawPet(renderer, spriteX, spriteY, state.stage, mood);

  // --- Stage + days info ---
  const char* stageLabelStr = tr(STR_PET_STAGE_EGG);
  switch (state.stage) {
    case PetStage::HATCHLING: stageLabelStr = tr(STR_PET_STAGE_HATCHLING); break;
    case PetStage::YOUNGSTER: stageLabelStr = tr(STR_PET_STAGE_YOUNGSTER); break;
    case PetStage::COMPANION: stageLabelStr = tr(STR_PET_STAGE_COMPANION); break;
    case PetStage::ELDER:     stageLabelStr = tr(STR_PET_STAGE_ELDER);     break;
    default: break;
  }
  char infoLine[64];
  snprintf(infoLine, sizeof(infoLine), "%s  |  Day %lu  |  Streak %u", stageLabelStr,
           (unsigned long)PET_MANAGER.getDaysAlive(), (unsigned)state.currentStreak);

  const int infoY = contentTop + spriteAreaH + metrics.verticalSpacing / 2;
  renderer.drawCenteredText(SMALL_FONT_ID, infoY, infoLine);

  // --- Stat bars ---
  const int barAreaTop = infoY + renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;
  const int barW = pageWidth - 80;  // leave margins for labels
  const int barX = (pageWidth - barW) / 2;
  const int barSpacing = renderer.getLineHeight(UI_10_FONT_ID) + 10;

  drawStatBar(barX, barAreaTop,                   barW, tr(STR_PET_HUNGER),    state.hunger);
  drawStatBar(barX, barAreaTop + barSpacing,       barW, tr(STR_PET_HAPPINESS), state.happiness);
  drawStatBar(barX, barAreaTop + barSpacing * 2,   barW, tr(STR_PET_HEALTH),    state.health);

  // --- Daily missions ---
  const int missionTop = barAreaTop + barSpacing * 3 + metrics.verticalSpacing;
  drawMissions(barX, missionTop, barW);

  // --- Button hints ---
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_PET_HAPPINESS), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

// ---- Missions panel -----------------------------------------------------

void VirtualPetActivity::drawMissions(int x, int y, int width) const {
  PetMission missions[3];
  PET_MANAGER.getMissions(missions);

  const int lh = renderer.getLineHeight(SMALL_FONT_ID);
  const int rowH = lh + 4;
  const int checkW = renderer.getTextWidth(SMALL_FONT_ID, "[x]");

  // Section header
  renderer.drawText(SMALL_FONT_ID, x, y, "Today:");
  int rowY = y + rowH;

  for (int i = 0; i < 3; i++) {
    const auto& m = missions[i];

    // Checkbox [x] or [ ]
    renderer.drawText(SMALL_FONT_ID, x, rowY, m.done ? "[x]" : "[ ]");

    // Mission label + progress (if not done)
    char buf[40];
    if (m.done) {
      snprintf(buf, sizeof(buf), "%s", m.label);
    } else {
      snprintf(buf, sizeof(buf), "%s  %d/%d", m.label, m.progress, m.goal);
    }
    renderer.drawText(SMALL_FONT_ID, x + checkW + 4, rowY, buf);
    rowY += rowH;
  }
}

// ---- Stat bar helper ----------------------------------------------------

void VirtualPetActivity::drawStatBar(int x, int y, int barWidth, const char* label,
                                      uint8_t value) const {
  constexpr int BAR_H = 10;
  const int lh = renderer.getLineHeight(SMALL_FONT_ID);

  // Label left-aligned
  renderer.drawText(SMALL_FONT_ID, x, y, label);

  // Bar outline
  const int barY = y + (lh - BAR_H) / 2;
  renderer.drawRect(x, barY, barWidth, BAR_H);

  // Fill proportional to value (0-100)
  if (value > 0) {
    const int fillW = (barWidth - 2) * value / 100;
    if (fillW > 0) {
      renderer.fillRect(x + 1, barY + 1, fillW, BAR_H - 2);
    }
  }
}
