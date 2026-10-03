#pragma once
#include <stdint.h>

// Simon -> Flamingo link (pure logic, unit-tested on the host).
// Packets are [simonId, value]; see docs/superpowers/specs/2026-10-03-flamingo-pairing-design.md.
namespace FlamingoValue {
const uint8_t kDark = 0x00;
const uint8_t kPink = 0x20;
const uint8_t kWin = 0xFE;
const uint8_t kUnpair = 0xFF;

// Simon color index (0 red, 1 green, 2 blue, 3 yellow) -> station button bit; anything else -> dark.
inline uint8_t forColor(int8_t color) {
  return (color >= 0 && color < 4) ? (uint8_t)(1u << color) : kDark;
}
}  // namespace FlamingoValue

struct FlamingoLinkConfig {
  uint8_t simonId;
  uint32_t keepaliveMs;
  uint8_t repeats;  // how many times win/unpair are sent
};

class FlamingoLink {
 public:
  static const uint8_t kPacketSize = 2;

  explicit FlamingoLink(const FlamingoLinkConfig& cfg);

  void pair();
  bool paired() const { return paired_; }
  void setValue(uint8_t value);
  void endWithUnpair();
  void endWithWin();
  bool poll(uint32_t now, uint8_t out[kPacketSize]);

 private:
  void end(uint8_t value);

  FlamingoLinkConfig cfg_;
  bool paired_;
  bool sentOnce_;
  uint8_t value_;
  uint8_t lastSent_;
  uint32_t lastSentAt_;
  uint8_t pendingEnd_;
  uint8_t endValue_;
};
