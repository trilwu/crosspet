#pragma once

#include <cstdint>

#include "PetState.h"

// Tracks care mistakes, attention calls, discipline changes, and care score.
// All functions are stateless; operate on PetState& reference.
namespace PetCareTracker {

  // Check for new care mistakes based on elapsed time and current state.
  // Increments state.careMistakes for: hungry too long, sick too long, dirty too long.
  void checkCareMistakes(PetState& state, uint32_t elapsedHours);

  // Attempt to generate an attention call (called from tick).
  // Checks awake state, no active call, and elapsed time since last call.
  // Sets attentionCall, isFakeCall, currentNeed if triggered.
  void generateAttentionCall(PetState& state, uint32_t nowSec);

  // Expire an attention call if it has been active too long (> ATTENTION_CALL_EXPIRE_SEC).
  // Treats expired real calls as ignored (care mistake + happiness penalty).
  void expireAttentionCall(PetState& state, uint32_t nowSec);

  // Update the rolling average care score (call once per day in tick).
  void updateCareScore(PetState& state);

}  // namespace PetCareTracker
