# ESP32 Simon Game — Design

Date: 2026-10-03
Reference: `~/Downloads/smn/simon_game` (blocking Arduino sketch; hardware and pinout reused, code rewritten)

## Goal

A physical Simon game on an ESP32 with 4 buttons and 4 addressable LED strips, flashable over WiFi (OTA). It boots with a hardware-check animation, idles with a pleasant per-strip animation, plays an 8-round game (3 → 10 colors, faster each round), and celebrates a win with a victory animation.

Success criteria:
- Game is fully playable with or without WiFi.
- Firmware can be re-flashed over WiFi with `pio run -t upload` (USB only for the first flash).
- Every tunable value (timing, colors, brightness, pins, animation speeds) lives in one file, `include/config.h`, so it can be tuned during physical testing.

## Hardware

Pinout updated 2026-10-03 for the actual board (ESP32 Ethernet module, likely WT32-ETH01, where the reference pins 13/18/27 are unavailable).

| Function        | GPIO |
|-----------------|------|
| Red button      | 39   |
| Green button    | 36   |
| Blue button     | 15   |
| Yellow button   | 35   |
| Red strip data  | 2    |
| Green strip data| 4    |
| Blue strip data | 12   |
| Yellow strip data | 14 |

- Board: ESP32 DevKit (`esp32dev`).
- Strips: 5V WS2812B, 8 LEDs each, GRB order.
- Buttons wired GPIO → button → GND (pressed = LOW). GPIO 35/36/39 have no internal pull-up: external 10 kΩ to 3.3 V required; GPIO 15 uses the internal pull-up.

## Toolchain

- PlatformIO, Arduino framework, FastLED.
- Environments in `platformio.ini`:
  - `esp32dev` — USB serial upload (first flash).
  - `ota` — extends `esp32dev`, `upload_protocol = espota`, `upload_port = simon.local`, OTA password passed via `upload_flags` from `secrets.ini` (`extra_configs`).
  - `native` — host build for unit tests of pure game logic.

## Architecture

Non-blocking main loop. `loop()` never calls `delay()` for more than a few ms; every iteration it:
1. Services `Net` (WiFi reconnect + `ArduinoOTA.handle()`).
2. Polls `Buttons` for debounced press edges.
3. Advances the state machine based on `millis()` and events.
4. Renders the current animation frame into the LED buffers and calls `FastLED.show()` (~60 fps cap).

### Files

```
platformio.ini
include/config.h          # ALL tunables, grouped and commented
secrets.example.ini       # template; copy to secrets.ini
secrets.ini               # git-ignored: wifi_ssid, wifi_password, ota_password
                          # (fed to firmware as build flags AND to espota upload auth)
src/Log.h                 # LOG(...) macro
lib/SimonGame/            # pure C++ game logic, no Arduino deps
  SimonGame.h / .cpp
src/main.cpp              # state machine wiring
src/Buttons.h / .cpp      # debounced edge detection
src/Leds.h / .cpp         # strip buffers, FastLED setup, helpers
src/Animations.h / .cpp   # frame-based effects
src/Net.h / .cpp          # WiFi + ArduinoOTA + mDNS
test/test_simon/test_main.cpp  # native unit tests
.gitignore
README.md                 # wiring, first flash, OTA flash, tuning guide
```

### Units

**`SimonGame`** (pure logic, unit-tested)
- `void start(uint32_t seed)` — generates a full sequence of `MAX_SEQUENCE_LEN` (10) random colors 0–3; round = 1.
- `uint8_t round() const`, `uint8_t totalRounds() const` (= MAX − START + 1 = 8).
- `uint8_t sequenceLength() const` — `START_SEQUENCE_LEN + round − 1`.
- `uint8_t colorAt(uint8_t i) const`.
- `PressResult press(uint8_t color)` → `Correct`, `RoundComplete`, `Won`, `Wrong`. Tracks input position internally.
- `void nextRound()` — advances round, resets input position.
- `uint8_t expectedColor() const` — used by the game-over animation.
- `Timing timing() const` — `{onMs, gapMs, timeoutMs}` linearly interpolated from round 1 to final round using config values.
- Takes its constants via a small config struct (filled from `config.h`) so tests can use custom values.

**`Buttons`**
- `begin()`, `update(now)`; `int8_t pressed()` returns the color index of a new debounced press since last call, or −1.
- Debounce time from config.

**`Leds`**
- Owns 4 `CRGB` arrays, registers them with FastLED, sets brightness and power limit (`FastLED.setMaxPowerInVoltsAndMilliamps`).
- Helpers: `fillStrip(i, color)`, `clear()`, `show()`, access to raw arrays.

**`Animations`**
- Each effect is a function `render(now, startedAt, params...)` that writes into the LED buffers; returns `true` when finished (for finite effects).
- Effects: `boot`, `idle`, `pulse(strip)` (sequence step / button feedback), `roundCleared`, `gameOver(correctStrip)`, `victory`, `otaProgress(fraction)`, `otaResult(ok)`.

**`Net`**
- `begin()` — starts WiFi in STA mode non-blocking, sets hostname `simon`, starts mDNS and ArduinoOTA with password.
- `update()` — calls `ArduinoOTA.handle()`; retries WiFi periodically if disconnected.
- OTA callbacks set flags so `main` renders the OTA progress animation (rendered directly from the progress callback, since the main loop is suspended during upload).
- Never blocks waiting for WiFi.

## State Machine

```
BOOT ──(boot anim done)──▶ IDLE ──(any press)──▶ GET_READY ──(600ms)──▶ SHOWING
SHOWING ──(sequence done)──▶ INPUT
INPUT ──Correct──▶ INPUT (pulse pressed strip, reset timeout)
INPUT ──RoundComplete──▶ ROUND_CLEARED ──(anim done)──▶ SHOWING (next round)
INPUT ──Won──▶ VICTORY ──(anim done)──▶ IDLE
INPUT ──Wrong / timeout──▶ GAME_OVER ──(anim done)──▶ IDLE
any ──(OTA start)──▶ OTA (progress anim; device reboots on success)
```

- The press that starts the game is not counted as a move.
- Buttons are ignored during BOOT, GET_READY, SHOWING, ROUND_CLEARED, VICTORY, GAME_OVER.
- Random seed: `esp_random()`.

## Game Rules & Difficulty

- One random 10-color sequence per game; round 1 shows the first 3, each round appends the next → 8 rounds (lengths 3..10).
- Per-round timing, linear from round 1 to round 8 (defaults):

| Parameter               | Round 1 | Round 8 |
|-------------------------|---------|---------|
| Light on time           | 600 ms  | 250 ms  |
| Gap between steps       | 250 ms  | 100 ms  |
| Input timeout per press | 5000 ms | 3000 ms |

- Input feedback pulse length: config value (default 250 ms), independent of round.
- Wrong press or timeout → game over.

## Animations (defaults; all durations/speeds in `config.h`)

- **Boot (~3 s):** sweep lights up each strip LED-by-LED in its color, strips in order R→G→B→Y; then all strips full white briefly (reveals dead pixels); fade into idle.
- **Idle:** each strip breathes in its own color with a slow sine; strips phase-offset so the glow rotates around the four; a gentle moving shimmer along each strip's LEDs and occasional sparkles. Low brightness ceiling (`IDLE_MAX_BRIGHTNESS`).
- **Pulse (sequence step / button feedback):** strip snaps to full color, fades out over the last 30% of its duration.
- **Round cleared:** all strips one quick green pulse (~400 ms).
- **Game over:** all strips flash red ×2, then the expected strip blinks its color ×3.
- **Victory (~6 s):** fast chase around the 4 strips → rainbow confetti/sparkle across all strips → 3 white flashes → fade to idle.
- **OTA:** all strips fill as a progress bar (cyan); green flash on success, red on error.

## `config.h` Contents (grouped)

- Pins: button pins, LED data pins.
- LEDs: `LEDS_PER_STRIP`, `COLOR_ORDER`, `MAX_BRIGHTNESS`, `POWER_LIMIT_MA` (default 1500 at 5 V), strip colors (`STRIP_COLORS[4]`).
- Game: `START_SEQUENCE_LEN` (3), `MAX_SEQUENCE_LEN` (10), on/gap/timeout for first & last round, `PRESS_PULSE_MS`, `GET_READY_MS`.
- Buttons: `DEBOUNCE_MS`.
- Animations: durations and speeds for each effect, `IDLE_MAX_BRIGHTNESS`, sparkle chance, etc.
- Network: `HOSTNAME` ("simon"), WiFi retry interval.
- Debug: `LOG_ENABLED`, serial baud 115200.

Changing behavior during tuning should require editing only `config.h`.

## Error Handling

- No WiFi / wrong credentials: game runs normally; WiFi retries in background; logged to serial.
- OTA failure: red flash, then return to IDLE.
- `secrets.ini` missing: PlatformIO reports the missing `[secrets]` values; README points to `secrets.example.ini`.
- Empty `wifi_ssid`: WiFi/OTA disabled, game runs, logged.

## Logging

Serial at 115200: boot info (IP, hostname), every state transition, every button press, round number, press results, OTA events.

## Testing

- **Native unit tests** (`pio test -e native`) for `SimonGame`: sequence lengths per round, 8 total rounds, Correct/RoundComplete/Won/Wrong results, expected color reporting, restart resets state, timing interpolation at first/middle/last round.
- **Build check:** `pio run -e esp32dev` compiles cleanly.
- **On hardware (by user):** animations' look, timing feel, button responsiveness, OTA upload — tuned via `config.h`.

## Out of Scope

- Sound/buzzer, high-score persistence, web UI, WiFi provisioning portal, difficulty selection.
