#include <Arduino.h>
#include "Animations.h"
#include "Buttons.h"
#include "Leds.h"
#include "Log.h"
#include "Net.h"
#include "config.h"

static Buttons buttons;
static uint32_t bootStart = 0;
static uint32_t lastFrame = 0;
static bool booted = false;

static void onOtaProgress(float fraction) {
  Animations::otaProgress(fraction);
  Leds::show();
}

static void onOtaEnd(bool ok) {
  Animations::otaResult(ok);
  Leds::show();
  if (!ok) delay(1000);  // show the red result before returning to idle
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  LOG("Simon booting");
  Leds::begin();
  buttons.begin();
  Net::begin(onOtaProgress, onOtaEnd);
  bootStart = millis();
}

void loop() {
  const uint32_t now = millis();
  Net::update(now);
  buttons.update(now);
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  const int8_t p = buttons.pressed();
  if (p >= 0) LOG("Button %d pressed", p);
  if (!booted) booted = Animations::boot(now - bootStart, now);
  else Animations::idle(now);
  Leds::show();
}
