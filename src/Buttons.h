#pragma once
#include <stdint.h>
#include "config.h"

// Debounced buttons. Each new press is queued (one slot per button) until read,
// so simultaneous presses are all delivered, lowest index first.
class Buttons {
 public:
  void begin();
  void update(uint32_t now);  // call every loop iteration
  int8_t pressed();           // next queued press (0-3), or -1
  void clear();               // drop queued presses

 private:
  bool lastRaw_[NUM_STRIPS];
  bool stable_[NUM_STRIPS];
  uint32_t changedAt_[NUM_STRIPS];
  uint8_t queue_ = 0;
};
