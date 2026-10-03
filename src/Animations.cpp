#include "Animations.h"
#include <math.h>
#include "Leds.h"
#include "config.h"

namespace {

const float TWO_PI_F = 6.2831853f;
uint8_t sparkle[NUM_STRIPS][LEDS_PER_STRIP];
uint8_t sparkleHue[NUM_STRIPS][LEDS_PER_STRIP];

// True during the "on" half of a blink with the given on/off time.
bool blinkOn(uint32_t t, uint32_t halfPeriodMs) { return (t / halfPeriodMs) % 2 == 0; }

}  // namespace

namespace Animations {

void idle(uint32_t now) {
  const float breathPhase = TWO_PI_F * (float)(now % IDLE_BREATH_PERIOD_MS) / IDLE_BREATH_PERIOD_MS;
  const float shimmerPhase =
      TWO_PI_F * (float)(now % IDLE_SHIMMER_PERIOD_MS) / IDLE_SHIMMER_PERIOD_MS;
  const uint32_t travelled = (uint64_t)now * IDLE_COMET_SPEED / 1000;  // LEDs of travel since boot
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    // Each strip breathes a quarter-cycle behind the previous one, so the glow rotates.
    const float breath = 0.5f + 0.5f * sinf(breathPhase - s * (TWO_PI_F / NUM_STRIPS));
    // Comets repeat every IDLE_COMET_SPACING LEDs; each strip is offset so they don't move in lockstep.
    const uint32_t cometHead = (travelled + s * IDLE_COMET_SPACING / NUM_STRIPS) % IDLE_COMET_SPACING;
    if (random8() < IDLE_SPARKLE_CHANCE) {
      const uint8_t i = random8(LEDS_PER_STRIP);
      sparkle[s][i] = IDLE_SPARKLE_LEVEL;
      sparkleHue[s][i] = random8();
    }
    for (uint8_t i = 0; i < LEDS_PER_STRIP; i++) {
      const float shimmer = 0.5f + 0.5f * sinf(shimmerPhase + i * IDLE_SHIMMER_SPACING);
      const float level = IDLE_MIN_LEVEL + (1.0f - IDLE_MIN_LEVEL) * breath *
                                               (1.0f - IDLE_SHIMMER_DEPTH + IDLE_SHIMMER_DEPTH * shimmer);
      CRGB c = Leds::scaled(STRIP_COLORS[s], (uint8_t)(level * IDLE_MAX_BRIGHTNESS));
      const uint8_t behind = (cometHead + IDLE_COMET_SPACING - i % IDLE_COMET_SPACING) % IDLE_COMET_SPACING;
      if (behind < IDLE_COMET_TAIL) {
        c += Leds::scaled(STRIP_COLORS[s], 255 - behind * 255 / IDLE_COMET_TAIL);
        if (behind == 0) c += CRGB(IDLE_COMET_HEAD_WHITE, IDLE_COMET_HEAD_WHITE, IDLE_COMET_HEAD_WHITE);
      }
      const uint8_t sp = sparkle[s][i];
      if (sp) c += CHSV(sparkleHue[s][i], IDLE_SPARKLE_SATURATION, sp);
      Leds::strips[s][i] = c;
      sparkle[s][i] = qsub8(sp, IDLE_SPARKLE_DECAY);
    }
  }
}

bool boot(uint32_t t, uint32_t now) {
  const uint32_t sweepEnd = BOOT_SWEEP_MS_PER_STRIP * NUM_STRIPS;
  const uint32_t whiteEnd = sweepEnd + BOOT_WHITE_MS;
  const uint32_t fadeEnd = whiteEnd + BOOT_FADE_MS;
  const CRGB white(BOOT_WHITE_LEVEL, BOOT_WHITE_LEVEL, BOOT_WHITE_LEVEL);

  if (t < sweepEnd) {
    Leds::clear();
    const uint8_t current = t / BOOT_SWEEP_MS_PER_STRIP;
    const uint32_t within = t % BOOT_SWEEP_MS_PER_STRIP;
    for (uint8_t s = 0; s < current; s++) Leds::fillStrip(s, CRGB(STRIP_COLORS[s]));
    const uint32_t lit = within * LEDS_PER_STRIP / BOOT_SWEEP_MS_PER_STRIP + 1;
    for (uint8_t i = 0; i < lit && i < LEDS_PER_STRIP; i++)
      Leds::strips[current][i] = CRGB(STRIP_COLORS[current]);
    return false;
  }
  if (t < whiteEnd) {
    Leds::fillAll(white);
    return false;
  }
  idle(now);
  if (t < fadeEnd) {
    const uint8_t whiteAmount = 255 - (t - whiteEnd) * 255 / BOOT_FADE_MS;
    for (uint8_t s = 0; s < NUM_STRIPS; s++)
      for (uint8_t i = 0; i < LEDS_PER_STRIP; i++) nblend(Leds::strips[s][i], white, whiteAmount);
    return false;
  }
  return true;
}

bool getReady(uint32_t t) {
  Leds::clear();
  if (t >= GET_READY_MS) return true;
  const uint8_t level = 255 - t * 255 / GET_READY_MS;
  for (uint8_t s = 0; s < NUM_STRIPS; s++) Leds::fillStrip(s, Leds::scaled(STRIP_COLORS[s], level));
  return false;
}

bool pulse(uint8_t strip, uint32_t t, uint32_t durationMs) {
  Leds::clear();
  if (t >= durationMs) return true;
  const uint32_t fadeStart = durationMs * (100 - PULSE_FADE_PERCENT) / 100;
  uint8_t level = 255;
  if (t > fadeStart) level = 255 * (durationMs - t) / (durationMs - fadeStart);
  Leds::fillStrip(strip, Leds::scaled(STRIP_COLORS[strip], level));
  return false;
}

bool roundCleared(uint32_t t) {
  Leds::clear();
  if (t >= ROUND_CLEARED_MS) return true;
  const uint32_t half = ROUND_CLEARED_MS / 2;
  const uint8_t level = t < half ? t * 255 / half : (ROUND_CLEARED_MS - t) * 255 / half;
  Leds::fillAll(Leds::scaled(ROUND_CLEARED_COLOR, level));
  return false;
}

bool gameOver(uint32_t t, uint8_t correctStrip) {
  const uint32_t flashEnd = GAME_OVER_FLASHES * 2 * GAME_OVER_FLASH_MS;
  const uint32_t blinkEnd = flashEnd + GAME_OVER_BLINKS * 2 * GAME_OVER_BLINK_MS;
  Leds::clear();
  if (t < flashEnd) {
    if (gameOverRedOn(t)) Leds::fillAll(CRGB(GAME_OVER_COLOR));
    return false;
  }
  if (t < blinkEnd) {
    if (blinkOn(t - flashEnd, GAME_OVER_BLINK_MS))
      Leds::fillStrip(correctStrip, CRGB(STRIP_COLORS[correctStrip]));
    return false;
  }
  return true;
}

bool victory(uint32_t t, uint32_t now) {
  const uint32_t chaseEnd = VICTORY_CHASE_MS;
  const uint32_t confettiEnd = chaseEnd + VICTORY_CONFETTI_MS;
  const uint32_t flashEnd = confettiEnd + VICTORY_FLASHES * 2 * VICTORY_FLASH_MS;
  const uint32_t fadeEnd = flashEnd + VICTORY_FADE_MS;

  if (t < chaseEnd) {
    const uint8_t head = (t / VICTORY_CHASE_STEP_MS) % NUM_STRIPS;
    for (uint8_t s = 0; s < NUM_STRIPS; s++) {
      const uint8_t dist = (head + NUM_STRIPS - s) % NUM_STRIPS;
      Leds::fillStrip(s, Leds::scaled(STRIP_COLORS[s], VICTORY_CHASE_TRAIL[dist]));
    }
    return false;
  }
  if (t < confettiEnd) {
    for (uint8_t s = 0; s < NUM_STRIPS; s++)
      fadeToBlackBy(Leds::strips[s], LEDS_PER_STRIP, VICTORY_CONFETTI_FADE);
    for (uint8_t n = 0; n < VICTORY_CONFETTI_PER_FRAME; n++)
      Leds::strips[random8(NUM_STRIPS)][random8(LEDS_PER_STRIP)] = CHSV(random8(), VICTORY_CONFETTI_SATURATION, 255);
    return false;
  }
  if (t < flashEnd) {
    Leds::clear();
    if (blinkOn(t - confettiEnd, VICTORY_FLASH_MS)) Leds::fillAll(CRGB::White);
    return false;
  }
  idle(now);
  if (t < fadeEnd) {
    const uint8_t level = (t - flashEnd) * 255 / VICTORY_FADE_MS;
    for (uint8_t s = 0; s < NUM_STRIPS; s++) nscale8_video(Leds::strips[s], LEDS_PER_STRIP, level);
    return false;
  }
  return true;
}

bool gameOverRedOn(uint32_t t) {
  return t < GAME_OVER_FLASHES * 2 * GAME_OVER_FLASH_MS && blinkOn(t, GAME_OVER_FLASH_MS);
}

bool pairFlashOn(uint32_t t) {
  return t < PAIR_FLASHES * 2 * PAIR_FLASH_MS && blinkOn(t, PAIR_FLASH_MS);
}

void pairHold(float fraction, uint32_t now) {
  idle(now);
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  const uint8_t lit = (uint8_t)(fraction * LEDS_PER_STRIP);
  for (uint8_t s = 0; s < NUM_STRIPS; s++)
    for (uint8_t i = 0; i < lit; i++) Leds::strips[s][i] = CRGB(PAIR_COLOR);
}

bool pairFlash(uint32_t t) {
  Leds::clear();
  if (t >= PAIR_FLASHES * 2 * PAIR_FLASH_MS) return true;
  if (pairFlashOn(t)) Leds::fillAll(CRGB(PAIR_COLOR));
  return false;
}

void otaProgress(float fraction) {
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  const float lit = fraction * LEDS_PER_STRIP;
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    for (uint8_t i = 0; i < LEDS_PER_STRIP; i++) {
      float level = lit - i;  // partial brightness on the leading LED
      if (level < 0) level = 0;
      if (level > 1) level = 1;
      Leds::strips[s][i] = Leds::scaled(OTA_PROGRESS_COLOR, (uint8_t)(level * OTA_PROGRESS_LEVEL));
    }
  }
}

void otaResult(bool ok) { Leds::fillAll(ok ? CRGB::Green : CRGB::Red); }

}  // namespace Animations
