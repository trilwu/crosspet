#include "Input.h"

#include <esp32-hal-adc.h>

void setupInputPinModes() {
  pinMode(BTN_GPIO1, INPUT);
  pinMode(BTN_GPIO2, INPUT);
  pinMode(BTN_GPIO3, INPUT_PULLUP);  // Power button
}

// Get currently pressed button by reading ADC values (and digital for power
// button)
Button getPressedButton() {
  // Check BTN_GPIO3 (Power button) - digital read
  if (digitalRead(BTN_GPIO3) == LOW) return POWER;

  // Check BTN_GPIO1 (4 buttons on resistor ladder)
  const int btn1 = analogRead(BTN_GPIO1);
  if (btn1 < BTN_RIGHT_VAL + BTN_THRESHOLD) return RIGHT;
  if (btn1 < BTN_LEFT_VAL + BTN_THRESHOLD) return LEFT;
  if (btn1 < BTN_CONFIRM_VAL + BTN_THRESHOLD) return CONFIRM;
  if (btn1 < BTN_BACK_VAL + BTN_THRESHOLD) return BACK;

  // Check BTN_GPIO2 (2 buttons on resistor ladder)
  const int btn2 = analogRead(BTN_GPIO2);
  if (btn2 < BTN_VOLUME_DOWN_VAL + BTN_THRESHOLD) return VOLUME_DOWN;
  if (btn2 < BTN_VOLUME_UP_VAL + BTN_THRESHOLD) return VOLUME_UP;

  return NONE;
}

Input getInput(const long maxHoldMs) {
  const Button button = getPressedButton();
  if (button == NONE) return {NONE, 0};

  const auto start = millis();
  unsigned long held = 0;
  while (getPressedButton() == button && (maxHoldMs < 0 || held < maxHoldMs)) {
    delay(50);
    held = millis() - start;
  }
  return {button, held};
}
