#pragma once
#include <stdint.h>

// Frame-based effects. Each call draws one frame into Leds::strips (never calls show()).
// t = ms since the effect started, now = millis(). Returns true once the effect has finished.
namespace Animations {
bool boot(uint32_t t, uint32_t now);
void idle(uint32_t now);
bool getReady(uint32_t t);
bool pulse(uint8_t strip, uint32_t t, uint32_t durationMs);
bool roundCleared(uint32_t t);
bool gameOver(uint32_t t, uint8_t correctStrip);
bool victory(uint32_t t, uint32_t now);
void otaProgress(float fraction);
void otaResult(bool ok);
}  // namespace Animations
