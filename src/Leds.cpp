#include "Leds.h"

namespace Leds {

CRGB strips[NUM_STRIPS][LEDS_PER_STRIP];

void begin() {
  FastLED.addLeds<LED_TYPE, LED_PIN_RED, COLOR_ORDER>(strips[0], LEDS_PER_STRIP);
  FastLED.addLeds<LED_TYPE, LED_PIN_GREEN, COLOR_ORDER>(strips[1], LEDS_PER_STRIP);
  FastLED.addLeds<LED_TYPE, LED_PIN_BLUE, COLOR_ORDER>(strips[2], LEDS_PER_STRIP);
  FastLED.addLeds<LED_TYPE, LED_PIN_YELLOW, COLOR_ORDER>(strips[3], LEDS_PER_STRIP);
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, POWER_LIMIT_MA);
  clear();
  show();
}

void clear() { fillAll(CRGB::Black); }

void fillStrip(uint8_t strip, const CRGB& color) {
  if (strip < NUM_STRIPS) fill_solid(strips[strip], LEDS_PER_STRIP, color);
}

void fillAll(const CRGB& color) {
  for (uint8_t s = 0; s < NUM_STRIPS; s++) fillStrip(s, color);
}

void show() { FastLED.show(); }

CRGB scaled(uint32_t rgb, uint8_t level) {
  CRGB c(rgb);
  c.nscale8_video(level);
  return c;
}

}  // namespace Leds
