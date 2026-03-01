#include "PetCareTracker.h"

#include <Arduino.h>
#include <Logging.h>

namespace {

static inline uint8_t clampSub(uint8_t val, uint8_t amount) {
  return (val > amount) ? (val - amount) : 0;
}

static inline uint8_t clampAdd(uint8_t val, uint8_t amount) {
  uint16_t r = static_cast<uint16_t>(val) + amount;
  return (r > PetConfig::MAX_STAT) ? PetConfig::MAX_STAT : static_cast<uint8_t>(r);
}

// Returns true if the current hour is within the sleep window
static inline bool isSleepHour(uint8_t hour) {
  return hour >= PetConfig::SLEEP_HOUR || hour < PetConfig::WAKE_HOUR;
}

// Detect the highest-priority need (NONE if no urgent need)
static PetNeed detectNeed(const PetState& state) {
  if (state.isSick)            return PetNeed::SICK;
  if (state.hunger < 30)       return PetNeed::HUNGRY;
  if (state.wasteCount > 0)    return PetNeed::DIRTY;
  if (state.happiness < 30)    return PetNeed::BORED;
  return PetNeed::NONE;
}

}  // namespace

namespace PetCareTracker {

void checkCareMistakes(PetState& state, uint32_t elapsedHours) {
  // Accumulate mistake tracking hours using wasteCount as a proxy timer.
  // Real implementation: track elapsed hungry/sick/dirty hours via counters.
  // Simplified: each tick checks threshold and adds mistake once per danger period.
  // We use elapsedHours as the unit and only flag if condition persists long enough.
  for (uint32_t h = 0; h < elapsedHours && h < 24; h++) {
    if (state.hunger == 0 && h >= PetConfig::CARE_MISTAKE_HUNGRY_HOURS) {
      if (state.careMistakes < 255) state.careMistakes++;
      break;
    }
  }
  if (state.isSick && state.sicknessTimer > 0 &&
      (PetConfig::SICK_RECOVERY_HOURS - state.sicknessTimer) >= PetConfig::CARE_MISTAKE_SICK_HOURS) {
    // Sick for too long without medicine
    if (elapsedHours >= PetConfig::CARE_MISTAKE_SICK_HOURS) {
      if (state.careMistakes < 255) state.careMistakes++;
    }
  }
}

void generateAttentionCall(PetState& state, uint32_t nowSec) {
  // Don't generate if: already calling, sleeping, or too soon since last call
  if (state.attentionCall) return;
  if (state.isSleeping) return;
  if (nowSec > 0 && state.lastCallTime > 0 &&
      (nowSec - state.lastCallTime) < PetConfig::ATTENTION_CALL_INTERVAL_SEC) return;

  // Check for a real need first
  PetNeed need = detectNeed(state);

  if (need == PetNeed::NONE) {
    // Roll for a fake call (discipline test)
    if (random(100) >= PetConfig::FAKE_CALL_CHANCE_PERCENT) return;
    state.attentionCall = true;
    state.isFakeCall = true;
    state.currentNeed = PetNeed::NONE;
  } else {
    state.attentionCall = true;
    state.isFakeCall = false;
    state.currentNeed = need;
  }

  state.lastCallTime = nowSec;
  LOG_DBG("PET", "Attention call: fake=%d need=%d", state.isFakeCall, (int)state.currentNeed);
}

void expireAttentionCall(PetState& state, uint32_t nowSec) {
  if (!state.attentionCall) return;
  if (state.lastCallTime == 0) return;
  if (nowSec <= state.lastCallTime) return;

  uint32_t elapsed = nowSec - state.lastCallTime;
  if (elapsed < PetConfig::ATTENTION_CALL_EXPIRE_SEC) return;

  // Call expired — treat as ignored
  if (!state.isFakeCall) {
    // Real need ignored = care mistake + happiness penalty
    if (state.careMistakes < 255) state.careMistakes++;
    state.happiness = clampSub(state.happiness, 10);
    LOG_DBG("PET", "Real attention call expired (ignored) — care mistake added");
  } else {
    // Fake call ignored = good discipline training
    state.discipline = clampAdd(state.discipline, PetConfig::DISCIPLINE_PER_IGNORE_FAKE);
    LOG_DBG("PET", "Fake call expired (ignored) — discipline +%d", PetConfig::DISCIPLINE_PER_IGNORE_FAKE);
  }

  state.attentionCall = false;
  state.isFakeCall = false;
  state.currentNeed = PetNeed::NONE;
}

void updateCareScore(PetState& state) {
  // Weighted rolling average: 60% old score + 40% today's score
  uint8_t daily = (state.hunger + state.happiness + state.health) / 3;
  state.avgCareScore = static_cast<uint8_t>((state.avgCareScore * 6 + daily * 4) / 10);
}

}  // namespace PetCareTracker
