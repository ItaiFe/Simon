#pragma once
#include <FastLED.h>
#include "config.h"

namespace Leds {
extern CRGB strips[NUM_STRIPS][LEDS_PER_STRIP];

void begin();
void clear();
void fillStrip(uint8_t strip, const CRGB& color);
void fillAll(const CRGB& color);
void show();
CRGB scaled(uint32_t rgb, uint8_t level);  // color dimmed to level 0-255
}  // namespace Leds
