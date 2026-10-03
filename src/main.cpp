#include <Arduino.h>
#include "Animations.h"
#include "Buttons.h"
#include "Leds.h"
#include "Log.h"
#include "config.h"

static Buttons buttons;
static uint32_t bootStart = 0;
static uint32_t lastFrame = 0;
static bool booted = false;

void setup() {
  Serial.begin(SERIAL_BAUD);
  LOG("Simon booting");
  Leds::begin();
  buttons.begin();
  bootStart = millis();
}

void loop() {
  const uint32_t now = millis();
  buttons.update(now);
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  const int8_t p = buttons.pressed();
  if (p >= 0) LOG("Button %d pressed", p);
  if (!booted) booted = Animations::boot(now - bootStart, now);
  else Animations::idle(now);
  Leds::show();
}
