#include "SimonGame.h"

namespace {

uint32_t xorshift32(uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

uint16_t lerp(uint16_t first, uint16_t last, int32_t num, int32_t den) {
  if (den <= 0) return first;
  return (uint16_t)((int32_t)first + ((int32_t)last - (int32_t)first) * num / den);
}

}  // namespace

SimonGame::SimonGame(const SimonConfig& cfg) : cfg_(cfg), round_(1), inputPos_(0), over_(true) {
  if (cfg_.maxLen > kMaxCapacity) cfg_.maxLen = kMaxCapacity;
  if (cfg_.maxLen < 1) cfg_.maxLen = 1;
  if (cfg_.startLen < 1) cfg_.startLen = 1;
  if (cfg_.startLen > cfg_.maxLen) cfg_.startLen = cfg_.maxLen;
  for (uint8_t i = 0; i < kMaxCapacity; i++) seq_[i] = 0;
}

void SimonGame::start(uint32_t seed) {
  uint32_t s = seed ? seed : 0x9E3779B9u;  // xorshift gets stuck on 0
  for (uint8_t i = 0; i < cfg_.maxLen; i++) seq_[i] = (xorshift32(s) >> 16) % kNumColors;
  round_ = 1;
  inputPos_ = 0;
  over_ = false;
}

PressResult SimonGame::press(uint8_t color) {
  if (over_ || inputPos_ >= sequenceLength()) return PressResult::Wrong;
  if (color != seq_[inputPos_]) {
    over_ = true;
    return PressResult::Wrong;
  }
  inputPos_++;
  if (inputPos_ < sequenceLength()) return PressResult::Correct;
  if (round_ >= totalRounds()) {
    over_ = true;
    return PressResult::Won;
  }
  return PressResult::RoundComplete;
}

void SimonGame::nextRound() {
  if (round_ < totalRounds()) round_++;
  inputPos_ = 0;
}

SimonTiming SimonGame::timing() const {
  const int32_t num = round_ - 1;
  const int32_t den = totalRounds() - 1;
  SimonTiming t;
  t.onMs = lerp(cfg_.onMsFirst, cfg_.onMsLast, num, den);
  t.gapMs = lerp(cfg_.gapMsFirst, cfg_.gapMsLast, num, den);
  t.timeoutMs = lerp(cfg_.timeoutMsFirst, cfg_.timeoutMsLast, num, den);
  return t;
}
