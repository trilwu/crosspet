#pragma once
#include <InputManager.h>

class GfxRenderer;

class Screen {
 protected:
  GfxRenderer& renderer;
  InputManager& inputManager;

 public:
  explicit Screen(GfxRenderer& renderer, InputManager& inputManager) : renderer(renderer), inputManager(inputManager) {}
  virtual ~Screen() = default;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void handleInput() {}
};
