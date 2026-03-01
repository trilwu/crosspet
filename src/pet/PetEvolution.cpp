#include "PetEvolution.h"

#include <Logging.h>

namespace {

// Determine which variant applies at an evolution branching point.
// Returns 0=good, 1=chubby, 2=misbehaved.
static uint8_t determineVariant(const PetState& state) {
  if (state.avgCareScore >= 70 &&
      state.weight >= PetConfig::UNDERWEIGHT_THRESHOLD &&
      state.weight <= PetConfig::OVERWEIGHT_THRESHOLD &&
      state.discipline >= 50) {
    return 0;  // Good care → standard evolution
  }
  if (state.weight > PetConfig::OVERWEIGHT_THRESHOLD) {
    return 1;  // Overweight → chubby variant
  }
  if (state.discipline < 30) {
    return 2;  // Low discipline → misbehaved variant
  }
  return 0;  // Default to good
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
      if (variant == 1) return "Pudgy Youngster";
      if (variant == 2) return "Rowdy Youngster";
      return "Youngster";
    case PetStage::COMPANION:
      if (variant == 1) return "Chonky Companion";
      if (variant == 2) return "Wild Companion";
      return "Companion";
    case PetStage::EGG:      return "Egg";
    case PetStage::HATCHLING: return "Hatchling";
    case PetStage::ELDER:     return "Elder";
    default:                  return "???";
  }
}

}  // namespace PetEvolution
