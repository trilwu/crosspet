#pragma once
#include "Screen.h"

class BootLogoScreen final : public Screen {
 public:
  explicit BootLogoScreen(GfxRenderer& renderer, InputManager& inputManager) : Screen(renderer, inputManager) {}
  void onEnter() override;
};
