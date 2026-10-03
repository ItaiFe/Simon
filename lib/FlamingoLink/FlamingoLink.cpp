#include "FlamingoLink.h"

FlamingoLink::FlamingoLink(const FlamingoLinkConfig& cfg)
    : cfg_(cfg),
      paired_(false),
      sentOnce_(false),
      value_(FlamingoValue::kDark),
      lastSent_(FlamingoValue::kDark),
      lastSentAt_(0),
      pendingEnd_(0),
      endValue_(FlamingoValue::kUnpair) {}

void FlamingoLink::pair() {
  paired_ = true;
  sentOnce_ = false;
  pendingEnd_ = 0;
  value_ = FlamingoValue::kPink;
}

void FlamingoLink::setValue(uint8_t value) {
  if (paired_) value_ = value;
}

void FlamingoLink::endWithUnpair() { end(FlamingoValue::kUnpair); }

void FlamingoLink::endWithWin() { end(FlamingoValue::kWin); }

void FlamingoLink::end(uint8_t value) {
  if (!paired_) return;
  paired_ = false;
  endValue_ = value;
  pendingEnd_ = cfg_.repeats;
}

bool FlamingoLink::poll(uint32_t now, uint8_t out[kPacketSize]) {
  out[0] = cfg_.simonId;
  if (pendingEnd_ > 0) {
    pendingEnd_--;
    out[1] = endValue_;
    return true;
  }
  if (!paired_) return false;
  if (sentOnce_ && value_ == lastSent_ && now - lastSentAt_ < cfg_.keepaliveMs) return false;
  out[1] = value_;
  lastSent_ = value_;
  lastSentAt_ = now;
  sentOnce_ = true;
  return true;
}
