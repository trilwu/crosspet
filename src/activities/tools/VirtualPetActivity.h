#pragma once
#include "../Activity.h"

// Main virtual pet interaction screen.
// Shows pet sprite (3x scaled area), stat bars, mood, and button hints.
// Confirm = pet interaction | Back = exit
// Dead/no-pet states prompt hatching a new egg.
class VirtualPetActivity final : public Activity {
 public:
  explicit VirtualPetActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Virtual Pet", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Split render helpers to keep render() under 200 lines
  void renderNoPet() const;
  void renderDead() const;
  void renderAlive() const;

  // Draw a labelled stat bar at (x, y) with given width
  void drawStatBar(int x, int y, int barWidth, const char* label, uint8_t value) const;

  // Draw today's missions panel at (x, y)
  void drawMissions(int x, int y, int width) const;
};
