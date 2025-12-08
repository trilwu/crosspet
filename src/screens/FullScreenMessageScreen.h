#pragma once
#include <EInkDisplay.h>
#include <EpdFontFamily.h>

#include <string>
#include <utility>

#include "Screen.h"

class FullScreenMessageScreen final : public Screen {
  std::string text;
  EpdFontStyle style;
  EInkDisplay::RefreshMode refreshMode;

 public:
  explicit FullScreenMessageScreen(GfxRenderer& renderer, InputManager& inputManager, std::string text,
                                   const EpdFontStyle style = REGULAR,
                                   const EInkDisplay::RefreshMode refreshMode = EInkDisplay::FAST_REFRESH)
      : Screen(renderer, inputManager), text(std::move(text)), style(style), refreshMode(refreshMode) {}
  void onEnter() override;
};
