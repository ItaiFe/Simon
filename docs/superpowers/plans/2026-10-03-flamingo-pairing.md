# Simon ↔ Flamingo Pairing + Ethernet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Hold all 4 Simon buttons for 2 s to pair with the Flamingo; the Flamingo then mirrors Simon's colours until a failure (unpair) or win (party, then unpair). Simon also gets Ethernet with WiFi fallback.

**Architecture:** A pure `FlamingoLink` library decides which 2-byte UDP packet (`[5, value]`) to send each frame; `src/Flamingo` owns the socket and host lookup; `main.cpp` gains `PairHold`/`PairFlash` states and computes a mirror value per frame. `Net` brings up LAN8720 Ethernet first and turns WiFi off while the cable is up. The Flamingo firmware (separate repo) gets a `PLAN_SIMON` that renders Simon's value instantly and ignores stations while active.

**Tech Stack:** PlatformIO, Arduino-ESP32 core 2.0.17 (espressif32 @ 6.10.0), FastLED 3.9.16, WiFiUDP, ESPmDNS, ETH (LAN8720), Unity native tests.

**Spec:** `docs/superpowers/specs/2026-10-03-flamingo-pairing-design.md` (builds on `docs/superpowers/specs/2026-10-03-esp32-simon-design.md`)

## Global Constraints

- Packet = 2 bytes `[5, value]` to `flamingo-esp32.local` UDP 5000. Values: `0x00` dark, `0x01` red, `0x02` green, `0x04` blue, `0x08` yellow, `0x20` pink, `0xFE` win, `0xFF` unpair.
- Keepalive 100 ms while paired; `0xFE`/`0xFF` sent 3 times; Flamingo unpairs after 2 s without ID-5 packets.
- Pairing = all 4 buttons held for 2000 ms in IDLE; normal games start on release.
- Simon must stay fully playable with the Flamingo off/unreachable or on old firmware.
- Every tunable lives in `include/config.h`; `loop()` stays non-blocking except bounded host lookups (≤ `FLAMINGO_RESOLVE_TIMEOUT_MS`) done only in IDLE or at pairing.
- Simon pins unchanged: buttons 12/15/2/5, strips 4/14/32/33. Ethernet: PHY addr 1, power GPIO16, MDC 23, MDIO 18, `ETH_CLOCK_GPIO0_IN` (same as the Flamingo, which runs on the same board without GPIO5).
- `lib/` code has no Arduino/FastLED includes; C++11.
- Flamingo work happens on branch `simon-pairing` in `~/gits/Flamingods`; never stage the unrelated pre-existing change `esps/test/platformio.ini`; the user flashes the Flamingo.
- Upload firmware at most once per request (user rule); this plan only builds.

## Review Focus

1. **Flamingo off / unreachable** → Simon never freezes mid-game: lookups only in IDLE (every 10 s) or at pairing, each ≤ 300 ms; packets are silently skipped. README checklist (Task 6).
2. **Partial hold** (3 buttons, or releasing at 1.9 s) → no pairing and no game start; a single press-and-release still starts a normal game. README checklist (Task 6).
3. **Simon reboots / loses power while paired** → Flamingo returns to its rainbow within 2 s (timeout). README checklist (Task 6).
4. **A station is pressed during a paired game** → Flamingo keeps showing Simon only; after unpair, stations work normally with no stuck MIXED colour. README checklist (Task 6).
5. **Ethernet cable pulled / plugged while running** → WiFi takes over / turns off; OTA, `simon.local` and the Flamingo link keep working. README checklist (Task 6).

---

## File Map

| File | Change |
|---|---|
| `lib/FlamingoLink/FlamingoLink.{h,cpp}` | New: pure packet/keepalive/repeat logic |
| `test/test_flamingo/test_main.cpp` | New: Unity tests for FlamingoLink |
| `include/config.h` | Flamingo, pairing and Ethernet tunables |
| `src/Flamingo.{h,cpp}` | New: UDP socket, host lookup, sends link packets |
| `src/Buttons.h` | `held()`, `allHeld()`, `anyHeld()` |
| `src/Animations.{h,cpp}` | `pairHold`, `pairFlash`, `pairFlashOn`, `gameOverRedOn` |
| `src/main.cpp` | `PairHold`/`PairFlash` states, start-on-release, mirroring |
| `src/Net.{h,cpp}` | Ethernet first, WiFi fallback, `Net::connected()` |
| `README.md` | Pairing, Ethernet, checklist |
| `~/gits/Flamingods/esps/flamingo/src/main.cpp` | `PLAN_SIMON`, `handleSimon`, drain UDP |

---

### Task 1: FlamingoLink library (TDD)

**Files:**
- Create: `lib/FlamingoLink/FlamingoLink.h`, `lib/FlamingoLink/FlamingoLink.cpp`
- Test: `test/test_flamingo/test_main.cpp`

**Interfaces:**
- Produces (used by Tasks 2–3):
  ```cpp
  namespace FlamingoValue { const uint8_t kDark=0x00, kPink=0x20, kWin=0xFE, kUnpair=0xFF;
                            uint8_t forColor(int8_t color); }  // 0..3 -> 1<<color, else kDark
  struct FlamingoLinkConfig { uint8_t simonId; uint32_t keepaliveMs; uint8_t repeats; };
  class FlamingoLink {
    static const uint8_t kPacketSize = 2;
    explicit FlamingoLink(const FlamingoLinkConfig&);
    void pair();                 // paired, value = kPink, cancels pending end packets
    bool paired() const;
    void setValue(uint8_t v);    // ignored when not paired
    void endWithUnpair();        // unpaired; next `repeats` polls send kUnpair (no-op if not paired)
    void endWithWin();           // unpaired; next `repeats` polls send kWin (no-op if not paired)
    bool poll(uint32_t now, uint8_t out[kPacketSize]);  // true = send out now
  };
  ```

- [ ] **Step 1: Write the failing tests** — `test/test_flamingo/test_main.cpp`

```cpp
#include <FlamingoLink.h>
#include <unity.h>

static FlamingoLinkConfig cfg() {
  FlamingoLinkConfig c;
  c.simonId = 5;
  c.keepaliveMs = 100;
  c.repeats = 3;
  return c;
}

void setUp() {}
void tearDown() {}

void test_unpaired_sends_nothing() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  TEST_ASSERT_FALSE(link.paired());
  TEST_ASSERT_FALSE(link.poll(0, out));
  TEST_ASSERT_FALSE(link.poll(1000, out));
}

void test_pair_sends_pink_immediately() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  TEST_ASSERT_TRUE(link.paired());
  TEST_ASSERT_TRUE(link.poll(10, out));
  TEST_ASSERT_EQUAL_HEX8(5, out[0]);
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kPink, out[1]);
}

void test_same_value_not_resent_before_keepalive() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.poll(0, out);
  TEST_ASSERT_FALSE(link.poll(16, out));
  TEST_ASSERT_FALSE(link.poll(99, out));
}

void test_keepalive_resends_after_interval() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.poll(0, out);
  TEST_ASSERT_TRUE(link.poll(100, out));
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kPink, out[1]);
}

void test_value_change_sends_immediately() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.poll(0, out);
  link.setValue(0x04);
  TEST_ASSERT_TRUE(link.poll(5, out));
  TEST_ASSERT_EQUAL_HEX8(0x04, out[1]);
  TEST_ASSERT_FALSE(link.poll(10, out));
}

void test_set_value_ignored_when_unpaired() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.setValue(0x01);
  TEST_ASSERT_FALSE(link.poll(0, out));
}

void test_unpair_sends_exactly_repeats_then_stops() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.poll(0, out);
  link.endWithUnpair();
  TEST_ASSERT_FALSE(link.paired());
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_TRUE(link.poll(20 + i, out));
    TEST_ASSERT_EQUAL_HEX8(5, out[0]);
    TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kUnpair, out[1]);
  }
  TEST_ASSERT_FALSE(link.poll(30, out));
  TEST_ASSERT_FALSE(link.poll(500, out));
}

void test_win_sends_exactly_repeats_then_stops() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.endWithWin();
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_TRUE(link.poll(i, out));
    TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kWin, out[1]);
  }
  TEST_ASSERT_FALSE(link.poll(10, out));
}

void test_end_when_not_paired_sends_nothing() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.endWithUnpair();
  link.endWithWin();
  TEST_ASSERT_FALSE(link.poll(0, out));
}

void test_repair_after_end_starts_fresh() {
  FlamingoLink link(cfg());
  uint8_t out[2];
  link.pair();
  link.endWithUnpair();
  link.pair();
  TEST_ASSERT_TRUE(link.poll(0, out));
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kPink, out[1]);
}

void test_for_color_maps_to_station_bits() {
  TEST_ASSERT_EQUAL_HEX8(0x01, FlamingoValue::forColor(0));
  TEST_ASSERT_EQUAL_HEX8(0x02, FlamingoValue::forColor(1));
  TEST_ASSERT_EQUAL_HEX8(0x04, FlamingoValue::forColor(2));
  TEST_ASSERT_EQUAL_HEX8(0x08, FlamingoValue::forColor(3));
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kDark, FlamingoValue::forColor(-1));
  TEST_ASSERT_EQUAL_HEX8(FlamingoValue::kDark, FlamingoValue::forColor(4));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_unpaired_sends_nothing);
  RUN_TEST(test_pair_sends_pink_immediately);
  RUN_TEST(test_same_value_not_resent_before_keepalive);
  RUN_TEST(test_keepalive_resends_after_interval);
  RUN_TEST(test_value_change_sends_immediately);
  RUN_TEST(test_set_value_ignored_when_unpaired);
  RUN_TEST(test_unpair_sends_exactly_repeats_then_stops);
  RUN_TEST(test_win_sends_exactly_repeats_then_stops);
  RUN_TEST(test_end_when_not_paired_sends_nothing);
  RUN_TEST(test_repair_after_end_starts_fresh);
  RUN_TEST(test_for_color_maps_to_station_bits);
  return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `pio test -e native -f test_flamingo`
Expected: build FAIL — `FlamingoLink.h: No such file or directory`.

- [ ] **Step 3: Implement** — `lib/FlamingoLink/FlamingoLink.h`

```cpp
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
```

`lib/FlamingoLink/FlamingoLink.cpp`

```cpp
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
```

- [ ] **Step 4: Run to verify it passes**

Run: `pio test -e native`
Expected: `test_flamingo` 11/11 and `test_simon` 16/16 succeeded (27 test cases).

- [ ] **Step 5: Commit**

```bash
git add lib/FlamingoLink test/test_flamingo
git commit -m "feat: add FlamingoLink packet logic with native tests"
```

---

### Task 2: Config + Flamingo network module

**Files:**
- Modify: `include/config.h` (new sections + static_asserts)
- Create: `src/Flamingo.h`, `src/Flamingo.cpp`
- Modify: `src/Net.h`, `src/Net.cpp` (add `Net::connected()` only; Ethernet comes in Task 4)

**Interfaces:**
- Consumes: `FlamingoLink`, `FlamingoLinkConfig` (Task 1); `LOG` (`src/Log.h`).
- Produces (used by Task 3):
  ```cpp
  namespace Flamingo {
    FlamingoLink& link();
    void refresh();  // request a fresh host lookup on the next update()
    void update(uint32_t now, bool networkUp, bool allowLookup);  // once per frame
  }
  bool Net::connected();
  ```
  config: `FLAMINGO_HOST`, `FLAMINGO_PORT`, `FLAMINGO_SIMON_ID`, `FLAMINGO_KEEPALIVE_MS`, `FLAMINGO_REPEATS`, `FLAMINGO_RESOLVE_RETRY_MS`, `FLAMINGO_RESOLVE_TIMEOUT_MS`, `PAIR_HOLD_MS`, `PAIR_COLOR`, `PAIR_FLASHES`, `PAIR_FLASH_MS`, `ETH_ENABLED`, `ETH01_PHY_ADDR`, `ETH01_PHY_POWER_PIN`, `ETH01_MDC_PIN`, `ETH01_MDIO_PIN`.

- [ ] **Step 1: Add config** — in `include/config.h`, insert before the line `// ---------- Network / system ----------`:

```cpp
// ---------- Flamingo pairing (hold all 4 buttons in idle) ----------
#define FLAMINGO_HOST "flamingo-esp32.local"         // mDNS name or IP address of the Flamingo
constexpr uint16_t FLAMINGO_PORT              = 5000;
constexpr uint8_t  FLAMINGO_SIMON_ID          = 5;      // stations are 1-4
constexpr uint32_t FLAMINGO_KEEPALIVE_MS      = 100;    // resend current colour this often while paired
constexpr uint8_t  FLAMINGO_REPEATS           = 3;      // win/unpair packets are sent this many times
constexpr uint32_t FLAMINGO_RESOLVE_RETRY_MS  = 10000;  // idle-only lookup retry while unresolved
constexpr uint32_t FLAMINGO_RESOLVE_TIMEOUT_MS = 300;   // max time one lookup may block
constexpr uint32_t PAIR_HOLD_MS               = 2000;   // hold all 4 buttons this long to pair
constexpr uint32_t PAIR_COLOR                 = 0xFF1493;  // pink
constexpr uint8_t  PAIR_FLASHES               = 2;      // pink flashes when paired
constexpr uint32_t PAIR_FLASH_MS              = 200;    // on time (off time is the same)

// ---------- Ethernet (WT32-ETH01 / ESP32-ETH01, LAN8720) ----------
constexpr bool ETH_ENABLED         = true;   // Ethernet first; WiFi only while the cable is down
constexpr uint8_t ETH01_PHY_ADDR   = 1;
constexpr int ETH01_PHY_POWER_PIN  = 16;
constexpr int ETH01_MDC_PIN        = 23;
constexpr int ETH01_MDIO_PIN       = 18;

```

Append to the sanity-check block at the end of `include/config.h`:

```cpp
static_assert(FLAMINGO_KEEPALIVE_MS > 0 && PAIR_HOLD_MS > 0 && PAIR_FLASH_MS > 0,
              "Flamingo/pairing timings must be > 0");
static_assert(FLAMINGO_SIMON_ID > 4, "FLAMINGO_SIMON_ID must not collide with stations 1-4");
```

- [ ] **Step 2: Add `Net::connected()`** — in `src/Net.h` add inside the namespace after `void update(uint32_t now);`:

```cpp
bool connected();  // true when any network interface has an IP
```

In `src/Net.cpp` append:

```cpp
bool Net::connected() { return WiFi.status() == WL_CONNECTED; }
```

- [ ] **Step 3: Create `src/Flamingo.h`**

```cpp
#pragma once
#include <FlamingoLink.h>
#include <stdint.h>

// Sends FlamingoLink packets to the Flamingo over UDP. Never waits for a reply.
namespace Flamingo {
FlamingoLink& link();
void refresh();  // request a fresh host lookup on the next update()
// Call once per frame. Lookups block up to FLAMINGO_RESOLVE_TIMEOUT_MS, so periodic retries
// only happen when allowLookup is true (idle); refresh() forces one regardless.
void update(uint32_t now, bool networkUp, bool allowLookup);
}  // namespace Flamingo
```

- [ ] **Step 4: Create `src/Flamingo.cpp`**

```cpp
#include "Flamingo.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "Log.h"
#include "config.h"

namespace {

FlamingoLinkConfig makeConfig() {
  FlamingoLinkConfig c;
  c.simonId = FLAMINGO_SIMON_ID;
  c.keepaliveMs = FLAMINGO_KEEPALIVE_MS;
  c.repeats = FLAMINGO_REPEATS;
  return c;
}

FlamingoLink linkInstance(makeConfig());
WiFiUDP udp;
IPAddress flamingoIp;
bool resolved = false;
bool lookupRequested = true;
bool hadNetwork = false;
uint32_t lastLookup = 0;

bool lookup() {
  const String host = FLAMINGO_HOST;
  IPAddress ip;
  if (ip.fromString(host)) {
    flamingoIp = ip;
    return true;
  }
  if (host.endsWith(".local")) {
    ip = MDNS.queryHost(host.substring(0, host.length() - 6), FLAMINGO_RESOLVE_TIMEOUT_MS);
    if (ip == IPAddress()) return false;  // 0.0.0.0 = not found
    flamingoIp = ip;
    return true;
  }
  if (WiFi.hostByName(host.c_str(), ip) != 1) return false;
  flamingoIp = ip;
  return true;
}

}  // namespace

FlamingoLink& Flamingo::link() { return linkInstance; }

void Flamingo::refresh() { lookupRequested = true; }

void Flamingo::update(uint32_t now, bool networkUp, bool allowLookup) {
  uint8_t packet[FlamingoLink::kPacketSize];
  if (!networkUp) {
    if (hadNetwork) {
      resolved = false;
      lookupRequested = true;
    }
    hadNetwork = false;
    while (linkInstance.poll(now, packet)) {
    }  // nowhere to send: drop due packets so repeats/keepalive state stays current
    return;
  }
  hadNetwork = true;

  const bool retryDue = allowLookup && !resolved && now - lastLookup >= FLAMINGO_RESOLVE_RETRY_MS;
  if (lookupRequested || retryDue) {
    lookupRequested = false;
    lastLookup = now;
    resolved = lookup();
    if (resolved) LOG("Flamingo at %s", flamingoIp.toString().c_str());
    else LOG("Flamingo unreachable (%s)", FLAMINGO_HOST);
  }

  if (!linkInstance.poll(now, packet) || !resolved) return;
  udp.beginPacket(flamingoIp, FLAMINGO_PORT);
  udp.write(packet, sizeof(packet));
  udp.endPacket();
}
```

- [ ] **Step 5: Build and test**

Run: `pio run -e esp32dev && pio test -e native`
Expected: `[SUCCESS]`, no warnings from `src/` or `include/`; 27 tests pass. (`Flamingo.cpp` is compiled but not yet called — that is fine.)

- [ ] **Step 6: Commit**

```bash
git add include/config.h src/Flamingo.h src/Flamingo.cpp src/Net.h src/Net.cpp
git commit -m "feat: add Flamingo UDP module and pairing/Ethernet config"
```

---

### Task 3: Pairing gesture + mirroring in the game

**Files:**
- Modify: `src/Buttons.h` (held-state accessors)
- Modify: `src/Animations.h`, `src/Animations.cpp` (pair animations, phase helpers)
- Modify: `src/main.cpp` (replace entirely)

**Interfaces:**
- Consumes: `Flamingo::link()/refresh()/update()`, `Net::connected()` (Task 2); `FlamingoValue::*` (Task 1); config from Task 2.
- Produces:
  ```cpp
  bool Buttons::held(uint8_t i) const; bool Buttons::allHeld() const; bool Buttons::anyHeld() const;
  void Animations::pairHold(float fraction, uint32_t now);
  bool Animations::pairFlash(uint32_t t);      // true when finished (strips dark)
  bool Animations::pairFlashOn(uint32_t t);    // pink currently shown
  bool Animations::gameOverRedOn(uint32_t t);  // red currently shown
  ```

- [ ] **Step 1: Buttons held state** — in `src/Buttons.h` add to the public section after `void clear();`:

```cpp
  bool held(uint8_t i) const { return i < NUM_STRIPS && stable_[i]; }  // debounced, currently down
  bool allHeld() const {
    for (uint8_t i = 0; i < NUM_STRIPS; i++)
      if (!stable_[i]) return false;
    return true;
  }
  bool anyHeld() const {
    for (uint8_t i = 0; i < NUM_STRIPS; i++)
      if (stable_[i]) return true;
    return false;
  }
```

- [ ] **Step 2: Animations** — in `src/Animations.h` add before `void otaProgress(float fraction);`:

```cpp
void pairHold(float fraction, uint32_t now);  // idle with pink rising as the 4-button hold progresses
bool pairFlash(uint32_t t);                   // pink flashes after pairing; true when finished
bool pairFlashOn(uint32_t t);                 // pink is currently shown by pairFlash
bool gameOverRedOn(uint32_t t);               // red is currently shown by gameOver
```

In `src/Animations.cpp` replace the body of `gameOver` so it uses the helper, i.e. replace

```cpp
  if (t < flashEnd) {
    if (blinkOn(t, GAME_OVER_FLASH_MS)) Leds::fillAll(CRGB(GAME_OVER_COLOR));
    return false;
  }
```

with

```cpp
  if (t < flashEnd) {
    if (gameOverRedOn(t)) Leds::fillAll(CRGB(GAME_OVER_COLOR));
    return false;
  }
```

and add before `void otaProgress(float fraction) {`:

```cpp
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
```

(`gameOverRedOn` must be defined before `gameOver` uses it — place this block above `bool gameOver(` if the compiler complains; the header declaration also covers it.)

- [ ] **Step 3: Replace `src/main.cpp`**

```cpp
#include <Arduino.h>
#include <FlamingoLink.h>
#include <SimonGame.h>
#include "Animations.h"
#include "Buttons.h"
#include "Flamingo.h"
#include "Leds.h"
#include "Log.h"
#include "Net.h"
#include "config.h"

enum class State : uint8_t {
  Boot,
  Idle,
  PairHold,
  PairFlash,
  GetReady,
  Showing,
  Input,
  RoundCleared,
  Victory,
  GameOver
};

static const char* const COLOR_NAMES[NUM_STRIPS] = {"red", "green", "blue", "yellow"};
static const int8_t GREEN = 1;

static const char* stateName(State s) {
  switch (s) {
    case State::Boot: return "Boot";
    case State::Idle: return "Idle";
    case State::PairHold: return "PairHold";
    case State::PairFlash: return "PairFlash";
    case State::GetReady: return "GetReady";
    case State::Showing: return "Showing";
    case State::Input: return "Input";
    case State::RoundCleared: return "RoundCleared";
    case State::Victory: return "Victory";
    case State::GameOver: return "GameOver";
  }
  return "?";
}

static SimonConfig makeGameConfig() {
  SimonConfig c;
  c.startLen = START_SEQUENCE_LEN;
  c.maxLen = MAX_SEQUENCE_LEN;
  c.onMsFirst = ON_MS_FIRST;
  c.onMsLast = ON_MS_LAST;
  c.gapMsFirst = GAP_MS_FIRST;
  c.gapMsLast = GAP_MS_LAST;
  c.timeoutMsFirst = TIMEOUT_MS_FIRST;
  c.timeoutMsLast = TIMEOUT_MS_LAST;
  return c;
}

static SimonGame game(makeGameConfig());
static Buttons buttons;
static State state = State::Boot;
static uint32_t stateStart = 0;
static uint32_t lastFrame = 0;
static uint32_t lastPressAt = 0;
static int8_t pulseStrip = -1;  // strip lit by the player's latest press, -1 = none
static uint32_t pulseStart = 0;
static bool startArmed = false;  // a press in Idle starts a game once every button is released

static void enter(State next, uint32_t now) {
  state = next;
  stateStart = now;
  // Presses made during playback/animations must not count as moves.
  if (next == State::Idle || next == State::Input) buttons.clear();
  if (next == State::Input) {
    lastPressAt = now;
    pulseStrip = -1;
  }
  if (next == State::Idle) {
    startArmed = false;
    if (Flamingo::link().paired()) {
      LOG("Flamingo: unpair");
      Flamingo::link().endWithUnpair();
    }
  }
  LOG("-> %s", stateName(next));
}

static void logRound() {
  const SimonTiming tm = game.timing();
  String seq;
  for (uint8_t i = 0; i < game.sequenceLength(); i++) {
    seq += COLOR_NAMES[game.colorAt(i)];
    seq += ' ';
  }
  LOG("Round %u/%u: %s(on %u ms, gap %u ms, timeout %u ms)", game.round(), game.totalRounds(),
      seq.c_str(), tm.onMs, tm.gapMs, tm.timeoutMs);
}

static void startGame(uint32_t now) {
  LOG("Start (%s game)", Flamingo::link().paired() ? "paired" : "normal");
  game.start(esp_random());
  enter(State::GetReady, now);
}

static void onOtaProgress(float fraction) {
  // Called for every network packet; redrawing 400 LEDs each time starves WiFi and breaks uploads.
  static uint32_t lastDraw = 0;
  const uint32_t now = millis();
  if (now - lastDraw < OTA_PROGRESS_FRAME_MS && fraction > 0.0f && fraction < 1.0f) return;
  lastDraw = now;
  Animations::otaProgress(fraction);
  Leds::show();
}

static void onOtaEnd(bool ok) {
  Animations::otaResult(ok);
  Leds::show();
  if (!ok) {
    delay(OTA_ERROR_SHOW_MS);  // show the red result before returning to idle
    enter(State::Idle, millis());
  }
}

static void handlePress(int8_t color, uint32_t now) {
  const uint8_t expected = game.expectedColor();
  const PressResult r = game.press(color);
  pulseStrip = color;
  pulseStart = now;
  lastPressAt = now;
  switch (r) {
    case PressResult::Correct:
      LOG("Press %s: correct", COLOR_NAMES[color]);
      break;
    case PressResult::RoundComplete:
      LOG("Press %s: round %u complete", COLOR_NAMES[color], game.round());
      enter(State::RoundCleared, now);
      break;
    case PressResult::Won:
      LOG("Press %s: WON!", COLOR_NAMES[color]);
      if (Flamingo::link().paired()) {
        LOG("Flamingo: party");
        Flamingo::link().endWithWin();
      }
      enter(State::Victory, now);
      break;
    case PressResult::Wrong:
      LOG("Press %s: wrong, expected %s", COLOR_NAMES[color], COLOR_NAMES[expected]);
      enter(State::GameOver, now);
      break;
  }
}

// Advances the state machine, draws one frame and tells the Flamingo what Simon shows.
static void step(uint32_t now) {
  const uint32_t t = now - stateStart;
  const int8_t press = buttons.pressed();
  uint8_t mirror = FlamingoValue::kDark;

  switch (state) {
    case State::Boot:
      if (Animations::boot(t, now)) enter(State::Idle, now);
      break;

    case State::Idle:
      Animations::idle(now);
      if (buttons.allHeld()) {
        LOG("All buttons held - pairing...");
        enter(State::PairHold, now);
        break;
      }
      if (press >= 0) startArmed = true;
      if (startArmed && !buttons.anyHeld()) startGame(now);
      break;

    case State::PairHold:
      if (!buttons.allHeld()) {
        LOG("Pairing cancelled");
        enter(State::Idle, now);
        break;
      }
      Animations::pairHold((float)t / PAIR_HOLD_MS, now);
      if (t >= PAIR_HOLD_MS) {
        LOG("Paired with Flamingo");
        Flamingo::refresh();
        Flamingo::link().pair();
        enter(State::PairFlash, now);
      }
      break;

    case State::PairFlash: {
      const bool done = Animations::pairFlash(t);
      mirror = Animations::pairFlashOn(t) ? FlamingoValue::kPink : FlamingoValue::kDark;
      if (done && !buttons.anyHeld()) startGame(now);
      break;
    }

    case State::GetReady:
      if (Animations::getReady(t)) {
        logRound();
        enter(State::Showing, now);
      }
      break;

    case State::Showing: {
      if (t < SHOW_LEAD_IN_MS) {
        Leds::clear();
        break;
      }
      const SimonTiming tm = game.timing();
      const uint32_t stepMs = tm.onMs + tm.gapMs;
      const uint32_t elapsed = t - SHOW_LEAD_IN_MS;
      const uint32_t idx = elapsed / stepMs;
      if (idx >= game.sequenceLength()) {
        Leds::clear();
        enter(State::Input, now);
        break;
      }
      const uint32_t within = elapsed % stepMs;
      if (within < tm.onMs) {
        Animations::pulse(game.colorAt(idx), within, tm.onMs);
        mirror = FlamingoValue::forColor(game.colorAt(idx));
      } else {
        Leds::clear();
      }
      break;
    }

    case State::Input:
      if (pulseStrip >= 0 && Animations::pulse(pulseStrip, now - pulseStart, PRESS_PULSE_MS))
        pulseStrip = -1;
      if (pulseStrip < 0) Leds::clear();
      mirror = FlamingoValue::forColor(pulseStrip);
      if (press >= 0) {
        handlePress(press, now);
      } else if (now - lastPressAt > game.timing().timeoutMs) {
        LOG("Timeout, expected %s", COLOR_NAMES[game.expectedColor()]);
        enter(State::GameOver, now);
      }
      break;

    case State::RoundCleared:
      // Finish the last press's pulse, then the green "cleared" pulse.
      if (t < PRESS_PULSE_MS) {
        Animations::pulse(pulseStrip, t, PRESS_PULSE_MS);
        mirror = FlamingoValue::forColor(pulseStrip);
      } else if (Animations::roundCleared(t - PRESS_PULSE_MS)) {
        game.nextRound();
        logRound();
        enter(State::Showing, now);
      } else {
        mirror = FlamingoValue::forColor(GREEN);
      }
      break;

    case State::Victory:
      if (t < PRESS_PULSE_MS) {
        Animations::pulse(pulseStrip, t, PRESS_PULSE_MS);
      } else if (Animations::victory(t - PRESS_PULSE_MS, now)) {
        enter(State::Idle, now);
      }
      break;

    case State::GameOver:
      mirror = Animations::gameOverRedOn(t) ? FlamingoValue::forColor(0) : FlamingoValue::kDark;
      if (Animations::gameOver(t, game.expectedColor())) enter(State::Idle, now);
      break;
  }

  Flamingo::link().setValue(mirror);
  Flamingo::update(now, Net::connected(), state == State::Idle);
  Leds::show();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  LOG("Simon booting");
  Leds::begin();
  buttons.begin();
  Net::begin(onOtaProgress, onOtaEnd);
  enter(State::Boot, millis());
}

void loop() {
  const uint32_t now = millis();
  Net::update(now);
  buttons.update(now);
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    step(now);
  }
}
```

- [ ] **Step 4: Build and test**

Run: `pio run -e esp32dev && pio test -e native`
Expected: `[SUCCESS]` with no warnings from `src/`/`include/`; 27 tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/Buttons.h src/Animations.h src/Animations.cpp src/main.cpp
git commit -m "feat: pair with the Flamingo by holding all 4 buttons; mirror game colours"
```

---

### Task 4: Ethernet first, WiFi fallback

**Files:**
- Modify: `src/Net.cpp` (replace entirely)

**Interfaces:**
- Consumes: config `ETH_ENABLED`, `ETH01_*`, `HOSTNAME`, `WIFI_RETRY_MS`; secrets defines.
- Produces: same `Net::begin/update/connected` signatures; `connected()` now true on Ethernet or WiFi.

- [ ] **Step 1: Replace `src/Net.cpp`**

```cpp
#include "Net.h"
#include <ArduinoOTA.h>
#include <ETH.h>
#include <WiFi.h>
#include "Log.h"
#include "config.h"

#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD) || !defined(OTA_PASSWORD)
#error "WiFi/OTA secrets missing: copy secrets.example.ini to secrets.ini and fill it in"
#endif

namespace {

bool wifiEnabled = false;
bool otaStarted = false;
volatile bool ethUp = false;  // written from the network event task
bool ethWasUp = false;
uint32_t lastRetry = 0;
Net::ProgressFn progressFn = nullptr;
Net::EndFn endFn = nullptr;

void onEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname(HOSTNAME);
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethUp = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
      ethUp = false;
      break;
    default:
      break;
  }
}

void startWifi() {
  WiFi.setHostname(HOSTNAME);  // must precede WiFi.mode() on core 2.x
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // modem sleep adds latency that makes OTA uploads drop
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  LOG("WiFi connecting to %s", WIFI_SSID);
}

void startOta() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    LOG("OTA start");
    progressFn(0);
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    progressFn(total ? (float)done / total : 0);
  });
  ArduinoOTA.onEnd([]() {
    LOG("OTA done, rebooting");
    endFn(true);
  });
  ArduinoOTA.onError([](ota_error_t err) {
    LOG("OTA error %u", (unsigned)err);
    endFn(false);
  });
  ArduinoOTA.begin();
  otaStarted = true;
  LOG("OTA ready at %s.local (%s via %s)", HOSTNAME,
      ethUp ? ETH.localIP().toString().c_str() : WiFi.localIP().toString().c_str(),
      ethUp ? "Ethernet" : "WiFi");
}

}  // namespace

void Net::begin(ProgressFn onProgress, EndFn onEnd) {
  progressFn = onProgress;
  endFn = onEnd;
  WiFi.onEvent(onEvent);
  if (ETH_ENABLED) {
    // Same bring-up as the Flamingo on this board: power the PHY, then start the MAC.
    pinMode(ETH01_PHY_POWER_PIN, OUTPUT);
    digitalWrite(ETH01_PHY_POWER_PIN, HIGH);
    if (ETH.begin(ETH01_PHY_ADDR, ETH01_PHY_POWER_PIN, ETH01_MDC_PIN, ETH01_MDIO_PIN,
                  ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN)) {
      LOG("Ethernet starting");
    } else {
      LOG("Ethernet init failed");
    }
  }
  wifiEnabled = strlen(WIFI_SSID) > 0;
  if (wifiEnabled) startWifi();
  else LOG("WiFi disabled (wifi_ssid empty in secrets.ini)");
}

bool Net::connected() { return ethUp || WiFi.status() == WL_CONNECTED; }

void Net::update(uint32_t now) {
  const bool eth = ethUp;
  if (eth != ethWasUp) {
    ethWasUp = eth;
    if (eth) {
      LOG("Ethernet up %s, WiFi off", ETH.localIP().toString().c_str());
      if (WiFi.getMode() != WIFI_OFF) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
      }
    } else {
      LOG("Ethernet down%s", wifiEnabled ? ", WiFi fallback" : "");
      if (wifiEnabled) startWifi();
    }
  }

  if (!connected()) {
    if (wifiEnabled && !eth && now - lastRetry >= WIFI_RETRY_MS) {
      lastRetry = now;
      LOG("WiFi not connected, retrying");
      WiFi.reconnect();
    }
    return;
  }
  if (!otaStarted) startOta();
  ArduinoOTA.handle();
}
```

- [ ] **Step 2: Build all envs and test**

Run: `pio run -e esp32dev && pio run -e ota && pio test -e native`
Expected: both builds `[SUCCESS]` with no warnings from `src/`/`include/`; 27 tests pass.

- [ ] **Step 3: Pin-conflict check**

Run: `grep -nE 'BUTTON_PIN|LED_PIN|ETH01_' include/config.h`
Expected: buttons 12/15/2/5, strips 4/14/32/33, Ethernet 16/23/18 — no number appears twice.

- [ ] **Step 4: Commit**

```bash
git add src/Net.cpp
git commit -m "feat: Ethernet first with WiFi fallback"
```

---

### Task 5: Flamingo firmware — Simon mode

**Files (other repo):**
- Modify: `~/gits/Flamingods/esps/flamingo/src/main.cpp`

**Interfaces:**
- Consumes: Simon packets `[5, value]` per Global Constraints.
- Produces: `PLAN_SIMON = 11`, `handleSimon(uint8_t)`, `checkSimonTimeout()`, `playSimonPattern()`.

- [ ] **Step 1: Branch**

```bash
cd ~/gits/Flamingods && git checkout -b simon-pairing && git status --short
```
Expected: on `simon-pairing`; the only pre-existing change listed is `esps/test/platformio.ini` (leave it unstaged).

- [ ] **Step 2: Apply the edits** — run from `~/gits/Flamingods/esps/flamingo` (each replacement asserts its anchor exists exactly once):

```bash
python3 - <<'EOF'
p = 'src/main.cpp'
s = open(p).read()

def sub(old, new):
    global s
    assert s.count(old) == 1, ('anchor not unique/missing', old)
    s = s.replace(old, new)

sub('#define MAX_STATIONS 4\n', '''#define MAX_STATIONS 4

// Simon game (station ID 5). Protocol: Simon repo docs/superpowers/specs/2026-10-03-flamingo-pairing-design.md
#define SIMON_ID 5
#define SIMON_TIMEOUT_MS 2000   // unpair if Simon goes silent this long
#define SIMON_PINK 0x20
#define SIMON_WIN 0xFE
#define SIMON_UNPAIR 0xFF
unsigned long simonLastPacket = 0;
CRGB simonColor = CRGB::Black;
''')

sub('    PLAN_STATION_4_SPECIAL = 10  // Station 4: Electric Pulse (8 seconds)\n',
    '    PLAN_STATION_4_SPECIAL = 10, // Station 4: Electric Pulse (8 seconds)\n'
    '    PLAN_SIMON = 11              // Simon game paired: shows Simon colours, stations ignored\n')

sub('void handleUDP();\n', 'void handleUDP();\nvoid handleSimon(uint8_t value);\nvoid checkSimonTimeout();\nvoid playSimonPattern();\n')

sub('''        case PLAN_MIXED_COLORS:
            playMixedColorsPattern();
            break;
''', '''        case PLAN_MIXED_COLORS:
            playMixedColorsPattern();
            break;
        case PLAN_SIMON:
            playSimonPattern();
            break;
''')

sub('''    int packetSize = udp.parsePacket();
    if (packetSize >= 2) {''', '''    int packetSize;
    while ((packetSize = udp.parsePacket()) > 0) {  // drain every waiting packet
    if (packetSize >= 2) {''')

sub('''        uint8_t buttonMask = buffer[1];
''', '''        uint8_t buttonMask = buffer[1];

        if (stationId == SIMON_ID) {
            handleSimon(buttonMask);
            continue;
        }
''')

sub('''            // Check if all 5 buttons pressed
''', '''            // While Simon is paired, stations are recorded but don't change the plan.
            if (currentPlan == PLAN_SIMON) continue;

            // Check if all 5 buttons pressed
''')

sub('''    // Check for inactive stations and return to IDLE if all inactive
    checkStationTimeouts();''', '''    }  // while: drain packets

    checkSimonTimeout();

    // Check for inactive stations and return to IDLE if all inactive
    checkStationTimeouts();''')

s += '''
/**
 * Simon game (station ID 5): paired while packets keep arriving.
 * 0x00 black, 0x01/0x02/0x04/0x08 red/green/blue/yellow, 0x20 pink,
 * 0xFE win (PARTY, then IDLE), 0xFF unpair (IDLE).
 */
static bool simonValueToColor(uint8_t value, CRGB& out) {
    switch (value) {
        case 0x00: out = CRGB::Black; return true;
        case 0x01: out = CRGB(255, 0, 0); return true;
        case 0x02: out = CRGB(0, 255, 0); return true;
        case 0x04: out = CRGB(0, 0, 255); return true;
        case 0x08: out = CRGB(180, 180, 0); return true;  // same yellow as stations
        case SIMON_PINK: out = CRGB(255, 20, 147); return true;
        default: return false;
    }
}

void handleSimon(uint8_t value) {
    unsigned long now = millis();
    if (value == SIMON_UNPAIR) {
        if (currentPlan == PLAN_SIMON) {
            Serial.println("Simon: unpaired - returning to IDLE");
            currentPlan = PLAN_IDLE;
        }
        return;
    }
    if (value == SIMON_WIN) {
        if (currentPlan == PLAN_SIMON) {
            Serial.println("Simon: won - PARTY");
            previousPlan = PLAN_IDLE;
            partyModeStartTime = now;
            currentPlan = PLAN_PARTY;
        }
        return;
    }
    CRGB color;
    if (!simonValueToColor(value, color)) return;
    if (currentPlan != PLAN_SIMON) Serial.println("Simon: paired");
    currentPlan = PLAN_SIMON;
    simonColor = color;
    simonLastPacket = now;
}

void checkSimonTimeout() {
    if (currentPlan == PLAN_SIMON && millis() - simonLastPacket > SIMON_TIMEOUT_MS) {
        Serial.println("Simon: timed out - returning to IDLE");
        currentPlan = PLAN_IDLE;
    }
}

void playSimonPattern() {
    // Instant (no easing) so short sequence steps stay readable.
    fill_solid(leds_strip_1, NUM_LEDS_PER_STRIP, simonColor);
    fill_solid(leds_strip_2, NUM_LEDS_PER_STRIP, simonColor);
    fill_solid(leds_strip_3, NUM_LEDS_PER_STRIP, simonColor);
    FastLED.show();
}
'''
open(p, 'w').write(s)
print('ok')
EOF
```
Expected: `ok`.

- [ ] **Step 3: Build**

Run: `cd ~/gits/Flamingods/esps/flamingo && pio run -e esp32dev`
Expected: `[SUCCESS]`; no new warnings in `src/main.cpp`. (If `CRGB` is unknown at the inserted globals, move the inserted `unsigned long simonLastPacket…`/`CRGB simonColor…` lines below the `#include <FastLED.h>` — the build output names the line.)

- [ ] **Step 4: Commit (Flamingo repo, only this file)**

```bash
cd ~/gits/Flamingods && git add esps/flamingo/src/main.cpp && git commit -m "feat(flamingo): Simon pairing mode (station ID 5)"
```
Expected: commit contains only `esps/flamingo/src/main.cpp`. Do not push or flash; the user decides.

---

### Task 6: README

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: config names from Tasks 2–4.

- [ ] **Step 1: Add sections** — in `README.md`, insert before `## Tuning`:

```markdown
## Flamingo pairing

Hold **all 4 buttons for 2 seconds** while Simon is idle. The strips fill with pink as you hold; at 2 s Simon and the Flamingo flash pink together. Release the buttons and the game starts — the Flamingo's whole body now mirrors Simon's colours.

- A **wrong press / timeout** ends the game with 4 red flashes (on the Flamingo too) and unpairs: the Flamingo goes back to its rainbow and the stations work again.
- A **win** makes the Flamingo party for 10 s, then it unpairs.
- While paired, the stations are ignored. If Simon loses power, the Flamingo unpairs by itself after 2 s.
- Simon talks to `flamingo-esp32.local` on UDP 5000 as station ID 5 (`FLAMINGO_*` in `config.h`). The Flamingo needs its `simon-pairing` firmware (Flamingods repo). If the Flamingo is off or unreachable, Simon still plays normally.
- A normal game starts when you **release** a button.

## Network

Simon prefers **Ethernet** (plug a cable into the board): WiFi turns off while the cable is up and comes back automatically if it's unplugged. OTA uploads, `simon.local` and the Flamingo link work on either. Disable Ethernet with `ETH_ENABLED = false`.
```

Append to the `## First power-on checklist` list:

```markdown
- [ ] Pairing: hold all 4 buttons 2 s → pink fill, then Simon and Flamingo flash pink; the game shows on the Flamingo in sync.
- [ ] Holding only 3 buttons, or letting go before 2 s, does nothing (no pairing, no game).
- [ ] Wrong press while paired → red flashes on both, then the Flamingo returns to its rainbow; stations work again.
- [ ] Winning while paired → Flamingo party, then rainbow.
- [ ] Press a station during a paired game → the Flamingo keeps showing Simon only.
- [ ] Power Simon off mid-game while paired → Flamingo returns to rainbow within ~2 s.
- [ ] Flamingo switched off → pairing still flashes pink on Simon and the game plays normally without stutter.
- [ ] Ethernet cable in → serial `Ethernet up …, WiFi off`; pull it → `Ethernet down, WiFi fallback`; OTA works both ways.
```

- [ ] **Step 2: Final verification**

Run: `pio test -e native && pio run -e esp32dev && pio run -e ota && git status --short`
Expected: 27 tests pass, both builds `[SUCCESS]`, clean tree after commit.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: Flamingo pairing, Ethernet and checklist"
```
