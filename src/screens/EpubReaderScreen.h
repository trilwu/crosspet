#pragma once
#include <Epub.h>
#include <Epub/Section.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "Screen.h"

class EpubReaderScreen final : public Screen {
  Epub* epub;
  Section* section = nullptr;
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t sectionMutex = nullptr;
  int currentSpineIndex = 0;
  int nextPageNumber = 0;
  bool updateRequired = false;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderPage();
  void renderStatusBar() const;

 public:
  explicit EpubReaderScreen(EpdRenderer* renderer, Epub* epub) : Screen(renderer), epub(epub) {}
  ~EpubReaderScreen() override { free(section); }
  void onEnter() override;
  void onExit() override;
  void handleInput(Input input) override;
};
