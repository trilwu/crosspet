#pragma once
#include <string>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

// On-device conference badge display and editor.
// VIEWING: shows badge; Confirm → enter EDITING mode.
// EDITING: Up/Down selects Name/Title/QR field; Confirm → opens keyboard; Back → return to VIEWING.
class ConferenceBadgeActivity final : public Activity {
 public:
  explicit ConferenceBadgeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Badge", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  // No preventAutoSleep — static display, device should sleep normally

 private:
  enum class ViewMode { VIEWING, EDITING };

  static constexpr int EDIT_FIELD_COUNT = 3;
  static constexpr int FIELD_NAME = 0;
  static constexpr int FIELD_TITLE = 1;
  static constexpr int FIELD_QR = 2;

  std::string badgeName;
  std::string badgeTitle;
  std::string qrData;
  bool fileLoaded = false;

  ViewMode mode = ViewMode::VIEWING;
  int editSelected = 0;
  ButtonNavigator buttonNavigator;

  void loadFromFile();
  void saveToFile();
  void openFieldEditor(int field);
  void renderViewing() const;
  void renderEditing() const;
};
