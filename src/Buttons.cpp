#include "Buttons.h"
#include <Arduino.h>

static const uint8_t PINS[NUM_STRIPS] = {BUTTON_PIN_RED, BUTTON_PIN_GREEN, BUTTON_PIN_BLUE,
                                         BUTTON_PIN_YELLOW};

void Buttons::begin() {
  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    // GPIO 34-39 are input-only without internal pull-ups (external resistor required).
    pinMode(PINS[i], PINS[i] >= 34 ? INPUT : INPUT_PULLUP);
    lastRaw_[i] = false;
    stable_[i] = false;
    changedAt_[i] = 0;
  }
  queue_ = 0;
}

void Buttons::update(uint32_t now) {
  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    const bool raw = digitalRead(PINS[i]) == LOW;  // wired to GND: pressed = LOW
    if (raw != lastRaw_[i]) {
      lastRaw_[i] = raw;
      changedAt_[i] = now;
    } else if (raw != stable_[i] && now - changedAt_[i] >= DEBOUNCE_MS) {
      stable_[i] = raw;
      if (raw) queue_ |= (1 << i);
    }
  }
}

int8_t Buttons::pressed() {
  for (uint8_t i = 0; i < NUM_STRIPS; i++) {
    if (queue_ & (1 << i)) {
      queue_ &= ~(1 << i);
      return i;
    }
  }
  return -1;
}

void Buttons::clear() { queue_ = 0; }
