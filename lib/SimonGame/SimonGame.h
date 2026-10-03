#pragma once
#include <stdint.h>

// Pure Simon rules: no Arduino/FastLED dependencies so it can be unit-tested on the host.

struct SimonConfig {
  uint8_t startLen;  // colors in round 1
  uint8_t maxLen;    // colors in the final round
  uint16_t onMsFirst, onMsLast;
  uint16_t gapMsFirst, gapMsLast;
  uint16_t timeoutMsFirst, timeoutMsLast;
};

struct SimonTiming {
  uint16_t onMs;       // how long one color lights during playback
  uint16_t gapMs;      // dark time between colors during playback
  uint16_t timeoutMs;  // max wait for each press
};

enum class PressResult : uint8_t { Correct, RoundComplete, Won, Wrong };

class SimonGame {
 public:
  static const uint8_t kNumColors = 4;
  static const uint8_t kMaxCapacity = 64;

  explicit SimonGame(const SimonConfig& cfg);

  void start(uint32_t seed);
  uint8_t round() const { return round_; }
  uint8_t totalRounds() const { return cfg_.maxLen - cfg_.startLen + 1; }
  uint8_t sequenceLength() const { return cfg_.startLen + round_ - 1; }
  uint8_t colorAt(uint8_t i) const { return i < cfg_.maxLen ? seq_[i] : 0; }
  uint8_t expectedColor() const { return colorAt(inputPos_); }
  PressResult press(uint8_t color);
  void nextRound();
  SimonTiming timing() const;

 private:
  SimonConfig cfg_;
  uint8_t seq_[kMaxCapacity];
  uint8_t round_;
  uint8_t inputPos_;
  bool over_;
};
