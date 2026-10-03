# Simon (ESP32)

Simon on an ESP32 with 4 buttons and 4 WS2812B LED strips. Flash once over USB, then over WiFi forever after.

## Wiring

| Color  | Button GPIO (other leg → GND) | Strip data GPIO |
|--------|-------------------------------|-----------------|
| Red    | IO12 | IO4 |
| Green  | IO15 | IO14 |
| Blue   | IO2 | IO32 (labeled **CFG**) |
| Yellow | IO5 (labeled **RXD2**) | IO33 (labeled **485_EN**) |

Board: WT32-ETH01. Buttons need no resistors (internal pull-ups). Strips: 5 V and GND from a 5 V supply; tie supply GND to ESP32 GND. If a strip flickers, add a 330 Ω resistor in series on its data line.

Leave IO0, TXD and RXD free for flashing (FTDI TX → RXD, FTDI RX → TXD, GND → GND; IO0 → GND during the first flash).

## Setup

1. Install PlatformIO (`pip install platformio` or the VS Code extension).
2. `cp secrets.example.ini secrets.ini` and fill in your WiFi and an OTA password. (`secrets.ini` is git-ignored.) Leave `wifi_ssid` empty to run without WiFi.

## Flashing

- **First time (USB):** `pio run -e esp32dev -t upload`
- **After that (WiFi):** `pio run -e ota -t upload`
  - If `simon.local` doesn't resolve: `pio run -e ota -t upload --upload-port <device IP>` (the IP is printed on serial at boot).
  - While uploading, all strips fill cyan like a progress bar; green = done (it reboots), red = failed.
- **Serial log:** `pio device monitor` — shows every state change, round, press and timeout.
- **Unit tests (game rules, on your computer):** `pio test -e native`

## How to play

Boot animation → idle glow. Press any button to start. Watch the sequence, repeat it. Round 1 is 3 colors; each round adds one and plays faster, up to 10. A wrong press or taking too long ends the game (red flash, then the button you should have pressed blinks). Clear round 8 for the victory show.

## Tuning

Everything is in **`include/config.h`** — edit, then `pio run -e ota -t upload` (~30 s).

| Want to change… | Edit |
|---|---|
| Overall brightness / power draw | `MAX_BRIGHTNESS`, `POWER_LIMIT_MA` |
| Strip colors (e.g. yellow looks green) | `STRIP_COLORS` |
| Red/green swapped on a strip | `COLOR_ORDER` (`GRB` ↔ `RGB`) |
| Number of LEDs | `LEDS_PER_STRIP` |
| Rounds / sequence length | `START_SEQUENCE_LEN`, `MAX_SEQUENCE_LEN` |
| Game speed | `ON_MS_FIRST/LAST`, `GAP_MS_FIRST/LAST` |
| Time allowed per press | `TIMEOUT_MS_FIRST/LAST` |
| Press feedback length | `PRESS_PULSE_MS` |
| Double presses / missed presses | `DEBOUNCE_MS` |
| Idle look | `IDLE_*` |
| Boot / victory / game-over animations | `BOOT_*`, `VICTORY_*`, `GAME_OVER_*` |
| Round-cleared pulse | `ROUND_CLEARED_*` |
| Pause / flourish before playback | `SHOW_LEAD_IN_MS`, `GET_READY_MS` |

## First power-on checklist

- [ ] Boot: each strip fills LED by LED in red, green, blue, yellow; then all go white — every LED lit?
- [ ] Idle: four strips gently breathe in their colors, glow rotating around, occasional sparkles.
- [ ] Each button lights its own strip (serial shows the right color name).
- [ ] Full game: 3 colors first, +1 each round, visibly faster; round 8 → victory animation → idle.
- [ ] Wrong press → red flashes, then the correct strip blinks 3 times → idle.
- [ ] Wait without pressing → timeout game over (5 s in round 1).
- [ ] Mash buttons during playback, the green round-cleared pulse and game-over: none of them count; input always starts fresh.
- [ ] Press two buttons at the same instant during input: serial shows two `Press …` lines (lowest color first).
- [ ] Router off (or `wifi_ssid` empty): device still boots straight into the animation and plays; serial shows WiFi retry messages.
- [ ] `pio run -e ota -t upload` works with the USB cable unplugged.
