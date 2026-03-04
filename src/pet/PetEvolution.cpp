#include "PetEvolution.h"

#include <Logging.h>

namespace {

// Determine reading-based variant at an evolution branching point.
// Returns 0=Scholar, 1=Balanced, 2=Wild.
// Called after state.stage is already advanced; prevStageIdx = new stage index - 1.
static uint8_t determineVariant(const PetState& state) {
  uint8_t prevStageIdx = static_cast<uint8_t>(state.stage) - 1;
  uint16_t minPages = PetConfig::EVOLUTION[prevStageIdx].minPages;
  uint16_t scholarThreshold = minPages + (minPages / 2);  // 1.5x

  // Scholar: active reader with streaks and at least one book finished
  if (state.currentStreak >= 7 &&
      state.booksFinished >= 1 &&
      state.totalPagesRead >= scholarThreshold) {
    return 0;  // Scholar
  }

  // Wild: barely reading, no streak maintenance
  if (state.currentStreak < 3 &&
      state.totalPagesRead <= minPages + 50) {
    return 2;  // Wild
  }

  return 1;  // Balanced (default)
}

}  // namespace

namespace PetEvolution {

void checkEvolution(PetState& state) {
  uint8_t stageIdx = static_cast<uint8_t>(state.stage);
  if (stageIdx >= static_cast<uint8_t>(PetStage::ELDER)) return;

  const auto& req = PetConfig::EVOLUTION[stageIdx];
  if (state.daysAtStage < req.minDays ||
      state.totalPagesRead < req.minPages ||
      state.hunger < req.minAvgHunger) return;

  // Additional reading gate for Companion → Elder
  if (stageIdx == 3) {
    if (state.currentStreak < 7 || state.booksFinished < 1) return;
  }

  // Advance to next stage
  state.stage = static_cast<PetStage>(stageIdx + 1);
  state.daysAtStage = 0;

  // Assign variant at branching stages (YOUNGSTER and COMPANION)
  if (state.stage == PetStage::YOUNGSTER || state.stage == PetStage::COMPANION) {
    state.evolutionVariant = determineVariant(state);
  }

  LOG_DBG("PET", "Evolved to stage %d variant %d", (int)state.stage, state.evolutionVariant);
}

const char* variantStageName(PetStage stage, uint8_t variant) {
  switch (stage) {
    case PetStage::YOUNGSTER:
      if (variant == 0) return "Scholarly Young";
      if (variant == 2) return "Wild Youngster";
      return "Youngster";
    case PetStage::COMPANION:
      if (variant == 0) return "Scholar";
      if (variant == 2) return "Wild Companion";
      return "Companion";
    case PetStage::EGG:       return "Egg";
    case PetStage::HATCHLING: return "Hatchling";
    case PetStage::ELDER:     return "Elder";
    default:                  return "???";
  }
}

}  // namespace PetEvolution
