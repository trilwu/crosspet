#pragma once

#include <cstdint>

#include "PetState.h"

// Applies hourly stat decay to a PetState.
// Handles: hunger/happiness/health decay, sleep skip, sickness effects,
// weight normalization, waste happiness penalty, and sickness triggering.
// All logic is stateless; operates on PetState& reference.
namespace PetDecayEngine {

  // Apply decay for the given number of elapsed hours.
  // startHour: the hour-of-day (0-23) at the START of the elapsed period
  //            (used to determine sleep windows for hour-by-hour simulation).
  void applyDecay(PetState& state, uint32_t elapsedHours, uint8_t startHour);

}  // namespace PetDecayEngine
