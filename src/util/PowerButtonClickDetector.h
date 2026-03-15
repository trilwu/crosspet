#pragma once
#include <Arduino.h>
#include <cstdint>

/// Detects single, double, and triple clicks on the power button.
/// Call update() every loop iteration with the current wasReleased state.
/// After multi-click timeout, getClickCount() returns the finalized count (1/2/3)
/// for exactly one loop iteration, then resets to 0.
class PowerButtonClickDetector {
 public:
  /// Feed the current power button release state. Call once per loop.
  void update(bool wasReleased) {
    // Clear previous result
    finalizedClicks_ = 0;

    if (wasReleased) {
      pendingClicks_++;
      lastClickMs_ = millis();
    }

    // Finalize after timeout — no more clicks expected
    if (pendingClicks_ > 0 && (millis() - lastClickMs_) > MULTI_CLICK_TIMEOUT_MS) {
      finalizedClicks_ = (pendingClicks_ > MAX_CLICKS) ? MAX_CLICKS : pendingClicks_;
      pendingClicks_ = 0;
    }
  }

  /// Returns finalized click count (1/2/3) for one frame, then 0.
  uint8_t getClickCount() const { return finalizedClicks_; }

  /// Returns true if currently accumulating clicks (waiting for timeout).
  bool isPending() const { return pendingClicks_ > 0; }

 private:
  uint8_t pendingClicks_ = 0;
  uint8_t finalizedClicks_ = 0;
  unsigned long lastClickMs_ = 0;

  static constexpr unsigned long MULTI_CLICK_TIMEOUT_MS = 400;
  static constexpr uint8_t MAX_CLICKS = 3;
};

// Global instance (defined in main.cpp)
extern PowerButtonClickDetector pwrClickDetector;

/// Returns the SHORT_PWRBTN action for the current finalized click count (0 if no event).
/// Reader activities call this instead of checking wasReleased(Power) directly.
uint8_t getPowerClickAction();
